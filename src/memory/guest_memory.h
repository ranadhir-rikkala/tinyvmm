#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "common/mapping.h"
#include "common/result.h"

namespace tvmm {

class GuestMemory {
 public:
  static Result<GuestMemory> Create(std::uint64_t size, bool prefault);

  std::uint64_t size() const { return mapping_.size(); }
  void* host_address() const { return mapping_.data(); }

  Result<std::span<std::byte>> Slice(std::uint64_t gpa, std::uint64_t length) const;
  Status Write(std::uint64_t gpa, std::span<const std::byte> data) const;
  Status Zero(std::uint64_t gpa, std::uint64_t length) const;

 private:
  explicit GuestMemory(Mapping mapping) : mapping_(std::move(mapping)) {}

  Mapping mapping_;
};

}  // namespace tvmm
