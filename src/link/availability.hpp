#pragma once

// Command groups by the runtime (D7, docs/decisions.md#d7): before linking, every runtime routine the
// program calls is checked against the target's library, including the routines it needs in turn.
// A statement that needs a routine the target lacks is reported with its line, the BASIC command and
// the command group of the module that provides the routine on another target.

#include <string_view>

#include "frontend/diagnostic.hpp"
#include "ir/lir.hpp"
#include "link/linker.hpp"

namespace basic::link {

// `code`: the generated program (its symbols count as available; routines the backend generated inline
// are not in its uses and not checked). `reference`: the library of a target with the full runtime
// (amiga), only used to name the group of a routine the target has no module for. One message per
// line and command.
void checkAvailability(const lir::Program& program, const Program& code, const Library& lib,
                       const Library& reference, std::string_view target, Diagnostics& diags);

}  // namespace basic::link
