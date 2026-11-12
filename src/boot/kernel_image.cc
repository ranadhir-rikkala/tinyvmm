#include "boot/kernel_image.h"

#include <algorithm>
#include <string>

#include "common/bytes.h"
#include "x86/layout.h"
#include "x86/zero_page.h"

namespace tvmm {

namespace {

namespace elf {
constexpr std::size_t kEhdrSize = 64;
constexpr std::size_t kPhdrSize = 56;
constexpr std::uint8_t kClass64 = 2;
constexpr std::uint8_t kDataLsb = 1;
constexpr std::uint16_t kTypeExec = 2;
constexpr std::uint16_t kMachineX86_64 = 62;
constexpr std::uint32_t kPtLoad = 1;
}  // namespace elf

bool IsElf(std::span<const std::byte> image) {
  return image.size() >= elf::kEhdrSize && image[0] == std::byte{0x7f} && image[1] == std::byte{'E'} &&
         image[2] == std::byte{'L'} && image[3] == std::byte{'F'};
}

bool IsBzImage(std::span<const std::byte> image) {
  return image.size() > x86::bp::kSetupHeaderLimit &&
         LoadLe<std::uint16_t>(image, x86::bp::kBootFlag) == x86::bp::kBootFlagMagic &&
         LoadLe<std::uint32_t>(image, x86::bp::kHeaderMagic) == x86::bp::kHdrSMagic;
}

bool Fits(std::span<const std::byte> image, std::uint64_t offset, std::uint64_t length) {
  return offset <= image.size() && length <= image.size() - offset;
}

Result<KernelImage> ParseElf(std::span<const std::byte> image) {
  if (static_cast<std::uint8_t>(image[4]) != elf::kClass64 || static_cast<std::uint8_t>(image[5]) != elf::kDataLsb) {
    return Fail("ELF kernel must be 64-bit little-endian");
  }
  if (LoadLe<std::uint16_t>(image, 16) != elf::kTypeExec) return Fail("ELF kernel is not an executable");
  if (LoadLe<std::uint16_t>(image, 18) != elf::kMachineX86_64) return Fail("ELF kernel is not x86_64");

  const auto entry = LoadLe<std::uint64_t>(image, 24);
  const auto phoff = LoadLe<std::uint64_t>(image, 32);
  const auto phentsize = LoadLe<std::uint16_t>(image, 54);
  const auto phnum = LoadLe<std::uint16_t>(image, 56);
  if (phentsize != elf::kPhdrSize) return Fail("unexpected ELF program header size");
  if (!Fits(image, phoff, std::uint64_t{phnum} * phentsize)) return Fail("ELF program headers out of bounds");

  KernelImage kernel;
  kernel.format = KernelFormat::kElf;
  kernel.entry = entry;
  kernel.cmdline_max = kDefaultCmdlineMax;
  kernel.initrd_addr_max = 0xffffffff;

  for (std::uint16_t i = 0; i < phnum; ++i) {
    const auto ph = image.subspan(phoff + std::size_t{i} * phentsize, phentsize);
    if (LoadLe<std::uint32_t>(ph, 0) != elf::kPtLoad) continue;
    const auto offset = LoadLe<std::uint64_t>(ph, 8);
    const auto paddr = LoadLe<std::uint64_t>(ph, 24);
    const auto filesz = LoadLe<std::uint64_t>(ph, 32);
    const auto memsz = LoadLe<std::uint64_t>(ph, 40);
    if (memsz == 0) continue;
    if (filesz > memsz) return Fail("ELF segment file size exceeds memory size");
    if (!Fits(image, offset, filesz)) return Fail("ELF segment data out of bounds");
    if (paddr < x86::kHighMemStart) return Fail("ELF segment loads below 1 MiB");
    kernel.segments.push_back({paddr, image.subspan(offset, filesz), memsz});
    kernel.kernel_end = std::max(kernel.kernel_end, paddr + memsz);
  }
  if (kernel.segments.empty()) return Fail("ELF kernel has no loadable segments");

  const bool entry_in_segment = std::any_of(kernel.segments.begin(), kernel.segments.end(), [&](const LoadSegment& s) {
    return entry >= s.guest_addr && entry < s.guest_addr + s.mem_size;
  });
  if (!entry_in_segment) return Fail("ELF entry point is outside every loadable segment");
  return kernel;
}

Result<KernelImage> ParseBzImage(std::span<const std::byte> image) {
  namespace bp = x86::bp;
  const auto version = LoadLe<std::uint16_t>(image, bp::kVersion);
  if (version < 0x020c) return Fail("bzImage boot protocol < 2.12 has no 64-bit entry");
  if (!(LoadLe<std::uint16_t>(image, bp::kXLoadFlags) & bp::kXlfKernel64)) {
    return Fail("bzImage does not advertise a 64-bit entry point");
  }
  if (!(LoadLe<std::uint8_t>(image, bp::kLoadFlags) & bp::kLoadedHigh)) {
    return Fail("bzImage is not a high-loaded kernel");
  }

  const std::size_t header_end = bp::kJump + 2 + std::to_integer<std::size_t>(image[bp::kJump + 1]);
  if (header_end <= bp::kInitSize + 4 || header_end > bp::kSetupHeaderLimit) {
    return Fail("bzImage setup header has an unexpected length");
  }

  std::size_t setup_sects = std::to_integer<std::size_t>(image[bp::kSetupSects]);
  if (setup_sects == 0) setup_sects = 4;
  const std::size_t pm_offset = (setup_sects + 1) * 512;
  if (pm_offset >= image.size()) return Fail("bzImage has no protected-mode payload");

  KernelImage kernel;
  kernel.format = KernelFormat::kBzImage;
  kernel.boot_protocol = version;
  kernel.setup_header = image.subspan(bp::kSetupHeader, header_end - bp::kSetupHeader);
  kernel.entry = x86::kBzImageLoadAddr + x86::kBzImage64BitEntryOffset;
  kernel.cmdline_max = LoadLe<std::uint32_t>(image, bp::kCmdlineSize);
  kernel.initrd_addr_max = LoadLe<std::uint32_t>(image, bp::kInitrdAddrMax);

  const auto payload = image.subspan(pm_offset);
  kernel.segments.push_back({x86::kBzImageLoadAddr, payload, payload.size()});

  const auto pref_address = LoadLe<std::uint64_t>(image, bp::kPrefAddress);
  const auto init_size = LoadLe<std::uint32_t>(image, bp::kInitSize);
  const std::uint64_t runtime_start = std::max<std::uint64_t>(x86::kBzImageLoadAddr, pref_address);
  kernel.kernel_end = std::max<std::uint64_t>(x86::kBzImageLoadAddr + payload.size(), runtime_start + init_size);
  return kernel;
}

}  // namespace

std::string_view KernelFormatName(KernelFormat format) {
  return format == KernelFormat::kElf ? "elf-vmlinux" : "bzImage";
}

Result<KernelImage> ParseKernel(std::span<const std::byte> image) {
  if (IsElf(image)) return ParseElf(image);
  if (IsBzImage(image)) return ParseBzImage(image);
  return Fail("unrecognized kernel image (expected ELF vmlinux or bzImage)");
}

}  // namespace tvmm
