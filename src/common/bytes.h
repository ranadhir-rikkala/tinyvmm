#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <string_view>
#include <type_traits>

namespace tvmm {

static_assert(std::endian::native == std::endian::little, "tinyvmm assumes a little-endian host");

template <typename T>
  requires std::is_trivially_copyable_v<T>
void StoreLe(std::span<std::byte> dst, std::size_t offset, T value) {
  assert(offset + sizeof(T) <= dst.size());
  std::memcpy(dst.data() + offset, &value, sizeof(T));
}

template <typename T>
  requires std::is_trivially_copyable_v<T>
T LoadLe(std::span<const std::byte> src, std::size_t offset) {
  assert(offset + sizeof(T) <= src.size());
  T value;
  std::memcpy(&value, src.data() + offset, sizeof(T));
  return value;
}

inline std::span<const std::byte> AsBytes(std::string_view s) {
  return std::as_bytes(std::span<const char>(s.data(), s.size()));
}

}  // namespace tvmm
