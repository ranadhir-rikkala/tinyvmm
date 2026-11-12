#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "common/result.h"

namespace tvmm {

inline constexpr std::string_view kDefaultCmdline =
    "console=ttyS0 reboot=k panic=1 pci=off nomodule 8250.nr_uarts=1 "
    "i8042.noaux i8042.nomux i8042.nopnp i8042.dumbkbd";

class Cmdline {
 public:
  explicit Cmdline(std::size_t capacity) : capacity_(capacity) {}

  Status Append(std::string_view params);
  Status Insert(std::string_view key, std::string_view value);
  bool Contains(std::string_view key) const;

  const std::string& str() const { return line_; }
  std::size_t capacity() const { return capacity_; }
  std::vector<std::byte> ToBytes() const;

 private:
  Status AppendToken(std::string_view token);

  std::size_t capacity_;
  std::string line_;
};

}  // namespace tvmm
