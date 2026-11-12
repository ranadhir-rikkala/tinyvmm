#include "trace/boot_trace.h"

#include <time.h>

#include <cstdio>

namespace tvmm {

namespace {

constexpr std::array<std::string_view, kBootPhaseCount> kNames = {
    "vmm_start", "kvm_ready", "memory_mapped", "kernel_loaded", "first_vcpu_entry", "first_serial_byte", "init_marker",
};

std::string Millis(std::uint64_t nanos) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.3f", static_cast<double>(nanos) / 1e6);
  return buf;
}

}  // namespace

std::string_view BootPhaseName(BootPhase phase) { return kNames[static_cast<std::size_t>(phase)]; }

std::uint64_t MonotonicNanos() {
  timespec ts{};
  ::clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<std::uint64_t>(ts.tv_nsec);
}

void BootTrace::Mark(BootPhase phase) {
  auto& slot = marks_[static_cast<std::size_t>(phase)];
  if (!slot) slot = clock_();
}

std::optional<std::uint64_t> BootTrace::SinceStartNanos(BootPhase phase) const {
  const auto& start = marks_[0];
  const auto& at = marks_[static_cast<std::size_t>(phase)];
  if (!start || !at) return std::nullopt;
  return *at - *start;
}

std::string BootTrace::FormatTable() const {
  std::string out;
  char line[128];
  std::snprintf(line, sizeof(line), "%-20s %14s %12s\n", "phase", "since_start_ms", "delta_ms");
  out += line;
  std::optional<std::uint64_t> previous;
  for (std::size_t i = 0; i < kBootPhaseCount; ++i) {
    const auto since = SinceStartNanos(static_cast<BootPhase>(i));
    if (!since) {
      std::snprintf(line, sizeof(line), "%-20s %14s %12s\n", std::string(kNames[i]).c_str(), "-", "not reached");
    } else {
      const std::uint64_t delta = previous ? *since - *previous : *since;
      std::snprintf(line, sizeof(line), "%-20s %14s %12s\n", std::string(kNames[i]).c_str(), Millis(*since).c_str(),
                    Millis(delta).c_str());
      previous = since;
    }
    out += line;
  }
  return out;
}

std::string BootTrace::ToJson() const {
  std::string out = "{\"unit\":\"ns\",\"phases\":{";
  for (std::size_t i = 0; i < kBootPhaseCount; ++i) {
    if (i != 0) out += ',';
    out += '"';
    out += kNames[i];
    out += "\":";
    const auto since = SinceStartNanos(static_cast<BootPhase>(i));
    out += since ? std::to_string(*since) : "null";
  }
  out += "}}";
  return out;
}

}  // namespace tvmm
