#pragma once

#include <cstdint>

#include "boot/linux_loader.h"
#include "common/result.h"
#include "kvm/kvm.h"

namespace tvmm::x86 {

Status ConfigureCpuid(const Kvm& kvm, Vcpu& vcpu, std::uint32_t vcpu_id);
Status SetupRealMode(Vcpu& vcpu, std::uint64_t entry);
Status SetupLongMode(Vcpu& vcpu, const LongModeTables& tables, std::uint64_t entry, std::uint64_t boot_params);
Status SetupFpu(Vcpu& vcpu);
Status SetupLapicLint(Vcpu& vcpu);

}  // namespace tvmm::x86
