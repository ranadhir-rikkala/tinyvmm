#include "boot/cmdline.h"

#include <gtest/gtest.h>

namespace tvmm {
namespace {

TEST(CmdlineTest, AppendsAndNormalizesWhitespace) {
  Cmdline c(64);
  ASSERT_TRUE(c.Append("console=ttyS0   panic=1"));
  ASSERT_TRUE(c.Append(" quiet "));
  EXPECT_EQ(c.str(), "console=ttyS0 panic=1 quiet");
}

TEST(CmdlineTest, ContainsMatchesWholeKeysOnly) {
  Cmdline c(128);
  ASSERT_TRUE(c.Append("consoleblank=0 quiet console=ttyS0"));
  EXPECT_TRUE(c.Contains("console"));
  EXPECT_TRUE(c.Contains("quiet"));
  EXPECT_TRUE(c.Contains("consoleblank"));
  EXPECT_FALSE(c.Contains("cons"));
  EXPECT_FALSE(c.Contains("panic"));
}

TEST(CmdlineTest, InsertQuotesValuesWithSpaces) {
  Cmdline c(128);
  ASSERT_TRUE(c.Insert("init", "/init"));
  ASSERT_TRUE(c.Insert("dyndbg", "file x.c +p"));
  EXPECT_EQ(c.str(), "init=/init dyndbg=\"file x.c +p\"");
  EXPECT_TRUE(c.Contains("dyndbg"));
}

TEST(CmdlineTest, InsertRejectsBadKeysAndValues) {
  Cmdline c(128);
  EXPECT_FALSE(c.Insert("", "x"));
  EXPECT_FALSE(c.Insert("a b", "x"));
  EXPECT_FALSE(c.Insert("a=b", "x"));
  EXPECT_FALSE(c.Insert("a", "x\"y"));
  EXPECT_TRUE(c.str().empty());
}

TEST(CmdlineTest, KeepsQuotedSpacesInOneToken) {
  Cmdline c(128);
  ASSERT_TRUE(c.Append("a=\"x  y\" b"));
  EXPECT_EQ(c.str(), "a=\"x  y\" b");
  EXPECT_TRUE(c.Contains("b"));
}

TEST(CmdlineTest, RejectsInvalidCharactersAndQuotes) {
  Cmdline c(128);
  EXPECT_FALSE(c.Append("bad\nline"));
  EXPECT_FALSE(c.Append("tab\there"));
  EXPECT_FALSE(c.Append("a=\"unterminated"));
  EXPECT_TRUE(c.str().empty());
}

TEST(CmdlineTest, EnforcesCapacityExactlyAndAtomically) {
  Cmdline c(10);
  ASSERT_TRUE(c.Append("abcd"));
  EXPECT_FALSE(c.Append("efghi xyz"));
  EXPECT_EQ(c.str(), "abcd");
  ASSERT_TRUE(c.Append("efghi"));
  EXPECT_EQ(c.str().size(), 10u);
  EXPECT_FALSE(c.Append("x"));
}

TEST(CmdlineTest, ToBytesIsNulTerminated) {
  Cmdline c(16);
  ASSERT_TRUE(c.Append("ab"));
  const auto bytes = c.ToBytes();
  ASSERT_EQ(bytes.size(), 3u);
  EXPECT_EQ(bytes[0], std::byte{'a'});
  EXPECT_EQ(bytes[1], std::byte{'b'});
  EXPECT_EQ(bytes[2], std::byte{0});
}

TEST(CmdlineTest, DefaultFitsLinuxLimit) {
  Cmdline c(2047);
  ASSERT_TRUE(c.Append(kDefaultCmdline));
  EXPECT_TRUE(c.Contains("console"));
  EXPECT_TRUE(c.Contains("reboot"));
}

}  // namespace
}  // namespace tvmm
