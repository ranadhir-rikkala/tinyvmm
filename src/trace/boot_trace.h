#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace tvmm {

enum class BootPhase : std::size_t {
  kVmmStart,
  kKvmReady,
  kMemoryMapped,
  kKernelLoaded,
  kFirstVcpuEntry,
  kFirstSerialByte,
  kInitMarker,
  kCount,
};

inline constexpr std::size_t kBootPhaseCount = static_cast<std::size_t>(BootPhase::kCount);

std::string_view BootPhaseName(BootPhase phase);

std::uint64_t MonotonicNanos();

class BootTrace {
 public:
  using Clock = std::function<std::uint64_t()>;

  explicit BootTrace(Clock clock = MonotonicNanos) : clock_(std::move(clock)) {}

  void Mark(BootPhase phase);
  std::optional<std::uint64_t> SinceStartNanos(BootPhase phase) const;

  std::string FormatTable() const;
  std::string ToJson() const;

 private:
  Clock clock_;
  std::array<std::optional<std::uint64_t>, kBootPhaseCount> marks_{};
};

}  // namespace tvmm
