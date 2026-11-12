#pragma once

#include <sys/mman.h>

#include <cstddef>
#include <span>
#include <utility>

#include "common/result.h"

namespace tvmm {

class Mapping {
 public:
  Mapping() = default;
  ~Mapping() { Reset(); }

  Mapping(Mapping&& other) noexcept
      : addr_(std::exchange(other.addr_, nullptr)), size_(std::exchange(other.size_, 0)) {}
  Mapping& operator=(Mapping&& other) noexcept {
    if (this != &other) {
      Reset();
      addr_ = std::exchange(other.addr_, nullptr);
      size_ = std::exchange(other.size_, 0);
    }
    return *this;
  }
  Mapping(const Mapping&) = delete;
  Mapping& operator=(const Mapping&) = delete;

  static Result<Mapping> Create(std::size_t size, int prot, int flags, int fd = -1, off_t offset = 0) {
    void* addr = ::mmap(nullptr, size, prot, flags, fd, offset);
    if (addr == MAP_FAILED) return FailErrno("mmap");
    Mapping mapping;
    mapping.addr_ = addr;
    mapping.size_ = size;
    return mapping;
  }

  void* data() const { return addr_; }
  std::size_t size() const { return size_; }
  std::span<std::byte> bytes() const { return {static_cast<std::byte*>(addr_), size_}; }

  void Reset() {
    if (addr_ != nullptr) ::munmap(addr_, size_);
    addr_ = nullptr;
    size_ = 0;
  }

 private:
  void* addr_ = nullptr;
  std::size_t size_ = 0;
};

}  // namespace tvmm
