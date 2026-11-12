#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "common/result.h"

namespace tvmm::x86 {

inline constexpr std::uint64_t kPtePresent = 1ULL << 0;
inline constexpr std::uint64_t kPteWritable = 1ULL << 1;
inline constexpr std::uint64_t kPteHuge = 1ULL << 7;
inline constexpr std::uint64_t kGiB = 1ULL << 30;
inline constexpr std::uint64_t kHugePageSize = 2ULL << 20;

struct PageTables {
  std::uint64_t base = 0;
  std::vector<std::uint64_t> words;

  std::uint64_t pml4() const { return base; }
  std::size_t size_bytes() const { return words.size() * sizeof(std::uint64_t); }
  std::span<const std::byte> bytes() const { return std::as_bytes(std::span(words)); }
};

Result<PageTables> BuildIdentityPageTables(std::uint64_t base, std::uint64_t bytes_to_map);

std::optional<std::uint64_t> Translate(const PageTables& tables, std::uint64_t vaddr);

}  // namespace tvmm::x86
