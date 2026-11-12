#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "common/result.h"

namespace tvmm {

enum class KernelFormat { kElf, kBzImage };

std::string_view KernelFormatName(KernelFormat format);

struct LoadSegment {
  std::uint64_t guest_addr = 0;
  std::span<const std::byte> data;
  std::uint64_t mem_size = 0;
};

struct KernelImage {
  KernelFormat format = KernelFormat::kElf;
  std::uint64_t entry = 0;
  std::vector<LoadSegment> segments;
  std::span<const std::byte> setup_header;
  std::uint16_t boot_protocol = 0;
  std::uint32_t cmdline_max = 0;
  std::uint64_t initrd_addr_max = 0;
  std::uint64_t kernel_end = 0;
};

inline constexpr std::uint32_t kDefaultCmdlineMax = 2047;

Result<KernelImage> ParseKernel(std::span<const std::byte> image);

}  // namespace tvmm
