#pragma once

#include <array>
#include <cstdint>

namespace tvmm::x86 {

struct Segment {
  std::uint64_t base = 0;
  std::uint32_t limit = 0;
  std::uint16_t selector = 0;
  std::uint8_t type = 0;
  std::uint8_t present = 0;
  std::uint8_t dpl = 0;
  std::uint8_t db = 0;
  std::uint8_t s = 0;
  std::uint8_t l = 0;
  std::uint8_t g = 0;
  std::uint8_t avl = 0;
};

inline constexpr std::uint16_t kBootCodeSelector = 0x10;
inline constexpr std::uint16_t kBootDataSelector = 0x18;
inline constexpr std::uint16_t kBootTssSelector = 0x20;

constexpr std::uint64_t GdtEntry(std::uint16_t flags, std::uint32_t base, std::uint32_t limit) {
  const std::uint64_t b = base;
  const std::uint64_t l = limit;
  const std::uint64_t f = flags;
  return ((b & 0xff000000ULL) << (56 - 24)) | ((f & 0x0000f0ffULL) << 40) | ((l & 0x000f0000ULL) << (48 - 16)) |
         ((b & 0x00ffffffULL) << 16) | (l & 0x0000ffffULL);
}

Segment SegmentFromGdtEntry(std::uint64_t entry, std::uint16_t selector);

using BootGdt = std::array<std::uint64_t, 5>;

BootGdt BuildBootGdt();

}  // namespace tvmm::x86
