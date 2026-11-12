#include "x86/e820.h"

#include <string>

#include "x86/layout.h"

namespace tvmm::x86 {

Result<std::vector<E820Entry>> BuildE820Map(std::uint64_t mem_size) {
  if (mem_size < kMinGuestMemory) {
    return Fail("guest memory must be at least " + std::to_string(kMinGuestMemory >> 20) + " MiB");
  }
  if (mem_size > kMmioGapStart) {
    return Fail("guest memory above the 32-bit MMIO gap (3 GiB) is not supported yet");
  }
  return std::vector<E820Entry>{
      {0, kEbdaStart, E820Type::kRam},
      {kEbdaStart, kHighMemStart - kEbdaStart, E820Type::kReserved},
      {kHighMemStart, mem_size - kHighMemStart, E820Type::kRam},
  };
}

}  // namespace tvmm::x86
