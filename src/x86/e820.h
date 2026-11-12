#pragma once

#include <cstdint>
#include <vector>

#include "common/result.h"

namespace tvmm::x86 {

enum class E820Type : std::uint32_t {
  kRam = 1,
  kReserved = 2,
};

struct E820Entry {
  std::uint64_t addr = 0;
  std::uint64_t size = 0;
  E820Type type = E820Type::kRam;

  std::uint64_t end() const { return addr + size; }
  bool operator==(const E820Entry&) const = default;
};

inline constexpr std::size_t kMaxE820Entries = 128;
inline constexpr std::uint64_t kMinGuestMemory = 2ULL << 20;

Result<std::vector<E820Entry>> BuildE820Map(std::uint64_t mem_size);

}  // namespace tvmm::x86
