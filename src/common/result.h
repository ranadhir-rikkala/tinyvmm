#pragma once

#include <cerrno>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace tvmm {

struct Error {
  std::string message;
  int code = 0;
};

struct Unexpected {
  Error error;
};

inline Unexpected Fail(std::string message, int code = 0) { return Unexpected{Error{std::move(message), code}}; }

inline Unexpected FailErrno(std::string_view what) {
  const int saved = errno;
  return Unexpected{Error{std::string(what) + ": " + std::strerror(saved), saved}};
}

inline Unexpected Propagate(const Error& error, std::string_view context = {}) {
  if (context.empty()) return Unexpected{error};
  return Unexpected{Error{std::string(context) + ": " + error.message, error.code}};
}

template <typename T>
class [[nodiscard]] Result {
 public:
  Result(T value) : storage_(std::in_place_index<0>, std::move(value)) {}
  Result(Unexpected unexpected) : storage_(std::in_place_index<1>, std::move(unexpected.error)) {}

  bool has_value() const { return storage_.index() == 0; }
  explicit operator bool() const { return has_value(); }

  T& value() & { return std::get<0>(storage_); }
  const T& value() const& { return std::get<0>(storage_); }
  T&& value() && { return std::get<0>(std::move(storage_)); }

  const Error& error() const { return std::get<1>(storage_); }

  T* operator->() { return &value(); }
  const T* operator->() const { return &value(); }
  T& operator*() & { return value(); }
  const T& operator*() const& { return value(); }

 private:
  std::variant<T, Error> storage_;
};

template <>
class [[nodiscard]] Result<void> {
 public:
  Result() = default;
  Result(Unexpected unexpected) : error_(std::move(unexpected.error)) {}

  bool has_value() const { return !error_.has_value(); }
  explicit operator bool() const { return has_value(); }
  const Error& error() const { return *error_; }

 private:
  std::optional<Error> error_;
};

using Status = Result<void>;

}  // namespace tvmm
