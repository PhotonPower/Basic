#pragma once

// C++ backend (target pc): HIR -> readable C++ source that is compiled together with runtime/ref
// and platform/pc (docs/architecture.md#backends).

#include <string>

#include "ir/hir.hpp"

namespace basic {

// `sourceName` appears in the header comment and in runtime error messages.
std::string generateCpp(const hir::Program& program, const std::string& sourceName);

}  // namespace basic
