#include "memory/guest_memory.h"

#include <sys/mman.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace tvmm {

namespace {

constexpr std::uint64_t kPageSize = 4096;

}  // namespace

Result<GuestMemory> GuestMemory::Create(std::uint64_t size, bool prefault) {
  if (size == 0 || size % kPageSize != 0) {
    return Fail("guest memory size must be a non-zero multiple of 4 KiB");
  }
  int flags = MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE;
  if (prefault) flags |= MAP_POPULATE;
  auto mapping = Mapping::Create(size, PROT_READ | PROT_WRITE, flags);
  if (!mapping) return Propagate(mapping.error(), "guest memory");
  return GuestMemory(std::move(*mapping));
}

Result<std::span<std::byte>> GuestMemory::Slice(std::uint64_t gpa, std::uint64_t length) const {
  if (gpa > size() || length > size() - gpa) {
    return Fail("guest range [" + std::to_string(gpa) + ", +" + std::to_string(length) + ") outside guest memory of " +
                std::to_string(size()) + " bytes");
  }
  return mapping_.bytes().subspan(gpa, length);
}

Status GuestMemory::Write(std::uint64_t gpa, std::span<const std::byte> data) const {
  auto dst = Slice(gpa, data.size());
  if (!dst) return Propagate(dst.error());
  std::copy(data.begin(), data.end(), dst->begin());
  return {};
}

Status GuestMemory::Zero(std::uint64_t gpa, std::uint64_t length) const {
  auto dst = Slice(gpa, length);
  if (!dst) return Propagate(dst.error());
  std::fill(dst->begin(), dst->end(), std::byte{0});
  return {};
}

}  // namespace tvmm
