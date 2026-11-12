#include "guests/smoke_guests.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace tvmm::guests {
namespace {

std::string MessageAt(std::span<const std::uint8_t> guest, std::size_t offset) {
  std::string out;
  while (offset < guest.size() && guest[offset] != 0) out += static_cast<char>(guest[offset++]);
  return out;
}

TEST(SmokeGuestsTest, RealModeMessageIsWhereSiPoints) {
  const std::uint16_t si = static_cast<std::uint16_t>(kRealModeGuest[1] | kRealModeGuest[2] << 8);
  EXPECT_EQ(si, 0x1010);
  EXPECT_EQ(MessageAt(kRealModeGuest, si - 0x1000), kRealModeGreeting);
}

TEST(SmokeGuestsTest, RealModeBranchesLandOnLoopAndHalt) {
  const std::size_t jz_next = 11;
  EXPECT_EQ(kRealModeGuest[jz_next + static_cast<std::int8_t>(kRealModeGuest[10])], 0xf4);
  const std::size_t jmp_next = 14;
  EXPECT_EQ(kRealModeGuest[jmp_next + static_cast<std::int8_t>(kRealModeGuest[13])], 0xac);
}

TEST(SmokeGuestsTest, LongModeRipRelativeLeaPointsAtMessage) {
  const std::int32_t disp = static_cast<std::int32_t>(kLongModeGuest[7] | kLongModeGuest[8] << 8 |
                                                      kLongModeGuest[9] << 16 | kLongModeGuest[10] << 24);
  EXPECT_EQ(MessageAt(kLongModeGuest, 11 + disp), kLongModeGreeting);
  EXPECT_EQ(kLongModeGuest[16 + static_cast<std::int8_t>(kLongModeGuest[15])], 0xf4);
  EXPECT_EQ(kLongModeGuest[19 + static_cast<std::int8_t>(kLongModeGuest[18])], 0xac);
}

}  // namespace
}  // namespace tvmm::guests
