#pragma once

// Reference interpreter on the HIR (decision D17). All numerics go through runtime/ref, so its
// results are the reference for the compiled targets.

#include <cstdint>
#include <string>

#include "hir.hpp"

namespace basic {

struct RunOptions {
    std::uint32_t seed = 1;        // start value of the random generator (RANDOMIZE without value)
    std::uint64_t maxSteps = 0;    // stop after this many statements, 0 = no limit
};

struct RunResult {
    std::string output;   // everything the program printed
    bool ok = true;       // false: unhandled runtime error or step limit
    int error = -1;       // GFA error number of an unhandled error
    int line = 0;         // line of the failing statement
    std::string message;  // error text
};

RunResult interpret(const hir::Program& program, const RunOptions& options = {});

// Text of a GFA error number as ERR$ returns it (docs/language.md).
std::string errorText(int code);

}  // namespace basic
