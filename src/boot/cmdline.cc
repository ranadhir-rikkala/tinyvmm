#include "boot/cmdline.h"

#include <algorithm>

namespace tvmm {

namespace {

bool IsPrintable(char c) { return c >= 0x20 && c <= 0x7e; }

Result<std::vector<std::string_view>> Tokenize(std::string_view s) {
  std::vector<std::string_view> tokens;
  bool in_quotes = false;
  std::size_t start = std::string_view::npos;
  for (std::size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (!IsPrintable(c)) return Fail("command line contains a non-printable character");
    if (c == '"') in_quotes = !in_quotes;
    if (c == ' ' && !in_quotes) {
      if (start != std::string_view::npos) tokens.push_back(s.substr(start, i - start));
      start = std::string_view::npos;
    } else if (start == std::string_view::npos) {
      start = i;
    }
  }
  if (in_quotes) return Fail("command line has an unterminated quote");
  if (start != std::string_view::npos) tokens.push_back(s.substr(start));
  return tokens;
}

}  // namespace

Status Cmdline::Append(std::string_view params) {
  auto tokens = Tokenize(params);
  if (!tokens) return Propagate(tokens.error());
  std::size_t needed = line_.size();
  for (auto token : *tokens) needed += (needed == 0 ? 0 : 1) + token.size();
  if (needed > capacity_) return Fail("command line exceeds " + std::to_string(capacity_) + " bytes");
  for (auto token : *tokens) {
    if (auto s = AppendToken(token); !s) return s;
  }
  return {};
}

Status Cmdline::Insert(std::string_view key, std::string_view value) {
  if (key.empty()) return Fail("command line key is empty");
  if (key.find_first_of(" =\"") != std::string_view::npos) {
    return Fail("command line key contains a space, '=' or quote");
  }
  if (value.find('"') != std::string_view::npos) return Fail("command line value contains a quote");
  std::string token(key);
  token += '=';
  if (value.find(' ') != std::string_view::npos) {
    token += '"';
    token += value;
    token += '"';
  } else {
    token += value;
  }
  return Append(token);
}

bool Cmdline::Contains(std::string_view key) const {
  auto tokens = Tokenize(line_);
  if (!tokens) return false;
  return std::any_of(tokens->begin(), tokens->end(), [&](std::string_view token) {
    return token == key || (token.size() > key.size() && token.starts_with(key) && token[key.size()] == '=');
  });
}

std::vector<std::byte> Cmdline::ToBytes() const {
  std::vector<std::byte> out(line_.size() + 1, std::byte{0});
  std::transform(line_.begin(), line_.end(), out.begin(), [](char c) { return static_cast<std::byte>(c); });
  return out;
}

Status Cmdline::AppendToken(std::string_view token) {
  const std::size_t extra = (line_.empty() ? 0 : 1) + token.size();
  if (line_.size() + extra > capacity_) {
    return Fail("command line exceeds " + std::to_string(capacity_) + " bytes");
  }
  if (!line_.empty()) line_ += ' ';
  line_ += token;
  return {};
}

}  // namespace tvmm
