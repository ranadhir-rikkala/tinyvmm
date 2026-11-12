#include "vmm/linux_boot.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "boot/cmdline.h"
#include "boot/kernel_image.h"
#include "boot/linux_loader.h"
#include "common/file.h"
#include "memory/guest_memory.h"
#include "x86/layout.h"

#if defined(__x86_64__)
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <csignal>

#include "devices/uart8250.h"
#include "kvm/kvm.h"
#include "kvm/x86_cpu.h"
#include "trace/marker_scanner.h"
#include "vmm/exit_stats.h"
#endif

namespace tvmm {

namespace {

void PrintLayout(const KernelImage& kernel, const BootLayout& layout, const Cmdline& cmdline, std::uint64_t mem) {
  auto ull = [](std::uint64_t v) { return static_cast<unsigned long long>(v); };
  std::fprintf(stderr, "guest memory : %llu MiB\n", ull(mem >> 20));
  std::fprintf(stderr, "kernel       : %s, entry 0x%llx, image [0x%llx, 0x%llx)\n",
               std::string(KernelFormatName(kernel.format)).c_str(), ull(layout.entry), ull(layout.kernel_start),
               ull(layout.kernel_end));
  if (kernel.format == KernelFormat::kBzImage) {
    std::fprintf(stderr, "boot proto   : %u.%02u\n", kernel.boot_protocol >> 8, kernel.boot_protocol & 0xff);
  }
  std::fprintf(stderr, "zero page    : 0x%llx\n", ull(layout.zero_page));
  std::fprintf(stderr, "cmdline      : 0x%llx (%llu bytes) \"%s\"\n", ull(layout.cmdline_addr), ull(layout.cmdline_len),
               cmdline.str().c_str());
  if (layout.initrd_size != 0) {
    std::fprintf(stderr, "initrd       : 0x%llx (%llu bytes)\n", ull(layout.initrd_addr), ull(layout.initrd_size));
  }
  std::fprintf(stderr, "gdt/idt      : 0x%llx / 0x%llx\n", ull(layout.tables.gdt_addr), ull(layout.tables.idt_addr));
  std::fprintf(stderr, "page tables  : pml4 0x%llx, %llu KiB, 2 MiB identity pages\n", ull(layout.tables.pml4),
               ull(layout.tables.page_table_bytes >> 10));
  for (const auto& e : layout.e820) {
    std::fprintf(stderr, "e820         : [0x%09llx, 0x%09llx) %s\n", ull(e.addr), ull(e.end()),
                 e.type == x86::E820Type::kRam ? "ram" : "reserved");
  }
}

#if defined(__x86_64__)

std::atomic<kvm_run*> g_run{nullptr};
volatile std::sig_atomic_t g_timed_out = 0;

void OnAlarm(int) {
  g_timed_out = 1;
  if (kvm_run* run = g_run.load()) run->immediate_exit = 1;
}

enum class StopReason { kMarker, kReset, kShutdown, kTimeout, kHalt };

const char* StopReasonName(StopReason reason) {
  switch (reason) {
    case StopReason::kMarker:
      return "init marker seen";
    case StopReason::kReset:
      return "guest requested reset via i8042 (reboot=k)";
    case StopReason::kShutdown:
      return "KVM_EXIT_SHUTDOWN (triple fault or system event)";
    case StopReason::kTimeout:
      return "timeout";
    case StopReason::kHalt:
      return "KVM_EXIT_HLT";
  }
  return "unknown";
}

Result<int> RunLinux(const Kvm& kvm, Vm& vm, Vcpu& vcpu, const BootLayout& layout, const BootOptions& options,
                     BootTrace& trace) {
  if (auto s = x86::ConfigureCpuid(kvm, vcpu, 0); !s) return Propagate(s.error());
  if (auto s = x86::SetupLongMode(vcpu, layout.tables, layout.entry, layout.zero_page); !s) {
    return Propagate(s.error());
  }
  if (auto s = x86::SetupFpu(vcpu); !s) return Propagate(s.error());
  if (auto s = x86::SetupLapicLint(vcpu); !s) return Propagate(s.error());

  MarkerScanner scanner(options.init_marker);
  bool marker_seen = false;
  Uart8250 uart([&](std::uint8_t b) {
    trace.Mark(BootPhase::kFirstSerialByte);
    if (options.echo_serial) {
      std::fputc(b, stdout);
      if (b == '\n') std::fflush(stdout);
    }
    if (scanner.Feed(static_cast<char>(b))) {
      trace.Mark(BootPhase::kInitMarker);
      marker_seen = true;
    }
  });

  kvm_run* run = vcpu.run_state();
  g_run.store(run);
  if (options.timeout_sec != 0) {
    struct sigaction sa {};
    sa.sa_handler = OnAlarm;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGALRM, &sa, nullptr);
    ::alarm(options.timeout_sec);
  }

  ExitStats stats;
  std::optional<StopReason> stop;
  trace.Mark(BootPhase::kFirstVcpuEntry);

