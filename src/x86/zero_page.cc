#include "x86/zero_page.h"

#include <algorithm>

namespace tvmm::x86 {

Status ZeroPage::CopySetupHeader(std::span<const std::byte> header) {
  if (header.size() > bp::kSetupHeaderLimit - bp::kSetupHeader) {
    return Fail("setup header too large for boot_params");
  }
  std::copy(header.begin(), header.end(), bytes_.begin() + bp::kSetupHeader);
  return {};
}

Status ZeroPage::SetE820(std::span<const E820Entry> entries) {
  if (entries.size() > kMaxE820Entries) return Fail("too many e820 entries");
  Set<std::uint8_t>(bp::kE820Entries, static_cast<std::uint8_t>(entries.size()));
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const std::size_t off = bp::kE820Table + i * bp::kE820EntrySize;
    Set<std::uint64_t>(off, entries[i].addr);
    Set<std::uint64_t>(off + 8, entries[i].size);
    Set<std::uint32_t>(off + 16, static_cast<std::uint32_t>(entries[i].type));
  }
  return {};
}

std::vector<E820Entry> ZeroPage::E820() const {
  std::vector<E820Entry> entries(Get<std::uint8_t>(bp::kE820Entries));
  for (std::size_t i = 0; i < entries.size(); ++i) {
    const std::size_t off = bp::kE820Table + i * bp::kE820EntrySize;
    entries[i].addr = Get<std::uint64_t>(off);
    entries[i].size = Get<std::uint64_t>(off + 8);
    entries[i].type = static_cast<E820Type>(Get<std::uint32_t>(off + 16));
  }
  return entries;
}

Result<ZeroPage> BuildZeroPage(const ZeroPageConfig& config) {
  ZeroPage zp;
  if (!config.setup_header.empty()) {
    if (auto s = zp.CopySetupHeader(config.setup_header); !s) return Propagate(s.error());
    if (zp.Get<std::uint32_t>(bp::kHeaderMagic) != bp::kHdrSMagic) {
      return Fail("setup header is missing the HdrS magic");
    }
  } else {
    zp.Set<std::uint16_t>(bp::kBootFlag, bp::kBootFlagMagic);
    zp.Set<std::uint32_t>(bp::kHeaderMagic, bp::kHdrSMagic);
    zp.Set<std::uint32_t>(bp::kKernelAlignment, bp::kDefaultKernelAlignment);
    zp.Set<std::uint32_t>(bp::kCmdlineSize, config.cmdline_size);
  }

  zp.Set<std::uint8_t>(bp::kTypeOfLoader, bp::kLoaderUndefined);
  zp.Set<std::uint32_t>(bp::kCmdLinePtr, static_cast<std::uint32_t>(config.cmdline_addr));
  zp.Set<std::uint32_t>(bp::kExtCmdLinePtr, static_cast<std::uint32_t>(config.cmdline_addr >> 32));

  if (config.initrd_size != 0) {
    zp.Set<std::uint32_t>(bp::kRamdiskImage, static_cast<std::uint32_t>(config.initrd_addr));
    zp.Set<std::uint32_t>(bp::kExtRamdiskImage, static_cast<std::uint32_t>(config.initrd_addr >> 32));
    zp.Set<std::uint32_t>(bp::kRamdiskSize, static_cast<std::uint32_t>(config.initrd_size));
    zp.Set<std::uint32_t>(bp::kExtRamdiskSize, static_cast<std::uint32_t>(config.initrd_size >> 32));
  }

  if (auto s = zp.SetE820(config.e820); !s) return Propagate(s.error());
  return zp;
}

}  // namespace tvmm::x86
