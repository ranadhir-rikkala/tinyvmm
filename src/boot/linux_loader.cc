#include "boot/linux_loader.h"

#include <algorithm>
#include <string>

#include "x86/layout.h"
#include "x86/page_tables.h"
#include "x86/zero_page.h"

namespace tvmm {

namespace {

constexpr std::uint64_t kPageSize = 4096;

constexpr std::uint64_t AlignUp(std::uint64_t v, std::uint64_t a) { return (v + a - 1) & ~(a - 1); }
constexpr std::uint64_t AlignDown(std::uint64_t v, std::uint64_t a) { return v & ~(a - 1); }

std::string Hex(std::uint64_t v) {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string out;
  do {
    out.insert(out.begin(), kDigits[v & 0xf]);
    v >>= 4;
  } while (v != 0);
  return "0x" + out;
}

Result<std::uint64_t> PlaceInitrd(std::uint64_t mem_size, const KernelImage& kernel, std::uint64_t size) {
  const std::uint64_t top = std::min<std::uint64_t>(mem_size, kernel.initrd_addr_max + 1);
  if (size > top) return Fail("initrd does not fit in guest memory");
  const std::uint64_t addr = AlignDown(top - size, kPageSize);
  if (addr < AlignUp(kernel.kernel_end, kPageSize)) {
    return Fail("initrd would overlap the kernel (kernel ends at " + Hex(kernel.kernel_end) + ", initrd at " +
                Hex(addr) + "); give the guest more memory");
  }
  return addr;
}

}  // namespace

Result<LongModeTables> WriteLongModeTables(const GuestMemory& mem) {
  LongModeTables t;
  t.gdt = x86::BuildBootGdt();
  t.gdt_addr = x86::kGdtAddr;
  t.gdt_limit = static_cast<std::uint16_t>(sizeof(t.gdt) - 1);
  t.idt_addr = x86::kIdtAddr;
  t.idt_limit = sizeof(std::uint64_t) - 1;

  if (auto s = mem.Write(t.gdt_addr, std::as_bytes(std::span(t.gdt))); !s) return Propagate(s.error(), "GDT");
  if (auto s = mem.Zero(t.idt_addr, t.idt_limit + 1); !s) return Propagate(s.error(), "IDT");

  auto tables = x86::BuildIdentityPageTables(x86::kPageTablesAddr, mem.size());
  if (!tables) return Propagate(tables.error());
  if (x86::kPageTablesAddr + tables->size_bytes() > x86::kCmdlineAddr) {
    return Fail("page tables overlap the command line area");
  }
  if (auto s = mem.Write(tables->base, tables->bytes()); !s) return Propagate(s.error(), "page tables");
  t.pml4 = tables->pml4();
  t.page_table_bytes = tables->size_bytes();
  return t;
}

Result<BootLayout> LoadLinux(const GuestMemory& mem, const KernelImage& kernel, const Cmdline& cmdline,
                             std::span<const std::byte> initrd) {
  BootLayout layout;
  auto e820 = x86::BuildE820Map(mem.size());
  if (!e820) return Propagate(e820.error());
  layout.e820 = std::move(*e820);

  if (kernel.kernel_end > mem.size()) {
    return Fail("kernel needs memory up to " + Hex(kernel.kernel_end) + " but the guest has " + Hex(mem.size()));
  }

  layout.kernel_start = kernel.segments.front().guest_addr;
  for (const auto& seg : kernel.segments) {
    layout.kernel_start = std::min(layout.kernel_start, seg.guest_addr);
    if (auto s = mem.Write(seg.guest_addr, seg.data); !s) return Propagate(s.error(), "kernel segment");
    if (auto s = mem.Zero(seg.guest_addr + seg.data.size(), seg.mem_size - seg.data.size()); !s) {
      return Propagate(s.error(), "kernel bss");
    }
  }
  layout.kernel_end = kernel.kernel_end;
  layout.entry = kernel.entry;

  if (cmdline.str().size() > kernel.cmdline_max) {
    return Fail("command line is longer than the kernel accepts (" + std::to_string(kernel.cmdline_max) + ")");
  }
  const auto cmdline_bytes = cmdline.ToBytes();
  if (auto s = mem.Write(x86::kCmdlineAddr, cmdline_bytes); !s) return Propagate(s.error(), "cmdline");
  layout.cmdline_addr = x86::kCmdlineAddr;
  layout.cmdline_len = cmdline.str().size();

  if (!initrd.empty()) {
    auto addr = PlaceInitrd(mem.size(), kernel, initrd.size());
    if (!addr) return Propagate(addr.error());
    if (auto s = mem.Write(*addr, initrd); !s) return Propagate(s.error(), "initrd");
    layout.initrd_addr = *addr;
    layout.initrd_size = initrd.size();
  }

  auto tables = WriteLongModeTables(mem);
  if (!tables) return Propagate(tables.error());
  layout.tables = *tables;

  x86::ZeroPageConfig zp_config;
  zp_config.setup_header = kernel.setup_header;
  zp_config.cmdline_addr = layout.cmdline_addr;
  zp_config.cmdline_size = kernel.cmdline_max;
  zp_config.initrd_addr = layout.initrd_addr;
  zp_config.initrd_size = layout.initrd_size;
  zp_config.e820 = layout.e820;
  auto zero_page = x86::BuildZeroPage(zp_config);
  if (!zero_page) return Propagate(zero_page.error());
  if (auto s = mem.Write(x86::kZeroPageAddr, zero_page->bytes()); !s) return Propagate(s.error(), "zero page");
  layout.zero_page = x86::kZeroPageAddr;
  return layout;
}

}  // namespace tvmm
