#include "trace/marker_scanner.h"

namespace tvmm {

MarkerScanner::MarkerScanner(std::string marker) : marker_(std::move(marker)), failure_(marker_.size(), 0) {
  for (std::size_t i = 1, k = 0; i < marker_.size(); ++i) {
    while (k > 0 && marker_[i] != marker_[k]) k = failure_[k - 1];
    if (marker_[i] == marker_[k]) ++k;
    failure_[i] = k;
  }
}

bool MarkerScanner::Feed(char c) {
  if (found_ || marker_.empty()) return false;
  while (matched_ > 0 && c != marker_[matched_]) matched_ = failure_[matched_ - 1];
  if (c == marker_[matched_]) ++matched_;
  if (matched_ == marker_.size()) {
    found_ = true;
    return true;
  }
  return false;
}

}  // namespace tvmm
