#include "x86/page_tables.h"

#include <gtest/gtest.h>

namespace tvmm::x86 {
namespace {

TEST(PageTablesTest, OneGibLayout) {
  auto tables = BuildIdentityPageTables(0x9000, 512ULL << 20);
  ASSERT_TRUE(tables);
  ASSERT_EQ(tables->words.size(), 3u * 512);
  EXPECT_EQ(tables->size_bytes(), 3u * 4096);
  EXPECT_EQ(tables->pml4(), 0x9000u);
  EXPECT_EQ(tables->words[0], 0xa000u | kPtePresent | kPteWritable);
  EXPECT_EQ(tables->words[512], 0xb000u | kPtePresent | kPteWritable);
  EXPECT_EQ(tables->words[1024], kPtePresent | kPteWritable | kPteHuge);
  EXPECT_EQ(tables->words[1025], 0x200000u | kPtePresent | kPteWritable | kPteHuge);
  for (std::size_t i = 1; i < 512; ++i) EXPECT_EQ(tables->words[i], 0u);
}

TEST(PageTablesTest, TranslatesIdentityWithinMappedRange) {
  auto tables = BuildIdentityPageTables(0x9000, 1ULL << 30);
  ASSERT_TRUE(tables);
  for (std::uint64_t va : {0x0ULL, 0x7000ULL, 0x100200ULL, 0x1000000ULL, 0x3fffffffULL}) {
    auto pa = Translate(*tables, va);
    ASSERT_TRUE(pa.has_value()) << std::hex << va;
    EXPECT_EQ(*pa, va);
  }
  EXPECT_FALSE(Translate(*tables, 1ULL << 30).has_value());
  EXPECT_FALSE(Translate(*tables, 1ULL << 39).has_value());
}

TEST(PageTablesTest, RoundsUpToWholeGibAndMapsThreeGib) {
  auto tables = BuildIdentityPageTables(0x9000, (2ULL << 30) + 1);
  ASSERT_TRUE(tables);
  EXPECT_EQ(tables->words.size(), 5u * 512);
  EXPECT_EQ(*Translate(*tables, 0xa0001234ULL), 0xa0001234ULL);
  EXPECT_FALSE(Translate(*tables, 3ULL << 30).has_value());
}

TEST(PageTablesTest, BytesViewCoversAllWords) {
  auto tables = BuildIdentityPageTables(0x9000, 1);
  ASSERT_TRUE(tables);
  EXPECT_EQ(tables->bytes().size(), tables->size_bytes());
}

TEST(PageTablesTest, RejectsBadInput) {
  EXPECT_FALSE(BuildIdentityPageTables(0x9001, 1ULL << 30));
  EXPECT_FALSE(BuildIdentityPageTables(0x9000, 513ULL << 30));
}

}  // namespace
}  // namespace tvmm::x86
