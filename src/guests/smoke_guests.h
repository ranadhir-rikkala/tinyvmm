#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace tvmm::guests {

inline constexpr std::string_view kRealModeGreeting = "Hello from real mode\n";
inline constexpr std::string_view kLongModeGreeting = "Hello from long mode\n";

template <std::size_t N>
constexpr auto WithMessage(const std::array<std::uint8_t, N>& code, std::string_view msg) {
  std::array<std::uint8_t, N + 32> out{};
  for (std::size_t i = 0; i < N; ++i) out[i] = code[i];
  for (std::size_t i = 0; i < msg.size(); ++i) out[N + i] = static_cast<std::uint8_t>(msg[i]);
  return out;
}

// 16-bit real mode, loaded at 0x1000 with CS=DS=0.
inline constexpr std::array<std::uint8_t, 16> kRealModeCode = {
    0xbe, 0x10, 0x10,  // mov si, 0x1010
    0xba, 0xf8, 0x03,  // mov dx, 0x3f8
    0xac,              // lodsb
    0x84, 0xc0,        // test al, al
    0x74, 0x03,        // jz +3
    0xee,              // out dx, al
    0xeb, 0xf8,        // jmp -8
    0xf4,              // hlt
    0x90,              // nop
};

// 64-bit long mode, loaded at 0x1000 with the boot GDT and identity page tables.
inline constexpr std::array<std::uint8_t, 20> kLongModeCode = {
    0x66, 0xba, 0xf8, 0x03,                    // mov dx, 0x3f8
    0x48, 0x8d, 0x35, 0x09, 0x00, 0x00, 0x00,  // lea rsi, [rip + 9]
    0xac,                                      // lodsb
    0x84, 0xc0,                                // test al, al
    0x74, 0x03,                                // jz +3
    0xee,                                      // out dx, al
    0xeb, 0xf8,                                // jmp -8
    0xf4,                                      // hlt
};

inline constexpr auto kRealModeGuest = WithMessage(kRealModeCode, kRealModeGreeting);
inline constexpr auto kLongModeGuest = WithMessage(kLongModeCode, kLongModeGreeting);

}  // namespace tvmm::guests
