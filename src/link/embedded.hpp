#pragma once

// The runtime and platform modules, built into the compiler (cmake/embed_modules.cmake).

#include <string>
#include <vector>

#include "frontend/diagnostic.hpp"
#include "link/linker.hpp"

namespace basic::link {

struct EmbeddedModule {
    const char* dir;   // e.g. "runtime/m68k"
    const char* name;  // file name
    const char* text;
};

const std::vector<EmbeddedModule>& embeddedModules();

// Parses the embedded modules of the given directories.
std::vector<Module> embeddedModulesOf(const std::vector<std::string>& dirs, Diagnostics& diags);

}  // namespace basic::link
