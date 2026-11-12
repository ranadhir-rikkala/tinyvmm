#include "kvm/x86_cpu.h"

#include <cstring>

#include "x86/gdt.h"
#include "x86/layout.h"

namespace tvmm::x86 {

namespace {

constexpr std::uint64_t kCr0Pe = 1ULL << 0;
constexpr std::uint64_t kCr0Et = 1ULL << 4;
constexpr std::uint64_t kCr0Pg = 1ULL << 31;
constexpr std::uint64_t kCr4Pae = 1ULL << 5;
constexpr std::uint64_t kEferLme = 1ULL << 8;
constexpr std::uint64_t kEferLma = 1ULL << 10;
constexpr std::uint64_t kRflagsReserved = 1ULL << 1;
constexpr std::uint32_t kCpuidHypervisorBit = 1U << 31;

constexpr std::size_t kApicLvt0 = 0x350;
constexpr std::size_t kApicLvt1 = 0x360;
constexpr std::uint32_t kApicModeNmi = 0x4;
constexpr std::uint32_t kApicModeExtInt = 0x7;

kvm_segment ToKvmSegment(const Segment& s) {
  kvm_segment k{};
  k.base = s.base;
  k.limit = s.limit;
  k.selector = s.selector;
  k.type = s.type;
  k.present = s.present;
  k.dpl = s.dpl;
  k.db = s.db;
  k.s = s.s;
  k.l = s.l;
  k.g = s.g;
  k.avl = s.avl;
  k.unusable = s.present ? 0 : 1;
  return k;
}

std::uint32_t ReadApicReg(const kvm_lapic_state& lapic, std::size_t offset) {
  std::uint32_t value;
  std::memcpy(&value, lapic.regs + offset, sizeof(value));
  return value;
}

void WriteApicReg(kvm_lapic_state& lapic, std::size_t offset, std::uint32_t value) {
  std::memcpy(lapic.regs + offset, &value, sizeof(value));
}

std::uint32_t WithDeliveryMode(std::uint32_t lvt, std::uint32_t mode) { return (lvt & ~0x700U) | (mode << 8); }

}  // namespace

Status ConfigureCpuid(const Kvm& kvm, Vcpu& vcpu, std::uint32_t vcpu_id) {
  auto cpuid = kvm.SupportedCpuid();
  if (!cpuid) return Propagate(cpuid.error());
  for (auto& entry : cpuid->entries()) {
    if (entry.function == 1) {
      entry.ebx = (entry.ebx & 0x00ffffffU) | (vcpu_id << 24);
      entry.ecx |= kCpuidHypervisorBit;
    }
  }
  return vcpu.SetCpuid(*cpuid);
}

Status SetupRealMode(Vcpu& vcpu, std::uint64_t entry) {
  auto sregs = vcpu.GetSregs();
  if (!sregs) return Propagate(sregs.error());
  for (kvm_segment* seg : {&sregs->cs, &sregs->ds, &sregs->es, &sregs->fs, &sregs->gs, &sregs->ss}) {
    seg->base = 0;
    seg->selector = 0;
  }
  if (auto s = vcpu.SetSregs(*sregs); !s) return s;

  kvm_regs regs{};
  regs.rip = entry;
  regs.rflags = kRflagsReserved;
  regs.rsp = kZeroPageAddr;
  return vcpu.SetRegs(regs);
}

Status SetupLongMode(Vcpu& vcpu, const LongModeTables& tables, std::uint64_t entry, std::uint64_t boot_params) {
  auto sregs = vcpu.GetSregs();
  if (!sregs) return Propagate(sregs.error());

  const kvm_segment code = ToKvmSegment(SegmentFromGdtEntry(tables.gdt[kBootCodeSelector >> 3], kBootCodeSelector));
  const kvm_segment data = ToKvmSegment(SegmentFromGdtEntry(tables.gdt[kBootDataSelector >> 3], kBootDataSelector));
  const kvm_segment tss = ToKvmSegment(SegmentFromGdtEntry(tables.gdt[kBootTssSelector >> 3], kBootTssSelector));

  sregs->cs = code;
  sregs->ds = data;
  sregs->es = data;
  sregs->fs = data;
  sregs->gs = data;
  sregs->ss = data;
  sregs->tr = tss;
  sregs->gdt.base = tables.gdt_addr;
  sregs->gdt.limit = tables.gdt_limit;
  sregs->idt.base = tables.idt_addr;
  sregs->idt.limit = tables.idt_limit;

  sregs->cr0 = kCr0Pe | kCr0Et | kCr0Pg;
  sregs->cr3 = tables.pml4;
  sregs->cr4 = kCr4Pae;
  sregs->efer = kEferLme | kEferLma;
  if (auto s = vcpu.SetSregs(*sregs); !s) return s;

  kvm_regs regs{};
  regs.rflags = kRflagsReserved;
  regs.rip = entry;
  regs.rsp = kBootStackTop;
  regs.rbp = kBootStackTop;
  regs.rsi = boot_params;
  return vcpu.SetRegs(regs);
}

Status SetupFpu(Vcpu& vcpu) {
  kvm_fpu fpu{};
  fpu.fcw = 0x37f;
  fpu.mxcsr = 0x1f80;
  return vcpu.SetFpu(fpu);
}

Status SetupLapicLint(Vcpu& vcpu) {
  auto lapic = vcpu.GetLapic();
  if (!lapic) return Propagate(lapic.error());
  WriteApicReg(*lapic, kApicLvt0, WithDeliveryMode(ReadApicReg(*lapic, kApicLvt0), kApicModeExtInt));
  WriteApicReg(*lapic, kApicLvt1, WithDeliveryMode(ReadApicReg(*lapic, kApicLvt1), kApicModeNmi));
  return vcpu.SetLapic(*lapic);
}

}  // namespace tvmm::x86
