#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "common/result.h"

namespace tvmm {

Result<std::vector<std::byte>> ReadFile(const std::string& path);

}  // namespace tvmm
