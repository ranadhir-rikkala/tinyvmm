#include "x86/page_tables.h"

#include <algorithm>

namespace tvmm::x86 {

namespace {

constexpr std::uint64_t kPageSize = 4096;
constexpr std::uint64_t kEntriesPerTable = 512;
constexpr std::uint64_t kAddrMask = 0x000ffffffffff000ULL;

std::optional<std::uint64_t> Entry(const PageTables& tables, std::uint64_t table_addr, std::uint64_t index) {
  if (table_addr < tables.base) return std::nullopt;
  const std::uint64_t word = (table_addr - tables.base) / sizeof(std::uint64_t) + index;
  if ((table_addr - tables.base) % kPageSize != 0 || word >= tables.words.size()) return std::nullopt;
  return tables.words[word];
}

}  // namespace

Result<PageTables> BuildIdentityPageTables(std::uint64_t base, std::uint64_t bytes_to_map) {
  if (base % kPageSize != 0) return Fail("page table base must be 4 KiB aligned");
  const std::uint64_t gib = std::max<std::uint64_t>(1, (bytes_to_map + kGiB - 1) / kGiB);
  if (gib > kEntriesPerTable) return Fail("identity map is limited to 512 GiB");

  PageTables tables;
  tables.base = base;
  tables.words.assign((2 + gib) * kEntriesPerTable, 0);

  const std::uint64_t pdpt = base + kPageSize;
  tables.words[0] = pdpt | kPtePresent | kPteWritable;

  for (std::uint64_t i = 0; i < gib; ++i) {
    const std::uint64_t pd = base + (2 + i) * kPageSize;
    tables.words[kEntriesPerTable + i] = pd | kPtePresent | kPteWritable;
    for (std::uint64_t j = 0; j < kEntriesPerTable; ++j) {
      const std::uint64_t phys = (i * kEntriesPerTable + j) * kHugePageSize;
      tables.words[(2 + i) * kEntriesPerTable + j] = phys | kPtePresent | kPteWritable | kPteHuge;
    }
  }
  return tables;
}

std::optional<std::uint64_t> Translate(const PageTables& tables, std::uint64_t vaddr) {
  const auto pml4e = Entry(tables, tables.base, (vaddr >> 39) & 0x1ff);
  if (!pml4e || !(*pml4e & kPtePresent)) return std::nullopt;
  const auto pdpte = Entry(tables, *pml4e & kAddrMask, (vaddr >> 30) & 0x1ff);
  if (!pdpte || !(*pdpte & kPtePresent)) return std::nullopt;
  const auto pde = Entry(tables, *pdpte & kAddrMask, (vaddr >> 21) & 0x1ff);
  if (!pde || !(*pde & kPtePresent) || !(*pde & kPteHuge)) return std::nullopt;
  return (*pde & ~(kHugePageSize - 1) & kAddrMask) | (vaddr & (kHugePageSize - 1));
}

}  // namespace tvmm::x86
