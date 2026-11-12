#pragma once

#include <linux/kvm.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "common/mapping.h"
#include "common/result.h"
#include "common/unique_fd.h"

namespace tvmm {

#if defined(__x86_64__)
class CpuidTable {
 public:
  explicit CpuidTable(std::size_t max_entries);

  kvm_cpuid2* raw() { return reinterpret_cast<kvm_cpuid2*>(storage_.data()); }
  const kvm_cpuid2* raw() const { return reinterpret_cast<const kvm_cpuid2*>(storage_.data()); }
  std::span<kvm_cpuid_entry2> entries() { return {raw()->entries, raw()->nent}; }

 private:
  std::vector<std::uint64_t> storage_;
};
#endif

class Vcpu {
 public:
  int fd() const { return fd_.get(); }
  kvm_run* run_state() const { return static_cast<kvm_run*>(run_.data()); }

  Status Run();

#if defined(__x86_64__)
  Result<kvm_regs> GetRegs() const;
  Status SetRegs(const kvm_regs& regs);
  Result<kvm_sregs> GetSregs() const;
  Status SetSregs(const kvm_sregs& sregs);
  Status SetFpu(const kvm_fpu& fpu);
  Status SetCpuid(CpuidTable& cpuid);
  Result<kvm_lapic_state> GetLapic() const;
  Status SetLapic(const kvm_lapic_state& lapic);
#endif

 private:
  friend class Vm;
  Vcpu(UniqueFd fd, Mapping run) : fd_(std::move(fd)), run_(std::move(run)) {}

  UniqueFd fd_;
  Mapping run_;
};

class Vm {
 public:
  int fd() const { return fd_.get(); }

  Status SetUserMemoryRegion(std::uint32_t slot, std::uint64_t guest_phys_addr, std::uint64_t size, void* host);
  Result<Vcpu> CreateVcpu(unsigned long id);
  Status IrqLine(std::uint32_t irq, bool level);

#if defined(__x86_64__)
  Status SetTssAddr(std::uint64_t addr);
  Status SetIdentityMapAddr(std::uint64_t addr);
  Status CreateIrqchip();
  Status CreatePit2();
#endif

 private:
  friend class Kvm;
  Vm(UniqueFd fd, std::size_t vcpu_mmap_size) : fd_(std::move(fd)), vcpu_mmap_size_(vcpu_mmap_size) {}

  UniqueFd fd_;
  std::size_t vcpu_mmap_size_;
};

class Kvm {
 public:
  static Result<Kvm> Open(const char* path = "/dev/kvm");

  int fd() const { return fd_.get(); }
  Result<int> CheckExtension(long capability) const;
  Result<Vm> CreateVm() const;

#if defined(__x86_64__)
  Result<CpuidTable> SupportedCpuid() const;
#endif

 private:
  Kvm(UniqueFd fd, std::size_t vcpu_mmap_size) : fd_(std::move(fd)), vcpu_mmap_size_(vcpu_mmap_size) {}

  UniqueFd fd_;
  std::size_t vcpu_mmap_size_;
};

}  // namespace tvmm
