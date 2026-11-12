#include "x86/zero_page.h"

#include <gtest/gtest.h>

#include "test_images.h"

namespace tvmm::x86 {
namespace {

ZeroPageConfig BasicConfig() {
  ZeroPageConfig config;
  config.cmdline_addr = 0x20000;
  config.cmdline_size = 2047;
  config.e820 = {{0, 0x9fc00, E820Type::kRam}, {0x100000, 0x1f00000, E820Type::kRam}};
  return config;
}

TEST(ZeroPageTest, SynthesizesHeaderForElfKernels) {
  auto zp = BuildZeroPage(BasicConfig());
  ASSERT_TRUE(zp);
  EXPECT_EQ(zp->Get<std::uint16_t>(0x1fe), 0xaa55);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x202), 0x53726448u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x230), 0x01000000u);
  EXPECT_EQ(zp->Get<std::uint8_t>(0x210), 0xff);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x228), 0x20000u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x0c8), 0u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x238), 2047u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x218), 0u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x21c), 0u);
}

TEST(ZeroPageTest, WritesE820TableAtLinuxOffsets) {
  auto config = BasicConfig();
  auto zp = BuildZeroPage(config);
  ASSERT_TRUE(zp);
  EXPECT_EQ(zp->Get<std::uint8_t>(0x1e8), 2);
  EXPECT_EQ(zp->Get<std::uint64_t>(0x2d0 + 20), 0x100000u);
  EXPECT_EQ(zp->Get<std::uint64_t>(0x2d0 + 28), 0x1f00000u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x2d0 + 36), 1u);
  EXPECT_EQ(zp->E820(), config.e820);
}

TEST(ZeroPageTest, SetsInitrdAndSplitsHighAddresses) {
  auto config = BasicConfig();
  config.initrd_addr = 0x1'2345'6000ULL;
  config.initrd_size = 0x80000;
  config.cmdline_addr = 0x2'0002'0000ULL;
  auto zp = BuildZeroPage(config);
  ASSERT_TRUE(zp);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x218), 0x23456000u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x0c0), 0x1u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x21c), 0x80000u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x0c4), 0u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x228), 0x00020000u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x0c8), 0x2u);
}

TEST(ZeroPageTest, CopiesBzImageSetupHeaderAndOverridesLoaderFields) {
  const auto image = testing::MakeBzImage({});
  const auto header = std::span<const std::byte>(image).subspan(0x1f1, 0x26c - 0x1f1);
  auto config = BasicConfig();
  config.setup_header = header;
  auto zp = BuildZeroPage(config);
  ASSERT_TRUE(zp);
  EXPECT_EQ(zp->Get<std::uint16_t>(0x206), 0x020f);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x260), 0x800000u);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x230), 0x200000u);
  EXPECT_EQ(zp->Get<std::uint8_t>(0x210), 0xff);
  EXPECT_EQ(zp->Get<std::uint32_t>(0x228), 0x20000u);
}

TEST(ZeroPageTest, RejectsHeaderWithoutMagic) {
  std::vector<std::byte> header(0x40, std::byte{0});
  auto config = BasicConfig();
  config.setup_header = header;
  EXPECT_FALSE(BuildZeroPage(config));
}

TEST(ZeroPageTest, RejectsOversizedInputs) {
  auto config = BasicConfig();
  config.e820.assign(kMaxE820Entries + 1, E820Entry{});
  EXPECT_FALSE(BuildZeroPage(config));

  std::vector<std::byte> header(0x100, std::byte{0});
  config = BasicConfig();
  config.setup_header = header;
  EXPECT_FALSE(BuildZeroPage(config));
}

}  // namespace
}  // namespace tvmm::x86
