#pragma once

// Semantic analysis: AST -> HIR (docs/architecture.md#frontend). Builds the blocks from the flat
// statement list (structure test), determines the type of every variable and expression, resolves
// procedures, functions and labels, and inserts conversions.

#include "diagnostic.hpp"
#include "frontend/ast.hpp"
#include "ir/hir.hpp"

namespace basic {

hir::Program analyze(const ast::Program& program, Diagnostics& diags);

}  // namespace basic
