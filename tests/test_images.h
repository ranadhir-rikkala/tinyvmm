#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "common/bytes.h"

namespace tvmm::testing {

struct ElfSegment {
  std::uint32_t type = 1;
  std::uint64_t paddr = 0;
  std::vector<std::byte> data;
  std::uint64_t mem_size = 0;
};

struct ElfSpec {
  std::uint16_t machine = 62;
  std::uint16_t type = 2;
  std::uint64_t entry = 0x1000000;
  std::vector<ElfSegment> segments;
};

inline std::vector<std::byte> Pattern(std::size_t n, std::uint8_t seed) {
  std::vector<std::byte> out(n);
  for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::byte>(seed + i * 7);
  return out;
}

inline std::vector<std::byte> MakeElf(const ElfSpec& spec) {
  constexpr std::size_t kEhdr = 64;
  constexpr std::size_t kPhdr = 56;
  std::size_t data_offset = kEhdr + kPhdr * spec.segments.size();
  std::size_t total = data_offset;
  for (const auto& s : spec.segments) total += s.data.size();

  std::vector<std::byte> img(total);
  std::span<std::byte> out(img);
  out[0] = std::byte{0x7f};
  out[1] = std::byte{'E'};
  out[2] = std::byte{'L'};
  out[3] = std::byte{'F'};
  out[4] = std::byte{2};
  out[5] = std::byte{1};
  out[6] = std::byte{1};
  StoreLe<std::uint16_t>(out, 16, spec.type);
  StoreLe<std::uint16_t>(out, 18, spec.machine);
  StoreLe<std::uint32_t>(out, 20, 1);
  StoreLe<std::uint64_t>(out, 24, spec.entry);
  StoreLe<std::uint64_t>(out, 32, kEhdr);
  StoreLe<std::uint16_t>(out, 52, kEhdr);
  StoreLe<std::uint16_t>(out, 54, kPhdr);
  StoreLe<std::uint16_t>(out, 56, static_cast<std::uint16_t>(spec.segments.size()));

  for (std::size_t i = 0; i < spec.segments.size(); ++i) {
    const auto& s = spec.segments[i];
    const std::size_t ph = kEhdr + i * kPhdr;
    StoreLe<std::uint32_t>(out, ph + 0, s.type);
    StoreLe<std::uint32_t>(out, ph + 4, 5);
    StoreLe<std::uint64_t>(out, ph + 8, data_offset);
    StoreLe<std::uint64_t>(out, ph + 16, s.paddr | 0xffffffff80000000ULL);
    StoreLe<std::uint64_t>(out, ph + 24, s.paddr);
    StoreLe<std::uint64_t>(out, ph + 32, s.data.size());
    StoreLe<std::uint64_t>(out, ph + 40, s.mem_size);
    StoreLe<std::uint64_t>(out, ph + 48, 0x200000);
    std::copy(s.data.begin(), s.data.end(), img.begin() + static_cast<std::ptrdiff_t>(data_offset));
    data_offset += s.data.size();
  }
  return img;
}

struct BzImageSpec {
  std::uint8_t setup_sects = 0;
  std::uint16_t version = 0x020f;
  std::uint16_t xloadflags = 0x0001;
  std::uint8_t loadflags = 0x01;
  std::uint32_t cmdline_size = 2047;
  std::uint32_t initrd_addr_max = 0x7fffffff;
  std::uint64_t pref_address = 0x1000000;
  std::uint32_t init_size = 0x800000;
  std::size_t payload_size = 8192;
};

inline std::vector<std::byte> MakeBzImage(const BzImageSpec& spec) {
  const std::size_t sects = spec.setup_sects == 0 ? 4 : spec.setup_sects;
  const std::size_t pm_offset = (sects + 1) * 512;
  std::vector<std::byte> img(pm_offset + spec.payload_size);
  std::span<std::byte> out(img);
  out[0x1f1] = std::byte{spec.setup_sects};
  StoreLe<std::uint16_t>(out, 0x1fe, 0xaa55);
  out[0x200] = std::byte{0xeb};
  out[0x201] = std::byte{0x6a};
  StoreLe<std::uint32_t>(out, 0x202, 0x53726448);
  StoreLe<std::uint16_t>(out, 0x206, spec.version);
  out[0x211] = std::byte{spec.loadflags};
  StoreLe<std::uint32_t>(out, 0x22c, spec.initrd_addr_max);
  StoreLe<std::uint32_t>(out, 0x230, 0x200000);
  StoreLe<std::uint16_t>(out, 0x236, spec.xloadflags);
  StoreLe<std::uint32_t>(out, 0x238, spec.cmdline_size);
  StoreLe<std::uint64_t>(out, 0x258, spec.pref_address);
  StoreLe<std::uint32_t>(out, 0x260, spec.init_size);
  auto payload = Pattern(spec.payload_size, 0x42);
  std::copy(payload.begin(), payload.end(), img.begin() + static_cast<std::ptrdiff_t>(pm_offset));
  return img;
}

}  // namespace tvmm::testing
