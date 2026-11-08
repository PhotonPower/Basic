#pragma once

// Z80 backend: LIR -> assembler source for the assembler of PhotonPower/Z80 (D30). Every virtual
// register lives in an 8-byte slot of the stack frame (IX); runtime routines get pointers: numbers
// as the address of their slot, strings and addresses as the value itself (docs/runtime.md).

#include <string>

#include "frontend/diagnostic.hpp"
#include "ir/lir.hpp"
#include "link/linker.hpp"

namespace basic {

// The program part for the linker; features the backend does not support yet go to `diags`.
link::Program generateZ80(const lir::Program& program, Diagnostics& diags);

enum class Z80Target { Test, Cpc };

// The whole Z80 pipeline after the frontend: lowering, backend, source-level linker with the
// runtime and platform modules. Returns the linked assembler source (one ORG program).
std::string buildZ80Program(const hir::Program& program, Z80Target target, Diagnostics& diags);

}  // namespace basic
