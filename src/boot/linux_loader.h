#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "boot/cmdline.h"
#include "boot/kernel_image.h"
#include "common/result.h"
#include "memory/guest_memory.h"
#include "x86/e820.h"
#include "x86/gdt.h"

namespace tvmm {

struct LongModeTables {
  x86::BootGdt gdt{};
  std::uint64_t gdt_addr = 0;
  std::uint16_t gdt_limit = 0;
  std::uint64_t idt_addr = 0;
  std::uint16_t idt_limit = 0;
  std::uint64_t pml4 = 0;
  std::uint64_t page_table_bytes = 0;
};

struct BootLayout {
  std::uint64_t entry = 0;
  std::uint64_t zero_page = 0;
  std::uint64_t cmdline_addr = 0;
  std::uint64_t cmdline_len = 0;
  std::uint64_t kernel_start = 0;
  std::uint64_t kernel_end = 0;
  std::uint64_t initrd_addr = 0;
  std::uint64_t initrd_size = 0;
  LongModeTables tables;
  std::vector<x86::E820Entry> e820;
};

Result<LongModeTables> WriteLongModeTables(const GuestMemory& mem);

Result<BootLayout> LoadLinux(const GuestMemory& mem, const KernelImage& kernel, const Cmdline& cmdline,
                             std::span<const std::byte> initrd);

}  // namespace tvmm
