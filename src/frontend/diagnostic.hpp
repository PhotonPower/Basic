#pragma once

#include <string>
#include <vector>

namespace basic {

// A message about the source program, with a 1-based line and column (0 = unknown).
struct Diagnostic {
    int line = 0;
    int col = 0;
    std::string message;
};

using Diagnostics = std::vector<Diagnostic>;

std::string formatDiagnostic(const std::string& file, const Diagnostic& d);

}  // namespace basic
