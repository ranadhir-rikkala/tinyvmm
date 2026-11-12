#include <charconv>
#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "trace/boot_trace.h"
#include "vmm/linux_boot.h"
#include "vmm/smoke.h"

namespace {

constexpr std::string_view kUsage = R"(usage:
  tinyvmm smoke [--mode real|long]
  tinyvmm boot --kernel PATH [options]

boot options:
  --initrd PATH       load an initramfs
  --cmdline STR       replace the default kernel command line
  --append STR        append to the kernel command line
  --mem MIB           guest memory in MiB (default 256, max 3072)
  --init-marker STR   serial string that marks init reached (default TINYVMM_INIT_OK)
  --exit-on-marker    stop the VM as soon as the init marker is seen
  --timeout SEC       stop the VM after SEC seconds
  --prefault          populate guest memory up front (MAP_POPULATE)
  --dry-run           build guest memory without KVM and print the layout
  --no-echo           do not copy guest serial output to stdout
  --trace-json PATH   write boot phase timestamps as JSON
)";

int Usage() {
  std::fputs(kUsage.data(), stderr);
  return 2;
}

template <typename T>
bool ParseNumber(std::string_view s, T& out) {
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
  return ec == std::errc() && ptr == s.data() + s.size();
}

int RunSmokeCommand(const std::vector<std::string_view>& args) {
  tvmm::SmokeMode mode = tvmm::SmokeMode::kReal;
  for (std::size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--mode" && i + 1 < args.size()) {
      const auto m = args[++i];
      if (m == "real") {
        mode = tvmm::SmokeMode::kReal;
      } else if (m == "long") {
        mode = tvmm::SmokeMode::kLong;
      } else {
        return Usage();
      }
    } else {
      return Usage();
    }
  }
  auto result = tvmm::RunSmoke(mode);
  if (!result) {
    std::fprintf(stderr, "tinyvmm: %s\n", result.error().message.c_str());
    return 1;
  }
  return *result;
}

int RunBootCommand(const std::vector<std::string_view>& args, tvmm::BootTrace& trace) {
  tvmm::BootOptions options;
  std::string trace_json;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const auto arg = args[i];
    const bool has_value = i + 1 < args.size();
    if (arg == "--kernel" && has_value) {
      options.kernel_path = args[++i];
    } else if (arg == "--initrd" && has_value) {
      options.initrd_path = args[++i];
    } else if (arg == "--cmdline" && has_value) {
      options.cmdline = std::string(args[++i]);
    } else if (arg == "--append" && has_value) {
      options.append = args[++i];
    } else if (arg == "--mem" && has_value) {
      if (!ParseNumber(args[++i], options.mem_mib)) return Usage();
    } else if (arg == "--init-marker" && has_value) {
      options.init_marker = args[++i];
    } else if (arg == "--exit-on-marker") {
      options.exit_on_marker = true;
    } else if (arg == "--timeout" && has_value) {
      if (!ParseNumber(args[++i], options.timeout_sec)) return Usage();
    } else if (arg == "--prefault") {
      options.prefault = true;
    } else if (arg == "--dry-run") {
      options.dry_run = true;
    } else if (arg == "--no-echo") {
      options.echo_serial = false;
    } else if (arg == "--trace-json" && has_value) {
      trace_json = args[++i];
    } else {
      return Usage();
    }
  }
  if (options.kernel_path.empty()) return Usage();

  auto result = tvmm::BootLinux(options, trace);
  std::fprintf(stderr, "\n%s", trace.FormatTable().c_str());
  if (!trace_json.empty()) {
    std::ofstream(trace_json) << trace.ToJson() << '\n';
  }
  if (!result) {
    std::fprintf(stderr, "tinyvmm: %s\n", result.error().message.c_str());
    return 1;
  }
  return *result;
}

}  // namespace

int main(int argc, char** argv) {
  tvmm::BootTrace trace;
  trace.Mark(tvmm::BootPhase::kVmmStart);

  if (argc < 2) return Usage();
  const std::string_view command = argv[1];
  const std::vector<std::string_view> args(argv + 2, argv + argc);
  if (command == "smoke") return RunSmokeCommand(args);
  if (command == "boot") return RunBootCommand(args, trace);
  return Usage();
}
