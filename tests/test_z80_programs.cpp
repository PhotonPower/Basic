// Differential test of the Z80 target: every tests/programs/<name>.lst that the backend supports is
// compiled for z80-test, run in the emulator of PhotonPower/Z80 and its output compared with
// <name>.out (made on the original). BASIC_Z80_PROGRAM=<file> runs one program.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "backend/z80/z80_backend.hpp"
#include "frontend/parser.hpp"
#include "frontend/sema.hpp"
#include "test_util.hpp"
#include "z80_runner.hpp"

using namespace basic;
namespace fs = std::filesystem;

namespace {

std::string read(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Compiles a program for z80-test; `unsupported` is set if the backend or the runtime cannot yet.
std::string compile(const std::string& src, bool& unsupported, std::string& message) {
    Diagnostics d;
    const ast::Program a = parseProgram(src, d);
    const hir::Program h = analyze(a, d);
    if (!d.empty()) {
        message = d[0].message;
        return "";
    }
    const std::string source = buildZ80Program(h, Z80Target::Test, d);
    if (!d.empty()) {
        message = d[0].message;
        unsupported = message.find("not supported") != std::string::npos || message.find("not provided by any module") != std::string::npos;
    }
    return source;
}

}  // namespace

int main() {
    const fs::path dir = fs::path(BASIC_SOURCE_DIR) / "tests" / "programs";
    int ran = 0, skipped = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".lst") continue;
        if (const char* only = std::getenv("BASIC_Z80_PROGRAM"); only && entry.path().filename() != only) continue;
        const std::string name = entry.path().stem().string();
        const std::string label = name + " (z80-test)";
        std::string expected = read(fs::path(entry.path()).replace_extension(".out"));
        if (const fs::path e = fs::path(entry.path()).replace_extension(".err"); fs::exists(e))
            expected += "\nruntime error " + std::to_string(std::stoi(read(e))) + "\n";
        if (fs::exists(fs::path(entry.path()).replace_extension(".in"))) {
            std::cout << label << ": skipped (keyboard input not yet on z80-test)\n";
            ++skipped;
            continue;
        }
        bool unsupported = false;
        std::string message;
        const std::string source = compile(read(entry.path()), unsupported, message);
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
        harness::Z80Options opt;
        opt.maxInstructions = 500'000'000;
        const harness::Z80Run run = harness::runZ80Source(source, harness::Z80Platform::Test, opt);
        ++ran;
        if (!run.error.empty() || run.console != expected) {
            ++g_failures;
            std::cerr << label << ": " << run.error << "\n--- expected\n" << expected << "--- actual\n" << run.console << "\n";
        } else {
            std::cout << label << ": ok (" << run.instructions << " instructions)\n";
        }
    }
    std::cout << ran << " programs run, " << skipped << " skipped (not yet supported by the backend)\n";
    return report("z80_programs");
}
