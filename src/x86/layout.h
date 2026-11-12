#pragma once

#include <cstdint>

namespace tvmm::x86 {

inline constexpr std::uint64_t kGdtAddr = 0x500;
inline constexpr std::uint64_t kIdtAddr = 0x520;
inline constexpr std::uint64_t kGuestCodeAddr = 0x1000;
inline constexpr std::uint64_t kZeroPageAddr = 0x7000;
inline constexpr std::uint64_t kBootStackTop = 0x8ff0;
inline constexpr std::uint64_t kPageTablesAddr = 0x9000;
inline constexpr std::uint64_t kCmdlineAddr = 0x20000;
inline constexpr std::uint64_t kEbdaStart = 0x9fc00;
inline constexpr std::uint64_t kHighMemStart = 0x100000;
inline constexpr std::uint64_t kBzImageLoadAddr = 0x100000;
inline constexpr std::uint64_t kBzImage64BitEntryOffset = 0x200;
inline constexpr std::uint64_t kMmioGapStart = 0xc0000000;
inline constexpr std::uint64_t kTssAddr = 0xfffbd000;
inline constexpr std::uint64_t kIdentityMapAddr = 0xfffbc000;

inline constexpr std::uint16_t kSerialPortBase = 0x3f8;
inline constexpr std::uint16_t kSerialPortCount = 8;
inline constexpr std::uint32_t kSerialIrq = 4;
inline constexpr std::uint16_t kI8042CommandPort = 0x64;
inline constexpr std::uint8_t kI8042ResetCommand = 0xfe;

}  // namespace tvmm::x86
