#include "boot/linux_loader.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

#include "common/bytes.h"
#include "test_images.h"
#include "x86/layout.h"
#include "x86/zero_page.h"

namespace tvmm {
namespace {

using testing::Pattern;

std::vector<std::byte> SmallElf() {
  testing::ElfSpec spec;
  spec.entry = 0x1000010;
  spec.segments.push_back({1, 0x1000000, Pattern(0x2000, 5), 0x6000});
  return testing::MakeElf(spec);
}

template <typename T>
T ReadGuest(const GuestMemory& mem, std::uint64_t gpa) {
  auto slice = mem.Slice(gpa, sizeof(T));
  EXPECT_TRUE(slice);
  return LoadLe<T>(*slice, 0);
}

Cmdline DefaultCmdline() {
  Cmdline c(kDefaultCmdlineMax);
  EXPECT_TRUE(c.Append(kDefaultCmdline));
  return c;
}

TEST(LinuxLoaderTest, LoadsElfKernelWithInitrd) {
  auto mem = GuestMemory::Create(64ULL << 20, false);
  ASSERT_TRUE(mem);
  const auto image = SmallElf();
  auto kernel = ParseKernel(image);
  ASSERT_TRUE(kernel);
  const auto cmdline = DefaultCmdline();
  const auto initrd = Pattern(10000, 7);

  ASSERT_TRUE(mem->Zero(0x1000000, 0x6000));
  auto dirty = Pattern(0x4000, 0x99);
  ASSERT_TRUE(mem->Write(0x1002000, dirty));

  auto layout = LoadLinux(*mem, *kernel, cmdline, initrd);
  ASSERT_TRUE(layout) << layout.error().message;

  EXPECT_EQ(layout->entry, 0x1000010u);
  EXPECT_EQ(layout->zero_page, x86::kZeroPageAddr);
  EXPECT_EQ(layout->kernel_start, 0x1000000u);
  EXPECT_EQ(layout->kernel_end, 0x1006000u);

  auto text = mem->Slice(0x1000000, 0x2000);
  ASSERT_TRUE(text);
  EXPECT_TRUE(std::equal(text->begin(), text->end(), kernel->segments[0].data.begin()));
  auto bss = mem->Slice(0x1002000, 0x4000);
  ASSERT_TRUE(bss);
  EXPECT_TRUE(std::all_of(bss->begin(), bss->end(), [](std::byte b) { return b == std::byte{0}; }));

  auto cl = mem->Slice(x86::kCmdlineAddr, cmdline.str().size() + 1);
  ASSERT_TRUE(cl);
  EXPECT_EQ(std::memcmp(cl->data(), cmdline.str().c_str(), cmdline.str().size() + 1), 0);

  EXPECT_EQ(layout->initrd_size, initrd.size());
  EXPECT_EQ(layout->initrd_addr % 4096, 0u);
  EXPECT_GE(layout->initrd_addr, layout->kernel_end);
  EXPECT_LE(layout->initrd_addr + layout->initrd_size, mem->size());
  auto rd = mem->Slice(layout->initrd_addr, initrd.size());
  ASSERT_TRUE(rd);
  EXPECT_TRUE(std::equal(rd->begin(), rd->end(), initrd.begin()));

  const auto zp = x86::kZeroPageAddr;
  EXPECT_EQ(ReadGuest<std::uint32_t>(*mem, zp + x86::bp::kHeaderMagic), x86::bp::kHdrSMagic);
  EXPECT_EQ(ReadGuest<std::uint32_t>(*mem, zp + x86::bp::kCmdLinePtr), x86::kCmdlineAddr);
  EXPECT_EQ(ReadGuest<std::uint32_t>(*mem, zp + x86::bp::kRamdiskImage), layout->initrd_addr);
  EXPECT_EQ(ReadGuest<std::uint32_t>(*mem, zp + x86::bp::kRamdiskSize), initrd.size());
  EXPECT_EQ(ReadGuest<std::uint8_t>(*mem, zp + x86::bp::kE820Entries), layout->e820.size());

  EXPECT_EQ(ReadGuest<std::uint64_t>(*mem, x86::kGdtAddr + 0x10), x86::GdtEntry(0xa09b, 0, 0xfffff));
  EXPECT_EQ(layout->tables.pml4, x86::kPageTablesAddr);
  EXPECT_EQ(ReadGuest<std::uint64_t>(*mem, x86::kPageTablesAddr), (x86::kPageTablesAddr + 0x1000) | 0x3);
}

TEST(LinuxLoaderTest, LoadsBzImageAndCopiesSetupHeader) {
  auto mem = GuestMemory::Create(64ULL << 20, false);
  ASSERT_TRUE(mem);
  const auto image = testing::MakeBzImage({});
  auto kernel = ParseKernel(image);
  ASSERT_TRUE(kernel);
  auto layout = LoadLinux(*mem, *kernel, DefaultCmdline(), {});
  ASSERT_TRUE(layout) << layout.error().message;
  EXPECT_EQ(layout->entry, 0x100200u);
  EXPECT_EQ(ReadGuest<std::uint16_t>(*mem, x86::kZeroPageAddr + x86::bp::kVersion), 0x020f);
  EXPECT_EQ(ReadGuest<std::uint8_t>(*mem, x86::kZeroPageAddr + x86::bp::kTypeOfLoader), 0xff);
  EXPECT_EQ(ReadGuest<std::uint32_t>(*mem, x86::kZeroPageAddr + x86::bp::kRamdiskSize), 0u);
  EXPECT_EQ(ReadGuest<std::uint8_t>(*mem, 0x100000), std::to_integer<std::uint8_t>(Pattern(1, 0x42)[0]));
}

TEST(LinuxLoaderTest, FailsWhenKernelDoesNotFit) {
  auto mem = GuestMemory::Create(16ULL << 20, false);
  ASSERT_TRUE(mem);
  const auto image = SmallElf();
  auto kernel = ParseKernel(image);
  ASSERT_TRUE(kernel);
  EXPECT_FALSE(LoadLinux(*mem, *kernel, DefaultCmdline(), {}));
}

TEST(LinuxLoaderTest, FailsWhenInitrdWouldOverlapKernel) {
  auto mem = GuestMemory::Create(24ULL << 20, false);
  ASSERT_TRUE(mem);
  const auto image = SmallElf();
  auto kernel = ParseKernel(image);
  ASSERT_TRUE(kernel);
  const auto initrd = Pattern(8ULL << 20, 1);
  EXPECT_FALSE(LoadLinux(*mem, *kernel, DefaultCmdline(), initrd));
}

TEST(LinuxLoaderTest, FailsWhenCmdlineExceedsKernelLimit) {
  auto mem = GuestMemory::Create(64ULL << 20, false);
  ASSERT_TRUE(mem);
  testing::BzImageSpec spec;
  spec.cmdline_size = 8;
  const auto image = testing::MakeBzImage(spec);
  auto kernel = ParseKernel(image);
  ASSERT_TRUE(kernel);
  EXPECT_FALSE(LoadLinux(*mem, *kernel, DefaultCmdline(), {}));
}

TEST(LinuxLoaderTest, LongModeTablesFitBelowCmdline) {
  auto mem = GuestMemory::Create(3ULL << 30, false);
  ASSERT_TRUE(mem);
  auto tables = WriteLongModeTables(*mem);
  ASSERT_TRUE(tables) << tables.error().message;
  EXPECT_EQ(tables->gdt_limit, 5 * 8 - 1);
  EXPECT_EQ(tables->page_table_bytes, 5u * 4096);
  EXPECT_LE(x86::kPageTablesAddr + tables->page_table_bytes, x86::kCmdlineAddr);
}

}  // namespace
}  // namespace tvmm
