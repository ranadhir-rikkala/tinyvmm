#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "common/bytes.h"
#include "common/result.h"
#include "x86/e820.h"

namespace tvmm::x86 {

namespace bp {
inline constexpr std::size_t kExtRamdiskImage = 0x0c0;
inline constexpr std::size_t kExtRamdiskSize = 0x0c4;
inline constexpr std::size_t kExtCmdLinePtr = 0x0c8;
inline constexpr std::size_t kE820Entries = 0x1e8;
inline constexpr std::size_t kSetupHeader = 0x1f1;
inline constexpr std::size_t kSetupSects = 0x1f1;
inline constexpr std::size_t kBootFlag = 0x1fe;
inline constexpr std::size_t kJump = 0x200;
inline constexpr std::size_t kHeaderMagic = 0x202;
inline constexpr std::size_t kVersion = 0x206;
inline constexpr std::size_t kTypeOfLoader = 0x210;
inline constexpr std::size_t kLoadFlags = 0x211;
inline constexpr std::size_t kRamdiskImage = 0x218;
inline constexpr std::size_t kRamdiskSize = 0x21c;
inline constexpr std::size_t kCmdLinePtr = 0x228;
inline constexpr std::size_t kInitrdAddrMax = 0x22c;
inline constexpr std::size_t kKernelAlignment = 0x230;
inline constexpr std::size_t kXLoadFlags = 0x236;
inline constexpr std::size_t kCmdlineSize = 0x238;
inline constexpr std::size_t kPrefAddress = 0x258;
inline constexpr std::size_t kInitSize = 0x260;
inline constexpr std::size_t kSetupHeaderLimit = 0x290;
inline constexpr std::size_t kE820Table = 0x2d0;
inline constexpr std::size_t kE820EntrySize = 20;

inline constexpr std::uint16_t kBootFlagMagic = 0xaa55;
inline constexpr std::uint32_t kHdrSMagic = 0x53726448;
inline constexpr std::uint8_t kLoaderUndefined = 0xff;
inline constexpr std::uint8_t kLoadedHigh = 0x01;
inline constexpr std::uint16_t kXlfKernel64 = 0x0001;
inline constexpr std::uint32_t kDefaultKernelAlignment = 0x01000000;
}  // namespace bp

class ZeroPage {
 public:
  static constexpr std::size_t kSize = 4096;

  template <typename T>
  void Set(std::size_t offset, T value) {
    StoreLe(std::span<std::byte>(bytes_), offset, value);
  }
  template <typename T>
  T Get(std::size_t offset) const {
    return LoadLe<T>(std::span<const std::byte>(bytes_), offset);
  }

  std::span<const std::byte> bytes() const { return bytes_; }

  Status CopySetupHeader(std::span<const std::byte> header);
  Status SetE820(std::span<const E820Entry> entries);
  std::vector<E820Entry> E820() const;

 private:
  std::array<std::byte, kSize> bytes_{};
};

struct ZeroPageConfig {
  std::span<const std::byte> setup_header;
  std::uint64_t cmdline_addr = 0;
  std::uint32_t cmdline_size = 0;
  std::uint64_t initrd_addr = 0;
  std::uint64_t initrd_size = 0;
  std::vector<E820Entry> e820;
};

Result<ZeroPage> BuildZeroPage(const ZeroPageConfig& config);

}  // namespace tvmm::x86
