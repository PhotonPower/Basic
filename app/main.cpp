// basicc - command line front end of the GFA-BASIC compiler.
//
// Only the target registry exists so far; compiling is the next milestone (docs/roadmap.md).

#include <iostream>
#include <string>
#include <vector>

#include "basic/target.hpp"
#include "basic/version.hpp"

namespace {

void usage(std::ostream& out) {
    out << "basicc " << basic::kVersion << " - GFA-BASIC 3.5 compiler\n\n"
        << "usage:\n"
        << "  basicc --target <name> <program.lst> [-o <output>]   compile (not implemented yet)\n"
        << "  basicc --targets                                      list targets and their command groups\n"
        << "  basicc --version\n"
        << "  basicc --help\n\n"
        << "Planned debugging options: --emit-ast, --emit-hir, --emit-lir, --emit-asm, --map\n"
        << "(see docs/usage.md).\n";
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        usage(std::cout);
        return args.empty() ? 1 : 0;
    }
    if (args[0] == "--version") {
        std::cout << "basicc " << basic::kVersion << "\n";
        return 0;
    }
    if (args[0] == "--targets") {
        std::cout << basic::formatTargets();
        return 0;
    }
    std::string target;
    for (std::size_t i = 0; i < args.size(); ++i)
        if (args[i] == "--target" && i + 1 < args.size()) target = args[i + 1];
    if (!target.empty() && !basic::findTarget(target)) {
        std::cerr << "basicc: unknown target '" << target << "' (see basicc --targets)\n";
        return 2;
    }
    std::cerr << "basicc: compiling is not implemented yet (see docs/roadmap.md)\n";
    return 2;
}
