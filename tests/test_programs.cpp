// Golden tests: every tests/programs/<name>.lst runs on the reference interpreter and must print
// exactly tests/programs/<name>.out (docs/testing.md#differenztests).

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "frontend/parser.hpp"
#include "frontend/sema.hpp"
#include "ir/interp.hpp"
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

// "' test: seed 42" in the program header
std::uint32_t seedOf(const std::string& src) {
    const auto pos = src.find("' test: seed ");
    return pos == std::string::npos ? 1u : static_cast<std::uint32_t>(std::stoul(src.substr(pos + 13)));
}

}  // namespace

int main() {
    const fs::path dir = fs::path(BASIC_SOURCE_DIR) / "tests" / "programs";
    int count = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".lst") continue;
        const std::string name = entry.path().stem().string();
        const fs::path expectedPath = fs::path(entry.path()).replace_extension(".out");
        CHECK(fs::exists(expectedPath));
        const std::string src = read(entry.path());
        Diagnostics d;
        const ast::Program a = parseProgram(src, d);
        const hir::Program h = analyze(a, d);
        for (const Diagnostic& x : d) std::cerr << name << ":" << x.line << ":" << x.col << ": " << x.message << "\n";
        CHECK(d.empty());
        RunOptions opt;
        opt.seed = seedOf(src);
        const fs::path inputPath = fs::path(entry.path()).replace_extension(".in");  // keyboard input
        std::istringstream input(fs::exists(inputPath) ? read(inputPath) : std::string());
        opt.input = &input;
        opt.maxSteps = 10'000'000;
        const RunResult r = interpret(h, opt);
        const fs::path errPath = fs::path(entry.path()).replace_extension(".err");  // expected unhandled error
        if (fs::exists(errPath)) {
            const int code = std::stoi(read(errPath));
            if (r.ok || r.error != code) std::cerr << name << ": expected the end with runtime error " << code << ", got " << (r.ok ? "none" : r.message) << "\n";
            CHECK(!r.ok && r.error == code);
        } else {
            if (!r.ok) std::cerr << name << ":" << r.line << ": " << r.message << "\n";
            CHECK(r.ok);
        }
        const fs::path gfxPath = fs::path(entry.path()).replace_extension(".gfx");  // graphics digest (D25)
        if (fs::exists(gfxPath)) {
            const std::string expectedGfx = read(gfxPath);
            if (r.gfx != expectedGfx) std::cerr << name << ": graphics differ\n--- expected\n" << expectedGfx << "--- actual\n" << r.gfx;
            CHECK_EQ(r.gfx, expectedGfx);
        }
        const std::string expected = read(expectedPath);
        if (r.output != expected) {
            ++g_failures;
            std::cerr << name << ": output differs\n--- expected\n" << expected << "--- actual\n" << r.output;
        }
        ++g_checks;
        ++count;
    }
    CHECK(count >= 7);
    return report("programs");
}
