#include "trace/marker_scanner.h"

#include <gtest/gtest.h>

#include <string_view>

namespace tvmm {
namespace {

int FeedAll(MarkerScanner& scanner, std::string_view text) {
  int hits = 0;
  for (char c : text) hits += scanner.Feed(c) ? 1 : 0;
  return hits;
}

TEST(MarkerScannerTest, FindsMarkerSplitAcrossChunks) {
  MarkerScanner scanner("INIT_OK");
  EXPECT_EQ(FeedAll(scanner, "[  0.1] booting... INI"), 0);
  EXPECT_EQ(FeedAll(scanner, "T_OK\n"), 1);
  EXPECT_TRUE(scanner.found());
}

TEST(MarkerScannerTest, HandlesOverlappingPrefixes) {
  MarkerScanner scanner("aab");
  EXPECT_EQ(FeedAll(scanner, "aaab"), 1);
  MarkerScanner scanner2("abab");
  EXPECT_EQ(FeedAll(scanner2, "abaabab"), 1);
}

TEST(MarkerScannerTest, ReportsOnlyOnce) {
  MarkerScanner scanner("ok");
  EXPECT_EQ(FeedAll(scanner, "ok ok ok"), 1);
}

TEST(MarkerScannerTest, EmptyMarkerNeverMatches) {
  MarkerScanner scanner("");
  EXPECT_EQ(FeedAll(scanner, "anything"), 0);
  EXPECT_FALSE(scanner.found());
}

}  // namespace
}  // namespace tvmm
