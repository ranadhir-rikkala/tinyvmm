#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace tvmm {

struct ExitStats {
  std::uint64_t total = 0;
  std::uint64_t io = 0;
  std::uint64_t mmio = 0;
  std::uint64_t interrupted = 0;
  std::map<std::uint16_t, std::uint64_t> io_by_port;

  std::string Format(std::size_t top_ports = 5) const {
    std::string out = "vm exits: total=" + std::to_string(total) + " io=" + std::to_string(io) +
                      " mmio=" + std::to_string(mmio) + " interrupted=" + std::to_string(interrupted) + "\n";
    std::vector<std::pair<std::uint16_t, std::uint64_t>> ports(io_by_port.begin(), io_by_port.end());
    std::sort(ports.begin(), ports.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if (ports.size() > top_ports) ports.resize(top_ports);
    for (const auto& [port, count] : ports) {
      char line[64];
      std::snprintf(line, sizeof(line), "  port 0x%04x: %llu exits\n", port, static_cast<unsigned long long>(count));
      out += line;
    }
    return out;
  }
};

}  // namespace tvmm
