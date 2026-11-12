#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace tvmm {

class MarkerScanner {
 public:
  explicit MarkerScanner(std::string marker);

  bool Feed(char c);
  bool found() const { return found_; }
  const std::string& marker() const { return marker_; }

 private:
  std::string marker_;
  std::vector<std::size_t> failure_;
  std::size_t matched_ = 0;
  bool found_ = false;
};

}  // namespace tvmm
