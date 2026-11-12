#include "x86/e820.h"

#include <gtest/gtest.h>

#include "x86/layout.h"

namespace tvmm::x86 {
namespace {

TEST(E820Test, LayoutFor512MiB) {
  const std::uint64_t mem = 512ULL << 20;
  auto map = BuildE820Map(mem);
  ASSERT_TRUE(map);
  const std::vector<E820Entry> expected = {
      {0, 0x9fc00, E820Type::kRam},
      {0x9fc00, 0x100000 - 0x9fc00, E820Type::kReserved},
      {0x100000, mem - 0x100000, E820Type::kRam},
  };
  EXPECT_EQ(*map, expected);
}

TEST(E820Test, EntriesAreSortedContiguousAndCoverMemory) {
  for (std::uint64_t mib : {2ULL, 128ULL, 1024ULL, 3072ULL}) {
    auto map = BuildE820Map(mib << 20);
    ASSERT_TRUE(map) << mib;
    std::uint64_t cursor = 0;
    std::uint64_t ram = 0;
    for (const auto& e : *map) {
      EXPECT_EQ(e.addr, cursor);
      cursor = e.end();
      if (e.type == E820Type::kRam) ram += e.size;
    }
    EXPECT_EQ(cursor, mib << 20);
    EXPECT_EQ(ram, (mib << 20) - (kHighMemStart - kEbdaStart));
  }
}

TEST(E820Test, RejectsTooSmallAndTooLarge) {
  EXPECT_FALSE(BuildE820Map(1ULL << 20));
  EXPECT_FALSE(BuildE820Map(kMmioGapStart + 4096));
  EXPECT_TRUE(BuildE820Map(kMmioGapStart));
}

}  // namespace
}  // namespace tvmm::x86
