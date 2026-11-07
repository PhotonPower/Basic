#pragma once

// MC68000 backend: LIR -> Motorola assembler source for the assembler of PhotonPower/MC68000.
// First version: every virtual register lives in an 8-byte slot of the stack frame (LINK A6);
// operations go through D0-D3. Register allocation is a later optimisation.

#include <string>
#include <vector>

#include "frontend/diagnostic.hpp"
#include "ir/lir.hpp"
#include "link/linker.hpp"

namespace basic {

// The program part for the linker: code, data (string constants), BSS (variables) and the runtime
// symbols it calls.
link::Program generateM68k(const lir::Program& program);

enum class M68kTarget { Test, Amiga };

// The whole MC68000 pipeline after the frontend: lowering, backend, source-level linker with the
// embedded runtime and platform modules. Returns the linked assembler source (m68k-test: one ORG
// $1000 program; amiga: SECTION code/data/bss for a hunk file); problems go to `diags` (features
// the backend does not support yet are reported by the lowering).
std::string buildM68kProgram(const hir::Program& program, M68kTarget target, Diagnostics& diags);

}  // namespace basic
