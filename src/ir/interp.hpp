#pragma once

// Reference interpreter on the HIR (decision D17). All numerics go through runtime/ref, so its
// results are the reference for the compiled targets.

#include <cstdint>
#include <iosfwd>
#include <string>

#include "hir.hpp"

namespace basic {

struct RunOptions {
    std::uint32_t seed = 1;        // start value of the random generator (RANDOMIZE without value)
    std::uint64_t maxSteps = 0;    // stop after this many statements, 0 = no limit
    std::istream* input = nullptr; // keyboard (INPUT, INKEY$); nullptr: no input
    std::ostream* live = nullptr;  // if set, the output goes here as it is printed (not into RunResult)
    std::string gfxPicture;        // if set and the program drew something: the screen as PPM (D25)
};

struct RunResult {
    std::string output;   // everything the program printed
    bool ok = true;       // false: unhandled runtime error or step limit
    int error = -1;       // GFA error number of an unhandled error
    int line = 0;         // line of the failing statement
    std::string message;  // error text
    std::string gfx;      // digest of the graphics (D25), empty if the program drew nothing
};

RunResult interpret(const hir::Program& program, const RunOptions& options = {});

// Text of a GFA error number as ERR$ returns it (docs/language.md).
std::string errorText(int code);

}  // namespace basic