  while (!stop) {
    if (auto s = vcpu.Run(); !s) {
      if (s.error().code == EINTR || s.error().code == EAGAIN) {
        ++stats.interrupted;
        if (g_timed_out) stop = StopReason::kTimeout;
        continue;
      }
      return Propagate(s.error());
    }
    ++stats.total;

    switch (run->exit_reason) {
      case KVM_EXIT_IO: {
        ++stats.io;
        ++stats.io_by_port[run->io.port];
        const std::uint16_t port = run->io.port;
        const bool out = run->io.direction == KVM_EXIT_IO_OUT;
        auto* data = reinterpret_cast<std::uint8_t*>(run) + run->io.data_offset;
        for (std::uint32_t i = 0; i < run->io.count; ++i, data += run->io.size) {
          if (port >= x86::kSerialPortBase && port < x86::kSerialPortBase + x86::kSerialPortCount &&
              run->io.size == 1) {
            const auto reg = static_cast<std::uint8_t>(port - x86::kSerialPortBase);
            if (out) {
              uart.Write(reg, *data);
            } else {
              *data = uart.Read(reg);
            }
            if (uart.TakeInterruptEdge()) {
              if (auto s = vm.IrqLine(x86::kSerialIrq, true); !s) return Propagate(s.error());
              if (auto s = vm.IrqLine(x86::kSerialIrq, false); !s) return Propagate(s.error());
            }
          } else if (port == x86::kI8042CommandPort && out && *data == x86::kI8042ResetCommand) {
            stop = StopReason::kReset;
          } else if (!out) {
            std::fill(data, data + run->io.size, std::uint8_t{0xff});
          }
        }
        break;
      }
      case KVM_EXIT_MMIO:
        ++stats.mmio;
        if (!run->mmio.is_write) std::fill(std::begin(run->mmio.data), std::end(run->mmio.data), std::uint8_t{0xff});
        break;
      case KVM_EXIT_HLT:
        stop = StopReason::kHalt;
        break;
      case KVM_EXIT_SHUTDOWN:
      case KVM_EXIT_SYSTEM_EVENT:
        stop = StopReason::kShutdown;
        break;
      case KVM_EXIT_INTR:
        ++stats.interrupted;
        break;
      case KVM_EXIT_FAIL_ENTRY:
        return Fail("KVM_EXIT_FAIL_ENTRY, hardware reason " +
                    std::to_string(run->fail_entry.hardware_entry_failure_reason));
      case KVM_EXIT_INTERNAL_ERROR:
        return Fail("KVM_EXIT_INTERNAL_ERROR, suberror " + std::to_string(run->internal.suberror));
      default:
        return Fail("unhandled KVM exit reason " + std::to_string(run->exit_reason));
    }
    if (marker_seen && options.exit_on_marker) stop = StopReason::kMarker;
  }

  ::alarm(0);
  g_run.store(nullptr);
  std::fflush(stdout);
  std::fprintf(stderr, "\nstop reason  : %s\n", StopReasonName(*stop));
  std::fprintf(stderr, "%s", stats.Format().c_str());
  if (!marker_seen) std::fprintf(stderr, "init marker \"%s\" was not seen\n", options.init_marker.c_str());
  return marker_seen ? 0 : 1;
}

#endif

}  // namespace

Result<int> BootLinux(const BootOptions& options, BootTrace& trace) {
  if (options.mem_mib < 2 || options.mem_mib > (x86::kMmioGapStart >> 20)) {
    return Fail("--mem must be between 2 and " + std::to_string(x86::kMmioGapStart >> 20) + " MiB");
  }
#if defined(__x86_64__)
  std::optional<Kvm> kvm;
  std::optional<Vm> vm;
  std::optional<Vcpu> vcpu;
  if (!options.dry_run) {
    auto k = Kvm::Open();
    if (!k) return Propagate(k.error());
    kvm.emplace(std::move(*k));
    auto v = kvm->CreateVm();
    if (!v) return Propagate(v.error());
    vm.emplace(std::move(*v));
    if (auto s = vm->SetTssAddr(x86::kTssAddr); !s) return Propagate(s.error());
    if (auto s = vm->SetIdentityMapAddr(x86::kIdentityMapAddr); !s) return Propagate(s.error());
    if (auto s = vm->CreateIrqchip(); !s) return Propagate(s.error());
    if (auto s = vm->CreatePit2(); !s) return Propagate(s.error());
    auto c = vm->CreateVcpu(0);
    if (!c) return Propagate(c.error());
    vcpu.emplace(std::move(*c));
  }
#else
  if (!options.dry_run) {
    return Fail("booting Linux needs an x86_64 KVM host; use --dry-run to exercise the loader on this machine");
  }
#endif
  trace.Mark(BootPhase::kKvmReady);

  auto mem = GuestMemory::Create(options.mem_mib << 20, options.prefault);
  if (!mem) return Propagate(mem.error());
#if defined(__x86_64__)
  if (vm) {
    if (auto s = vm->SetUserMemoryRegion(0, 0, mem->size(), mem->host_address()); !s) return Propagate(s.error());
  }
#endif
  trace.Mark(BootPhase::kMemoryMapped);

  auto kernel_bytes = ReadFile(options.kernel_path);
  if (!kernel_bytes) return Propagate(kernel_bytes.error());
  std::vector<std::byte> initrd;
  if (!options.initrd_path.empty()) {
    auto r = ReadFile(options.initrd_path);
    if (!r) return Propagate(r.error());
    initrd = std::move(*r);
  }
  auto kernel = ParseKernel(*kernel_bytes);
  if (!kernel) return Propagate(kernel.error(), options.kernel_path);

  Cmdline cmdline(kernel->cmdline_max);
  if (auto s = cmdline.Append(options.cmdline.value_or(std::string(kDefaultCmdline))); !s) {
    return Propagate(s.error());
  }
  if (auto s = cmdline.Append(options.append); !s) return Propagate(s.error());

  auto layout = LoadLinux(*mem, *kernel, cmdline, initrd);
  if (!layout) return Propagate(layout.error());
  trace.Mark(BootPhase::kKernelLoaded);

  PrintLayout(*kernel, *layout, cmdline, mem->size());
  if (options.dry_run) return 0;

#if defined(__x86_64__)
  return RunLinux(*kvm, *vm, *vcpu, *layout, options, trace);
#else
  return Fail("unreachable");
#endif
}

}  // namespace tvmm
