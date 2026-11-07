#pragma once

// Lowering HIR -> LIR (docs/ir.md). The first version covers what the MC68000 backend supports:
// numeric variables, arithmetic, comparisons, IF, all loops, SELECT, procedures and functions with
// value parameters and LOCAL, GOTO, PRINT of numbers and string constants. Everything else is
// reported as not supported yet.

#include "frontend/diagnostic.hpp"
#include "ir/hir.hpp"
#include "ir/lir.hpp"

namespace basic {

lir::Program lower(const hir::Program& program, Diagnostics& diags);

}  // namespace basic
