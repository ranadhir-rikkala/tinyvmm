#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "common/result.h"
#include "trace/boot_trace.h"

namespace tvmm {

inline constexpr std::string_view kDefaultInitMarker = "TINYVMM_INIT_OK";

struct BootOptions {
  std::string kernel_path;
  std::string initrd_path;
  std::optional<std::string> cmdline;
  std::string append;
  std::uint64_t mem_mib = 256;
  std::string init_marker = std::string(kDefaultInitMarker);
  bool exit_on_marker = false;
  unsigned timeout_sec = 0;
  bool prefault = false;
  bool dry_run = false;
  bool echo_serial = true;
};

Result<int> BootLinux(const BootOptions& options, BootTrace& trace);

}  // namespace tvmm
