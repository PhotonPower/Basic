#pragma once

// MC68000 backend: LIR -> Motorola assembler source for the assembler of PhotonPower/MC68000.
// First version: every virtual register lives in an 8-byte slot of the stack frame (LINK A6);
// operations go through D0-D3. Register allocation is a later optimisation.

#include <string>
#include <vector>

#include "ir/lir.hpp"
#include "link/linker.hpp"

namespace basic {

// The program part for the linker: code, data (string constants), BSS (variables) and the runtime
// symbols it calls.
link::Program generateM68k(const lir::Program& program);

}  // namespace basic
