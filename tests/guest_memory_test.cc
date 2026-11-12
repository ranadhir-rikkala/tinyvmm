#include "memory/guest_memory.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "test_images.h"

namespace tvmm {
namespace {

TEST(GuestMemoryTest, StartsZeroedAndWritesRoundTrip) {
  auto mem = GuestMemory::Create(1 << 20, false);
  ASSERT_TRUE(mem);
  auto slice = mem->Slice(0x1000, 16);
  ASSERT_TRUE(slice);
  for (auto b : *slice) EXPECT_EQ(b, std::byte{0});

  const auto data = testing::Pattern(16, 3);
  ASSERT_TRUE(mem->Write(0x1000, data));
  auto again = mem->Slice(0x1000, 16);
  ASSERT_TRUE(again);
  EXPECT_TRUE(std::equal(again->begin(), again->end(), data.begin()));

  ASSERT_TRUE(mem->Zero(0x1004, 4));
  EXPECT_EQ((*again)[4], std::byte{0});
  EXPECT_EQ((*again)[8], data[8]);
}

TEST(GuestMemoryTest, PrefaultedMappingWorks) {
  auto mem = GuestMemory::Create(1 << 20, true);
  ASSERT_TRUE(mem);
  EXPECT_EQ(mem->size(), 1u << 20);
  EXPECT_NE(mem->host_address(), nullptr);
}

TEST(GuestMemoryTest, RejectsOutOfBoundsAccess) {
  auto mem = GuestMemory::Create(1 << 20, false);
  ASSERT_TRUE(mem);
  EXPECT_TRUE(mem->Slice((1 << 20) - 1, 1));
  EXPECT_TRUE(mem->Slice(1 << 20, 0));
  EXPECT_FALSE(mem->Slice((1 << 20) - 1, 2));
  EXPECT_FALSE(mem->Slice(std::numeric_limits<std::uint64_t>::max(), 2));
  EXPECT_FALSE(mem->Slice(2, std::numeric_limits<std::uint64_t>::max()));
  EXPECT_FALSE(mem->Write(1 << 20, testing::Pattern(1, 0)));
}

TEST(GuestMemoryTest, RejectsBadSizes) {
  EXPECT_FALSE(GuestMemory::Create(0, false));
  EXPECT_FALSE(GuestMemory::Create(4097, false));
}

TEST(GuestMemoryTest, MoveTransfersOwnership) {
  auto mem = GuestMemory::Create(1 << 16, false);
  ASSERT_TRUE(mem);
  void* addr = mem->host_address();
  GuestMemory moved = std::move(*mem);
  EXPECT_EQ(moved.host_address(), addr);
  EXPECT_EQ(mem->size(), 0u);
}

}  // namespace
}  // namespace tvmm
