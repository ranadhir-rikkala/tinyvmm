#include "boot/kernel_image.h"

#include <gtest/gtest.h>

#include "test_images.h"

namespace tvmm {
namespace {

using testing::ElfSpec;
using testing::MakeBzImage;
using testing::MakeElf;
using testing::Pattern;

ElfSpec TwoSegmentKernel() {
  ElfSpec spec;
  spec.entry = 0x1000000;
  spec.segments.push_back({1, 0x1000000, Pattern(0x3000, 1), 0x3000});
  spec.segments.push_back({4, 0, Pattern(16, 9), 16});
  spec.segments.push_back({1, 0x1200000, Pattern(0x800, 2), 0x5000});
  return spec;
}

TEST(KernelImageTest, ParsesElfVmlinux) {
  const auto img = MakeElf(TwoSegmentKernel());
  auto kernel = ParseKernel(img);
  ASSERT_TRUE(kernel) << kernel.error().message;
  EXPECT_EQ(kernel->format, KernelFormat::kElf);
  EXPECT_EQ(kernel->entry, 0x1000000u);
  ASSERT_EQ(kernel->segments.size(), 2u);
  EXPECT_EQ(kernel->segments[0].guest_addr, 0x1000000u);
  EXPECT_EQ(kernel->segments[0].data.size(), 0x3000u);
  EXPECT_EQ(kernel->segments[1].guest_addr, 0x1200000u);
  EXPECT_EQ(kernel->segments[1].data.size(), 0x800u);
  EXPECT_EQ(kernel->segments[1].mem_size, 0x5000u);
  EXPECT_EQ(kernel->segments[1].data[0], Pattern(1, 2)[0]);
  EXPECT_EQ(kernel->kernel_end, 0x1205000u);
  EXPECT_EQ(kernel->cmdline_max, kDefaultCmdlineMax);
  EXPECT_TRUE(kernel->setup_header.empty());
}

TEST(KernelImageTest, RejectsBadElfKernels) {
  auto spec = TwoSegmentKernel();
  spec.machine = 183;
  EXPECT_FALSE(ParseKernel(MakeElf(spec)));

  spec = TwoSegmentKernel();
  spec.type = 3;
  EXPECT_FALSE(ParseKernel(MakeElf(spec)));

  spec = TwoSegmentKernel();
  spec.entry = 0x2000000;
  EXPECT_FALSE(ParseKernel(MakeElf(spec)));

  spec = TwoSegmentKernel();
  spec.segments[0].paddr = 0x8000;
  EXPECT_FALSE(ParseKernel(MakeElf(spec)));

  spec = TwoSegmentKernel();
  spec.segments[0].mem_size = 0x10;
  EXPECT_FALSE(ParseKernel(MakeElf(spec)));

  auto img = MakeElf(TwoSegmentKernel());
  img.resize(img.size() - 1);
  EXPECT_FALSE(ParseKernel(img));
}

TEST(KernelImageTest, ParsesBzImage) {
  const auto img = MakeBzImage({});
  auto kernel = ParseKernel(img);
  ASSERT_TRUE(kernel) << kernel.error().message;
  EXPECT_EQ(kernel->format, KernelFormat::kBzImage);
  EXPECT_EQ(kernel->boot_protocol, 0x020f);
  EXPECT_EQ(kernel->entry, 0x100200u);
  ASSERT_EQ(kernel->segments.size(), 1u);
  EXPECT_EQ(kernel->segments[0].guest_addr, 0x100000u);
  EXPECT_EQ(kernel->segments[0].data.data(), img.data() + 5 * 512);
  EXPECT_EQ(kernel->segments[0].data.size(), 8192u);
  EXPECT_EQ(kernel->setup_header.size(), 0x26cu - 0x1f1u);
  EXPECT_EQ(kernel->cmdline_max, 2047u);
  EXPECT_EQ(kernel->initrd_addr_max, 0x7fffffffu);
  EXPECT_EQ(kernel->kernel_end, 0x1000000u + 0x800000u);
}

TEST(KernelImageTest, BzImageHonorsSetupSects) {
  testing::BzImageSpec spec;
  spec.setup_sects = 0x1b;
  const auto img = MakeBzImage(spec);
  auto kernel = ParseKernel(img);
  ASSERT_TRUE(kernel);
  EXPECT_EQ(kernel->segments[0].data.data(), img.data() + (0x1b + 1) * 512);
}

TEST(KernelImageTest, RejectsUnsupportedBzImages) {
  testing::BzImageSpec spec;
  spec.version = 0x020b;
  EXPECT_FALSE(ParseKernel(MakeBzImage(spec)));

  spec = {};
  spec.xloadflags = 0;
  EXPECT_FALSE(ParseKernel(MakeBzImage(spec)));

  spec = {};
  spec.loadflags = 0;
  EXPECT_FALSE(ParseKernel(MakeBzImage(spec)));
}

TEST(KernelImageTest, RejectsGarbage) {
  EXPECT_FALSE(ParseKernel(Pattern(4096, 0)));
  EXPECT_FALSE(ParseKernel({}));
}

}  // namespace
}  // namespace tvmm
