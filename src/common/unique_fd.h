#pragma once

#include <unistd.h>

#include <utility>

namespace tvmm {

class UniqueFd {
 public:
  UniqueFd() = default;
  explicit UniqueFd(int fd) : fd_(fd) {}
  ~UniqueFd() { Reset(); }

  UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
      Reset();
      fd_ = std::exchange(other.fd_, -1);
    }
    return *this;
  }
  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  int get() const { return fd_; }
  bool valid() const { return fd_ >= 0; }

  void Reset() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
  }

 private:
  int fd_ = -1;
};

}  // namespace tvmm
