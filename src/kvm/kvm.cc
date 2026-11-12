#include "kvm/kvm.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <cerrno>
#include <string>

namespace tvmm {

namespace {

template <typename Arg>
int Ioctl(int fd, unsigned long request, Arg arg) {
  int r;
  do {
    r = ::ioctl(fd, request, arg);
  } while (r < 0 && errno == EINTR);
  return r;
}

}  // namespace

#if defined(__x86_64__)
CpuidTable::CpuidTable(std::size_t max_entries)
    : storage_((sizeof(kvm_cpuid2) + max_entries * sizeof(kvm_cpuid_entry2) + sizeof(std::uint64_t) - 1) /
                   sizeof(std::uint64_t),
               0) {
  raw()->nent = static_cast<std::uint32_t>(max_entries);
}
#endif

Result<Kvm> Kvm::Open(const char* path) {
  UniqueFd fd(::open(path, O_RDWR | O_CLOEXEC));
  if (!fd.valid()) return FailErrno(std::string("open ") + path);

  const int version = Ioctl(fd.get(), KVM_GET_API_VERSION, 0);
  if (version < 0) return FailErrno("KVM_GET_API_VERSION");
  if (version != KVM_API_VERSION) return Fail("unsupported KVM API version " + std::to_string(version));

  if (Ioctl(fd.get(), KVM_CHECK_EXTENSION, KVM_CAP_USER_MEMORY) <= 0) {
    return Fail("KVM lacks KVM_CAP_USER_MEMORY");
  }
  const int mmap_size = Ioctl(fd.get(), KVM_GET_VCPU_MMAP_SIZE, 0);
  if (mmap_size <= 0) return FailErrno("KVM_GET_VCPU_MMAP_SIZE");
  return Kvm(std::move(fd), static_cast<std::size_t>(mmap_size));
}

Result<int> Kvm::CheckExtension(long capability) const {
  const int r = Ioctl(fd_.get(), KVM_CHECK_EXTENSION, capability);
  if (r < 0) return FailErrno("KVM_CHECK_EXTENSION");
  return r;
}

Result<Vm> Kvm::CreateVm() const {
  UniqueFd vm(Ioctl(fd_.get(), KVM_CREATE_VM, 0));
  if (!vm.valid()) return FailErrno("KVM_CREATE_VM");
  return Vm(std::move(vm), vcpu_mmap_size_);
}

#if defined(__x86_64__)
Result<CpuidTable> Kvm::SupportedCpuid() const {
  constexpr std::size_t kMaxEntries = 256;
  CpuidTable table(kMaxEntries);
  if (Ioctl(fd_.get(), KVM_GET_SUPPORTED_CPUID, table.raw()) < 0) return FailErrno("KVM_GET_SUPPORTED_CPUID");
  return table;
}
#endif

Status Vm::SetUserMemoryRegion(std::uint32_t slot, std::uint64_t guest_phys_addr, std::uint64_t size, void* host) {
  kvm_userspace_memory_region region{};
  region.slot = slot;
  region.guest_phys_addr = guest_phys_addr;
  region.memory_size = size;
  region.userspace_addr = reinterpret_cast<std::uint64_t>(host);
  if (Ioctl(fd_.get(), KVM_SET_USER_MEMORY_REGION, &region) < 0) return FailErrno("KVM_SET_USER_MEMORY_REGION");
  return {};
}

Result<Vcpu> Vm::CreateVcpu(unsigned long id) {
  UniqueFd fd(Ioctl(fd_.get(), KVM_CREATE_VCPU, id));
  if (!fd.valid()) return FailErrno("KVM_CREATE_VCPU");
  auto run = Mapping::Create(vcpu_mmap_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd.get());
  if (!run) return Propagate(run.error(), "kvm_run mapping");
  return Vcpu(std::move(fd), std::move(*run));
}

Status Vm::IrqLine(std::uint32_t irq, bool level) {
  kvm_irq_level irq_level{};
  irq_level.irq = irq;
  irq_level.level = level ? 1 : 0;
  if (Ioctl(fd_.get(), KVM_IRQ_LINE, &irq_level) < 0) return FailErrno("KVM_IRQ_LINE");
  return {};
}

#if defined(__x86_64__)
Status Vm::SetTssAddr(std::uint64_t addr) {
  if (Ioctl(fd_.get(), KVM_SET_TSS_ADDR, static_cast<unsigned long>(addr)) < 0) return FailErrno("KVM_SET_TSS_ADDR");
  return {};
}

Status Vm::SetIdentityMapAddr(std::uint64_t addr) {
  __u64 value = addr;
  if (Ioctl(fd_.get(), KVM_SET_IDENTITY_MAP_ADDR, &value) < 0) return FailErrno("KVM_SET_IDENTITY_MAP_ADDR");
  return {};
}

Status Vm::CreateIrqchip() {
  if (Ioctl(fd_.get(), KVM_CREATE_IRQCHIP, 0) < 0) return FailErrno("KVM_CREATE_IRQCHIP");
  return {};
}

Status Vm::CreatePit2() {
  kvm_pit_config config{};
  config.flags = KVM_PIT_SPEAKER_DUMMY;
  if (Ioctl(fd_.get(), KVM_CREATE_PIT2, &config) < 0) return FailErrno("KVM_CREATE_PIT2");
  return {};
}
#endif

Status Vcpu::Run() {
  if (::ioctl(fd_.get(), KVM_RUN, 0) < 0) return FailErrno("KVM_RUN");
  return {};
}

#if defined(__x86_64__)
Result<kvm_regs> Vcpu::GetRegs() const {
  kvm_regs regs{};
  if (Ioctl(fd_.get(), KVM_GET_REGS, &regs) < 0) return FailErrno("KVM_GET_REGS");
  return regs;
}

Status Vcpu::SetRegs(const kvm_regs& regs) {
  if (Ioctl(fd_.get(), KVM_SET_REGS, &regs) < 0) return FailErrno("KVM_SET_REGS");
  return {};
}

Result<kvm_sregs> Vcpu::GetSregs() const {
  kvm_sregs sregs{};
  if (Ioctl(fd_.get(), KVM_GET_SREGS, &sregs) < 0) return FailErrno("KVM_GET_SREGS");
  return sregs;
}

Status Vcpu::SetSregs(const kvm_sregs& sregs) {
  if (Ioctl(fd_.get(), KVM_SET_SREGS, &sregs) < 0) return FailErrno("KVM_SET_SREGS");
  return {};
}

Status Vcpu::SetFpu(const kvm_fpu& fpu) {
  if (Ioctl(fd_.get(), KVM_SET_FPU, &fpu) < 0) return FailErrno("KVM_SET_FPU");
  return {};
}

Status Vcpu::SetCpuid(CpuidTable& cpuid) {
  if (Ioctl(fd_.get(), KVM_SET_CPUID2, cpuid.raw()) < 0) return FailErrno("KVM_SET_CPUID2");
  return {};
}

Result<kvm_lapic_state> Vcpu::GetLapic() const {
  kvm_lapic_state lapic{};
  if (Ioctl(fd_.get(), KVM_GET_LAPIC, &lapic) < 0) return FailErrno("KVM_GET_LAPIC");
  return lapic;
}

Status Vcpu::SetLapic(const kvm_lapic_state& lapic) {
  if (Ioctl(fd_.get(), KVM_SET_LAPIC, &lapic) < 0) return FailErrno("KVM_SET_LAPIC");
  return {};
}
#endif

}  // namespace tvmm
