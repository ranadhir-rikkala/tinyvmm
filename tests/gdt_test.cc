#include "x86/gdt.h"

#include <gtest/gtest.h>

namespace tvmm::x86 {
namespace {

TEST(GdtTest, EncodesWellKnownFlatDescriptors) {
  EXPECT_EQ(GdtEntry(0xa09b, 0, 0xfffff), 0x00af9b000000ffffULL);
  EXPECT_EQ(GdtEntry(0xc093, 0, 0xfffff), 0x00cf93000000ffffULL);
  EXPECT_EQ(GdtEntry(0x808b, 0, 0xfffff), 0x008f8b000000ffffULL);
}

TEST(GdtTest, Decodes64BitCodeSegment) {
  const Segment cs = SegmentFromGdtEntry(GdtEntry(0xa09b, 0, 0xfffff), kBootCodeSelector);
  EXPECT_EQ(cs.base, 0u);
  EXPECT_EQ(cs.limit, 0xffffffffu);
  EXPECT_EQ(cs.selector, kBootCodeSelector);
  EXPECT_EQ(cs.type, 0xb);
  EXPECT_EQ(cs.present, 1);
  EXPECT_EQ(cs.s, 1);
  EXPECT_EQ(cs.l, 1);
  EXPECT_EQ(cs.db, 0);
  EXPECT_EQ(cs.g, 1);
  EXPECT_EQ(cs.dpl, 0);
}

TEST(GdtTest, DecodesDataAndTssSegments) {
  const Segment ds = SegmentFromGdtEntry(GdtEntry(0xc093, 0, 0xfffff), kBootDataSelector);
  EXPECT_EQ(ds.type, 0x3);
  EXPECT_EQ(ds.db, 1);
  EXPECT_EQ(ds.l, 0);
  EXPECT_EQ(ds.s, 1);

  const Segment tss = SegmentFromGdtEntry(GdtEntry(0x808b, 0, 0xfffff), kBootTssSelector);
  EXPECT_EQ(tss.type, 0xb);
  EXPECT_EQ(tss.s, 0);
  EXPECT_EQ(tss.present, 1);
}

TEST(GdtTest, RoundTripsBaseAndByteGranularLimit) {
  const Segment seg = SegmentFromGdtEntry(GdtEntry(0x0093, 0x12345678, 0xabcde), 0x8);
  EXPECT_EQ(seg.base, 0x12345678u);
  EXPECT_EQ(seg.limit, 0xabcdeu);
  EXPECT_EQ(seg.g, 0);
}

TEST(GdtTest, BootGdtMatchesLinuxBootSelectors) {
  const BootGdt gdt = BuildBootGdt();
  EXPECT_EQ(gdt[0], 0u);
  EXPECT_EQ(gdt[kBootCodeSelector >> 3], GdtEntry(0xa09b, 0, 0xfffff));
  EXPECT_EQ(gdt[kBootDataSelector >> 3], GdtEntry(0xc093, 0, 0xfffff));
  EXPECT_EQ(gdt[kBootTssSelector >> 3], GdtEntry(0x808b, 0, 0xfffff));
}

}  // namespace
}  // namespace tvmm::x86
