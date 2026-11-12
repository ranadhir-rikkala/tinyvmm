#include "x86/gdt.h"

namespace tvmm::x86 {

Segment SegmentFromGdtEntry(std::uint64_t entry, std::uint16_t selector) {
  Segment seg;
  seg.base = ((entry >> 16) & 0x00ffffffULL) | ((entry >> 32) & 0xff000000ULL);
  const auto raw_limit = static_cast<std::uint32_t>((entry & 0xffffULL) | ((entry >> 32) & 0xf0000ULL));
  seg.type = static_cast<std::uint8_t>((entry >> 40) & 0xf);
  seg.s = static_cast<std::uint8_t>((entry >> 44) & 0x1);
  seg.dpl = static_cast<std::uint8_t>((entry >> 45) & 0x3);
  seg.present = static_cast<std::uint8_t>((entry >> 47) & 0x1);
  seg.avl = static_cast<std::uint8_t>((entry >> 52) & 0x1);
  seg.l = static_cast<std::uint8_t>((entry >> 53) & 0x1);
  seg.db = static_cast<std::uint8_t>((entry >> 54) & 0x1);
  seg.g = static_cast<std::uint8_t>((entry >> 55) & 0x1);
  seg.limit = seg.g ? ((raw_limit << 12) | 0xfff) : raw_limit;
  seg.selector = selector;
  return seg;
}

BootGdt BuildBootGdt() {
  return {
      0, 0, GdtEntry(0xa09b, 0, 0xfffff), GdtEntry(0xc093, 0, 0xfffff), GdtEntry(0x808b, 0, 0xfffff),
  };
}

}  // namespace tvmm::x86
