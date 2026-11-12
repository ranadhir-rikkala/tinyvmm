#pragma once

#include "common/result.h"

namespace tvmm {

enum class SmokeMode { kReal, kLong };

Result<int> RunSmoke(SmokeMode mode);

}  // namespace tvmm
