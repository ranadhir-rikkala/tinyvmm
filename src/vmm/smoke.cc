#include "vmm/smoke.h"

#include <cstdio>
#include <span>
#include <string>

#include "guests/smoke_guests.h"

#if defined(__x86_64__)
#include "boot/linux_loader.h"
#include "devices/uart8250.h"
#include "kvm/kvm.h"
#include "kvm/x86_cpu.h"
#include "memory/guest_memory.h"
#include "vmm/exit_stats.h"
#include "x86/layout.h"
#endif

namespace tvmm {

#if defined(__x86_64__)

namespace {

constexpr std::uint64_t kSmokeMemory = 2ULL << 20;
constexpr std::uint64_t kMaxExits = 4096;

}  // namespace

Result<int> RunSmoke(SmokeMode mode) {
  const bool real = mode == SmokeMode::kReal;
  const std::string_view name = real ? "real" : "long";
  const std::string_view expected = real ? guests::kRealModeGreeting : guests::kLongModeGreeting;
  const auto guest = real ? std::span<const std::uint8_t>(guests::kRealModeGuest)
                          : std::span<const std::uint8_t>(guests::kLongModeGuest);

  auto kvm = Kvm::Open();
  if (!kvm) return Propagate(kvm.error());
  auto vm = kvm->CreateVm();
  if (!vm) return Propagate(vm.error());
  if (auto s = vm->SetTssAddr(x86::kTssAddr); !s) return Propagate(s.error());
  if (auto s = vm->SetIdentityMapAddr(x86::kIdentityMapAddr); !s) return Propagate(s.error());

  auto mem = GuestMemory::Create(kSmokeMemory, false);
  if (!mem) return Propagate(mem.error());
  if (auto s = vm->SetUserMemoryRegion(0, 0, mem->size(), mem->host_address()); !s) return Propagate(s.error());
  if (auto s = mem->Write(x86::kGuestCodeAddr, std::as_bytes(guest)); !s) return Propagate(s.error());

  auto vcpu = vm->CreateVcpu(0);
  if (!vcpu) return Propagate(vcpu.error());
  if (auto s = x86::ConfigureCpuid(*kvm, *vcpu, 0); !s) return Propagate(s.error());
  if (real) {
    if (auto s = x86::SetupRealMode(*vcpu, x86::kGuestCodeAddr); !s) return Propagate(s.error());
  } else {
    auto tables = WriteLongModeTables(*mem);
    if (!tables) return Propagate(tables.error());
    if (auto s = x86::SetupLongMode(*vcpu, *tables, x86::kGuestCodeAddr, 0); !s) return Propagate(s.error());
  }

  std::string output;
  Uart8250 uart([&output](std::uint8_t b) { output += static_cast<char>(b); });
  ExitStats stats;
  kvm_run* run = vcpu->run_state();
  bool halted = false;

  while (!halted && stats.total < kMaxExits) {
    if (auto s = vcpu->Run(); !s) return Propagate(s.error());
    ++stats.total;
    switch (run->exit_reason) {
      case KVM_EXIT_IO: {
        ++stats.io;
        ++stats.io_by_port[run->io.port];
        auto* data = reinterpret_cast<std::uint8_t*>(run) + run->io.data_offset;
        const bool serial = run->io.port >= x86::kSerialPortBase &&
                            run->io.port < x86::kSerialPortBase + x86::kSerialPortCount && run->io.size == 1;
        for (std::uint32_t i = 0; i < run->io.count; ++i, data += run->io.size) {
          const auto reg = static_cast<std::uint8_t>(run->io.port - x86::kSerialPortBase);
          if (serial && run->io.direction == KVM_EXIT_IO_OUT) {
            uart.Write(reg, *data);
          } else if (serial) {
            *data = uart.Read(reg);
          }
        }
        break;
      }
      case KVM_EXIT_HLT:
        halted = true;
        break;
      case KVM_EXIT_SHUTDOWN:
        return Fail("guest triple-faulted (KVM_EXIT_SHUTDOWN)");
      case KVM_EXIT_FAIL_ENTRY:
        return Fail("KVM_EXIT_FAIL_ENTRY, hardware reason " +
                    std::to_string(run->fail_entry.hardware_entry_failure_reason));
      case KVM_EXIT_INTERNAL_ERROR:
        return Fail("KVM_EXIT_INTERNAL_ERROR, suberror " + std::to_string(run->internal.suberror));
      default:
        return Fail("unexpected exit reason " + std::to_string(run->exit_reason));
    }
  }

  std::fprintf(stdout, "%s", output.c_str());
  std::fprintf(stderr, "%s", stats.Format().c_str());
  const bool pass = halted && output == expected;
  std::fprintf(stderr, "smoke(%s): %s\n", std::string(name).c_str(), pass ? "PASS" : "FAIL");
  return pass ? 0 : 1;
}

#else

Result<int> RunSmoke(SmokeMode) { return Fail("the smoke guests are x86 code and need an x86_64 KVM host"); }

#endif

}  // namespace tvmm
