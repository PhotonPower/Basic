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

// Compiles a program for the MC68000 target; empty result and `unsupported` set if the backend
// cannot yet.
std::string compile(const std::string& src, M68kTarget target, bool& unsupported, std::string& message) {
    Diagnostics d;
    const ast::Program a = parseProgram(src, d);
    const hir::Program h = analyze(a, d);
    if (!d.empty()) {
        message = d[0].message;
        return "";
    }
    const std::string source = buildM68kProgram(h, target, d);
    if (!d.empty()) {
        unsupported = d[0].message.find("not supported") != std::string::npos;
        message = d[0].message;
    }
    return source;
}

}  // namespace

int main() {
    const fs::path dir = fs::path(BASIC_SOURCE_DIR) / "tests" / "programs";
    int ran = 0, skipped = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".lst") continue;
        if (const char* only = std::getenv("BASIC_M68K_PROGRAM"); only && entry.path().filename() != only) continue;
        const std::string name = entry.path().stem().string();
        const std::string expected = read(fs::path(entry.path()).replace_extension(".out"));
        for (const M68kTarget target : {M68kTarget::Test, M68kTarget::Amiga}) {
            const bool amiga = target == M68kTarget::Amiga;
            const std::string label = name + (amiga ? " (amiga)" : " (m68k-test)");
            bool unsupported = false;
            std::string message;
            const std::string source = compile(read(entry.path()), target, unsupported, message);
            if (unsupported) {
                std::cout << label << ": skipped (" << message << ")\n";
                ++skipped;
                continue;
            }
            ++g_checks;
            if (source.empty()) {
                ++g_failures;
                std::cerr << label << ": " << message << "\n";
                continue;
            }
            harness::M68kOptions opt;
            opt.maxInstructions = 500'000'000;
            if (amiga) opt.origin = 0x20000;  // hunk standard (docs/amiga500.md)
            const harness::M68kRun run = harness::runM68kSource(source, amiga ? harness::M68kPlatform::Amiga : harness::M68kPlatform::Test, opt);
            ++ran;
            std::string unemulated;
            for (const std::string& c : run.unemulatedCalls) unemulated += " " + c;
            if (!run.error.empty() || run.console != expected || !unemulated.empty()) {
                ++g_failures;
                std::cerr << label << ": " << run.error << (unemulated.empty() ? "" : " unemulated calls:" + unemulated)
                          << "\n--- expected\n" << expected << "--- actual\n" << run.console << "\n";
            } else {
                std::cout << label << ": ok (" << run.instructions << " instructions)\n";
            }
        }
    }
    std::cout << ran << " programs run, " << skipped << " skipped (not yet supported by the backend)\n";
    return report("m68k_programs");
}
