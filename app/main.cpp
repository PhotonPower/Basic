// basicc - command line front end of the GFA-BASIC compiler.
//
// Implemented so far: the target registry and the parser (--emit-ast); compiling follows
// (docs/roadmap.md).

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "basic/target.hpp"
#include "basic/version.hpp"
#include "frontend/parser.hpp"
#include "frontend/sema.hpp"

namespace {

void usage(std::ostream& out) {
    out << "basicc " << basic::kVersion << " - GFA-BASIC 3.5 compiler\n\n"
        << "usage:\n"
        << "  basicc --target <name> <program.lst> [-o <output>]   compile (not implemented yet)\n"
        << "  basicc --targets                                      list targets and their command groups\n"
        << "  basicc --emit-ast <program.lst>                       print the syntax tree\n"
        << "  basicc --emit-hir <program.lst>                       print the typed intermediate code\n"
        << "  basicc --version\n"
        << "  basicc --help\n\n"
        << "Planned debugging options: --emit-lir, --emit-asm, --map\n"
        << "(see docs/usage.md).\n";
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

int emit(const std::string& path, bool hir) {
    std::string source;
    if (!readFile(path, source)) {
        std::cerr << "basicc: cannot read '" << path << "'\n";
        return 2;
    }
    basic::Diagnostics diags;
    const basic::ast::Program program = basic::parseProgram(source, diags);
    if (hir) {
        if (diags.empty()) {
            const basic::hir::Program h = basic::analyze(program, diags);
            if (diags.empty()) std::cout << basic::hir::dump(h);
        }
    } else {
        std::cout << basic::ast::dump(program);
    }
    for (const basic::Diagnostic& d : diags) std::cerr << basic::formatDiagnostic(path, d) << "\n";
    return diags.empty() ? 0 : 1;
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
    if (args[0] == "--emit-ast" || args[0] == "--emit-hir") {
        if (args.size() != 2) {
            usage(std::cerr);
            return 2;
        }
        return emit(args[1], args[0] == "--emit-hir");
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
