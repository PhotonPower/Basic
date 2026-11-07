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
#include "backend/cpp/cpp_backend.hpp"
#include "backend/m68k/m68k_backend.hpp"
#include "ir/interp.hpp"
#include "ir/lower.hpp"
#include "link/embedded.hpp"

#ifdef BASIC_HAVE_M68K_ASSEMBLER
#include "m68k/assembler.hpp"
#include "m68k/hunk.hpp"
#endif

namespace {

void usage(std::ostream& out) {
    out << "basicc " << basic::kVersion << " - GFA-BASIC 3.5 compiler\n\n"
        << "usage:\n"
        << "  basicc --target <name> <program.lst> [-o <output>]   compile: pc -> C++ source, amiga ->\n"
        << "                                                        AmigaDOS executable (-o x.s: assembler\n"
        << "                                                        source), m68k-test -> assembler source\n"
        << "  basicc --targets                                      list targets and their command groups\n"
        << "  basicc --emit-ast <program.lst>                       print the syntax tree\n"
        << "  basicc --emit-hir <program.lst>                       print the typed intermediate code\n"
        << "  basicc --emit-lir <program.lst>                       print the three-address code\n"
        << "  basicc --run <program.lst> [--seed <n>]               run with the reference interpreter\n"
        << "  basicc --version\n"
        << "  basicc --help\n\n"
        << "Planned debugging options: --emit-asm, --map\n"
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

int run(const std::string& path, std::uint32_t seed) {
    std::string source;
    if (!readFile(path, source)) {
        std::cerr << "basicc: cannot read '" << path << "'\n";
        return 2;
    }
    basic::Diagnostics diags;
    const basic::ast::Program program = basic::parseProgram(source, diags);
    basic::hir::Program h;
    if (diags.empty()) h = basic::analyze(program, diags);
    for (const basic::Diagnostic& d : diags) std::cerr << basic::formatDiagnostic(path, d) << "\n";
    if (!diags.empty()) return 1;
    basic::RunOptions options;
    options.seed = seed;
    const basic::RunResult r = basic::interpret(h, options);
    std::cout << r.output;
    if (!r.ok) {
        std::cerr << path << ":" << r.line << ": runtime error";
        if (r.error >= 0) std::cerr << " " << r.error;
        std::cerr << ": " << r.message << "\n";
        return 1;
    }
    return 0;
}

// Front end of all compile modes: parse and analyse, print diagnostics.
bool frontEnd(const std::string& path, basic::hir::Program& out) {
    std::string source;
    if (!readFile(path, source)) {
        std::cerr << "basicc: cannot read '" << path << "'\n";
        return false;
    }
    basic::Diagnostics diags;
    const basic::ast::Program program = basic::parseProgram(source, diags);
    if (diags.empty()) out = basic::analyze(program, diags);
    for (const basic::Diagnostic& d : diags) std::cerr << basic::formatDiagnostic(path, d) << "\n";
    return diags.empty();
}

int compilePc(const std::string& path, std::string output) {
    basic::hir::Program h;
    if (!frontEnd(path, h)) return 1;
    if (output.empty()) {
        output = path;
        const auto dot = output.rfind('.');
        if (dot != std::string::npos && output.find('/', dot) == std::string::npos) output.resize(dot);
        output += ".cpp";
    }
    std::string name = path;
    if (const auto slash = name.rfind('/'); slash != std::string::npos) name = name.substr(slash + 1);
    std::ofstream out(output, std::ios::binary);
    if (!out) {
        std::cerr << "basicc: cannot write '" << output << "'\n";
        return 2;
    }
    out << basic::generateCpp(h, name);
    return 0;
}

// MC68000 (m68k-test, amiga): lowering, backend, source-level linker. m68k-test writes the
// assembler source (ORG $1000); amiga writes an AmigaDOS executable (hunk file, assembled with the
// assembler of PhotonPower/MC68000), or the source if the output name ends in ".s".
int compileM68k(const std::string& path, std::string output, bool amiga) {
    basic::hir::Program h;
    if (!frontEnd(path, h)) return 1;
    basic::Diagnostics diags;
    const std::string source = basic::buildM68kProgram(h, amiga ? basic::M68kTarget::Amiga : basic::M68kTarget::Test, diags);
    if (diags.empty()) {
        if (output.empty()) {
            output = path;
            const auto dot = output.rfind('.');
            if (dot != std::string::npos && output.find('/', dot) == std::string::npos) output.resize(dot);
            if (!amiga) output += ".s";
        }
        const bool asSource = !amiga || (output.size() > 2 && output.compare(output.size() - 2, 2, ".s") == 0);
        std::vector<std::uint8_t> bytes(source.begin(), source.end());
        if (!asSource) {
#ifdef BASIC_HAVE_M68K_ASSEMBLER
            const m68k::AssemblyResult assembled = m68k::assemble(source, m68k::AssemblerOptions{});
            if (!assembled.ok()) {
                for (const m68k::Diagnostic& d : assembled.errors)
                    std::cerr << "basicc: internal error, assembler line " << d.line << ": " << d.message << "\n";
                return 1;
            }
            bytes = m68k::writeHunkFile(assembled.sections);
#else
            std::cerr << "basicc: built without the MC68000 assembler (BASIC_WITH_EMULATORS=OFF); use -o name.s\n";
            return 2;
#endif
        }
        std::ofstream out(output, std::ios::binary);
        if (!out) {
            std::cerr << "basicc: cannot write '" << output << "'\n";
            return 2;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    for (const basic::Diagnostic& d : diags) std::cerr << basic::formatDiagnostic(path, d) << "\n";
    return diags.empty() ? 0 : 1;
}

int emitLir(const std::string& path) {
    basic::hir::Program h;
    if (!frontEnd(path, h)) return 1;
    basic::Diagnostics diags;
    const basic::lir::Program l = basic::lower(h, diags);
    std::cout << basic::lir::dump(l);
    for (const basic::Diagnostic& d : diags) std::cerr << basic::formatDiagnostic(path, d) << "\n";
    return diags.empty() ? 0 : 1;
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
    if (args[0] == "--emit-lir") {
        if (args.size() != 2) {
            usage(std::cerr);
            return 2;
        }
        return emitLir(args[1]);
    }
    if (args[0] == "--run") {
        if (args.size() != 2 && !(args.size() == 4 && args[2] == "--seed")) {
            usage(std::cerr);
            return 2;
        }
        return run(args[1], args.size() == 4 ? static_cast<std::uint32_t>(std::stoul(args[3])) : 1u);
    }
    std::string target, input, output;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--target" && i + 1 < args.size()) target = args[++i];
        else if (args[i] == "-o" && i + 1 < args.size()) output = args[++i];
        else if (input.empty() && args[i].rfind("-", 0) != 0) input = args[i];
        else {
            usage(std::cerr);
            return 2;
        }
    }
    if (target.empty() || input.empty()) {
        usage(std::cerr);
        return 2;
    }
    if (!basic::findTarget(target)) {
        std::cerr << "basicc: unknown target '" << target << "' (see basicc --targets)\n";
        return 2;
    }
    if (target == "pc") return compilePc(input, output);
    if (target == "m68k-test" || target == "amiga") return compileM68k(input, output, target == "amiga");
    std::cerr << "basicc: target '" << target << "' is not implemented yet (see docs/roadmap.md)\n";
    return 2;
}
