#include "trace/boot_trace.h"

#include <gtest/gtest.h>

namespace tvmm {
namespace {

class FakeClock {
 public:
  std::uint64_t now = 1'000'000;
  BootTrace::Clock fn() {
    return [this] { return now; };
  }
};

TEST(BootTraceTest, RecordsFirstMarkOnly) {
  FakeClock clock;
  BootTrace trace(clock.fn());
  trace.Mark(BootPhase::kVmmStart);
  clock.now += 2'500'000;
  trace.Mark(BootPhase::kKvmReady);
  clock.now += 9'000'000;
  trace.Mark(BootPhase::kKvmReady);
  EXPECT_EQ(trace.SinceStartNanos(BootPhase::kVmmStart), 0u);
  EXPECT_EQ(trace.SinceStartNanos(BootPhase::kKvmReady), 2'500'000u);
  EXPECT_FALSE(trace.SinceStartNanos(BootPhase::kInitMarker).has_value());
}

TEST(BootTraceTest, NothingIsRelativeWithoutStart) {
  FakeClock clock;
  BootTrace trace(clock.fn());
  trace.Mark(BootPhase::kKvmReady);
  EXPECT_FALSE(trace.SinceStartNanos(BootPhase::kKvmReady).has_value());
}

TEST(BootTraceTest, TableShowsDeltasAndMissingPhases) {
  FakeClock clock;
  BootTrace trace(clock.fn());
  trace.Mark(BootPhase::kVmmStart);
  clock.now += 1'000'000;
  trace.Mark(BootPhase::kKvmReady);
  clock.now += 500'000;
  trace.Mark(BootPhase::kFirstVcpuEntry);
  const std::string table = trace.FormatTable();
  EXPECT_NE(table.find("kvm_ready"), std::string::npos);
  EXPECT_NE(table.find("1.000"), std::string::npos);
  EXPECT_NE(table.find("1.500"), std::string::npos);
  EXPECT_NE(table.find("0.500"), std::string::npos);
  EXPECT_NE(table.find("not reached"), std::string::npos);
}

TEST(BootTraceTest, JsonUsesNullForMissingPhases) {
  FakeClock clock;
  BootTrace trace(clock.fn());
  trace.Mark(BootPhase::kVmmStart);
  clock.now += 42;
  trace.Mark(BootPhase::kMemoryMapped);
  const std::string json = trace.ToJson();
  EXPECT_NE(json.find("\"vmm_start\":0"), std::string::npos);
  EXPECT_NE(json.find("\"memory_mapped\":42"), std::string::npos);
  EXPECT_NE(json.find("\"init_marker\":null"), std::string::npos);
}

TEST(BootTraceTest, MonotonicClockAdvances) {
  const auto a = MonotonicNanos();
  const auto b = MonotonicNanos();
  EXPECT_LE(a, b);
}

}  // namespace
}  // namespace tvmm
