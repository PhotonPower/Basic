// Differential test of the MC68000 target: every tests/programs/<name>.lst that the backend
// supports is compiled for m68k-test, run in the emulator of PhotonPower/MC68000 and its output
// compared with <name>.out (made on the original). BASIC_M68K_PROGRAM=<file> runs one program.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "backend/m68k/m68k_backend.hpp"
#include "frontend/parser.hpp"
#include "frontend/sema.hpp"
#include "ir/lower.hpp"
#include "link/embedded.hpp"
#include "m68k_runner.hpp"
#include "test_util.hpp"

using namespace basic;
namespace fs = std::filesystem;

namespace {

std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Compiles a program for m68k-test; empty result and `unsupported` set if the backend cannot yet.
std::string compile(const std::string& src, bool& unsupported, std::string& message) {
    Diagnostics d;
    const ast::Program a = parseProgram(src, d);
    const hir::Program h = analyze(a, d);
    if (!d.empty()) {
        message = d[0].message;
        return "";
    }
    const lir::Program l = lower(h, d);
    if (!d.empty()) {
        unsupported = true;
        message = d[0].message;
        return "";
    }
    const link::Program prog = generateM68k(l);
    std::vector<link::Module> mods = link::embeddedModulesOf({"runtime/m68k", "platform/m68k-test"}, d);
    const link::Library lib = link::makeLibrary(std::move(mods), d);
    const link::Result r = link::link(lib, prog, {"plat_start"}, link::Layout::Plain, "        ORG     $1000\n", d);
    if (!d.empty()) message = d[0].message;
    return r.source;
}

}  // namespace

int main() {
    const fs::path dir = fs::path(BASIC_SOURCE_DIR) / "tests" / "programs";
    int ran = 0, skipped = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".lst") continue;
        if (const char* only = std::getenv("BASIC_M68K_PROGRAM"); only && entry.path().filename() != only) continue;
        const std::string name = entry.path().stem().string();
        bool unsupported = false;
        std::string message;
        const std::string source = compile(read(entry.path()), unsupported, message);
        if (unsupported) {
            std::cout << name << ": skipped (" << message << ")\n";
            ++skipped;
            continue;
        }
        ++g_checks;
        if (source.empty()) {
            ++g_failures;
            std::cerr << name << ": " << message << "\n";
            continue;
        }
        harness::M68kOptions opt;
        opt.maxInstructions = 500'000'000;
        const harness::M68kRun run = harness::runM68kSource(source, harness::M68kPlatform::Test, opt);
        const std::string expected = read(fs::path(entry.path()).replace_extension(".out"));
        ++ran;
        if (!run.error.empty() || run.console != expected) {
            ++g_failures;
            std::cerr << name << ": " << run.error << "\n--- expected\n" << expected << "--- actual\n" << run.console << "\n";
        } else {
            std::cout << name << ": ok (" << run.instructions << " instructions)\n";
        }
    }
    std::cout << ran << " programs run, " << skipped << " skipped (not yet supported by the backend)\n";
    return report("m68k_programs");
}
