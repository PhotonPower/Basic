// Differential test of the Z80 targets: every tests/programs/<name>.lst that the backend supports is
// compiled for z80-test and for cpc (firmware calls stubbed in the harness), run in the emulator of
// PhotonPower/Z80 and its output compared with <name>.out (made on the original). On the CPC a line
// ends with CR LF, so CR before LF is ignored there; programs with keyboard input run on z80-test only
// (the CPC echoes the typed characters). A <name>.z80.out replaces <name>.out where the Z80 sees other
// bytes (group memory), a <name>.cpc.out on cpc where AMSDOS differs (one input and one output
// file). BASIC_Z80_PROGRAM=<file> runs one program; BASIC_Z80_PROFILE=1 prints the T-states per
// routine (z80-test).

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

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

// Compiles a program; `unsupported` is set if the backend or the runtime cannot yet.
std::string compile(const std::string& src, Z80Target target, bool& unsupported, std::string& message) {
    Diagnostics d;
    const ast::Program a = parseProgram(src, d);
    const hir::Program h = analyze(a, d);
    if (!d.empty()) {
        message = d[0].message;
        return "";
    }
    const std::string source = buildZ80Program(h, target, d);
    if (!d.empty()) {
        message = d[0].message;
        unsupported = message.find("not supported") != std::string::npos || message.find("not provided by any module") != std::string::npos;
    }
    return source;
}

std::string withoutCr(const std::string& t) {
    std::string r;
    for (std::size_t k = 0; k < t.size(); ++k)
        if (!(t[k] == '\r' && k + 1 < t.size() && t[k + 1] == '\n')) r += t[k];
    return r;
}

}  // namespace

int main() {
    const fs::path dir = fs::path(BASIC_SOURCE_DIR) / "tests" / "programs";
    int ran = 0, skipped = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".lst") continue;
        if (const char* only = std::getenv("BASIC_Z80_PROGRAM"); only && entry.path().filename() != only) continue;
        for (const Z80Target target : {Z80Target::Test, Z80Target::Cpc}) {
            const bool cpc = target == Z80Target::Cpc;
            const std::string name = entry.path().stem().string();
            const std::string label = name + (cpc ? " (cpc)" : " (z80-test)");
            const fs::path in = fs::path(entry.path()).replace_extension(".in");
            if (cpc && fs::exists(in)) continue;
            // <name>.z80.out: the Z80 sees other bytes (little-endian, 16-bit descriptors; group memory);
            // <name>.cpc.out: the CPC differs (AMSDOS: one input and one output file)
            const fs::path z80Out = fs::path(entry.path()).replace_extension(".z80.out");
            const fs::path cpcOut = fs::path(entry.path()).replace_extension(".cpc.out");
            std::string expected = read(cpc && fs::exists(cpcOut) ? cpcOut
                                        : fs::exists(z80Out)     ? z80Out
                                                                 : fs::path(entry.path()).replace_extension(".out"));
            if (const fs::path e = fs::path(entry.path()).replace_extension(".err"); fs::exists(e))
                expected += "\nruntime error " + std::to_string(std::stoi(read(e))) + "\n";
            bool unsupported = false;
            std::string message;
            const std::string source = compile(read(entry.path()), target, unsupported, message);
            if (unsupported) {
                std::cout << label << ": skipped (" << message << ")\n";
                ++skipped;
                continue;
            }
            if (cpc && source.find("rt_gfx_cmd") != std::string::npos) {  // no GRA stubs: on the real firmware
                std::cout << label << ": skipped (graphics: see z80_firmware)\n";
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
            opt.origin = z80Origin(target);
            if (cpc) {  // as from the binary file: no BSS, memory not cleared
                opt.fill = 0xE5;
                opt.withoutBss = true;
            }
            if (fs::exists(in)) opt.input = read(in);
            opt.profile = std::getenv("BASIC_Z80_PROFILE") != nullptr && !cpc;  // T-states per routine
            const harness::Z80Run run = harness::runZ80Source(source, cpc ? harness::Z80Platform::Cpc : harness::Z80Platform::Test, opt);
            ++ran;
            const unsigned size = run.end > opt.origin ? run.end - opt.origin : 0;
            const std::string got = cpc ? withoutCr(run.console) : run.console;
            if (!run.error.empty() || got != (cpc ? withoutCr(expected) : expected)) {
                ++g_failures;
                std::cerr << label << ": " << run.error << " (" << size << " bytes)\n--- expected\n" << expected << "--- actual\n" << got << "\n";
            } else {
                std::cout << label << ": ok (" << run.instructions << " instructions, " << run.cycles << " cycles, " << size << " bytes)\n"
                          << run.profile;
            }
        }
    }
    {  // the AMSDOS header of a CPC binary
        const std::vector<std::uint8_t> h = amsdosHeader("out/Prog12345.bin", 0x0100, 0x0123, 0x12345);
        unsigned sum = 0;
        for (int k = 0; k <= 0x42; ++k) sum += h[static_cast<std::size_t>(k)];
        const std::string name(h.begin() + 1, h.begin() + 12);
        ++g_checks;
        if (h.size() != 128 || name != "PROG1234BIN" || h[0x12] != 2 || h[0x15] != 0x00 || h[0x16] != 0x01 || h[0x1A] != 0x23 ||
            h[0x1B] != 0x01 || h[0x18] != 0x45 || h[0x19] != 0x23 || h[0x40] != 0x45 || h[0x41] != 0x23 || h[0x42] != 0x01 ||
            h[0x43] != (sum & 0xFF) || h[0x44] != (sum >> 8)) {
            ++g_failures;
            std::cerr << "amsdosHeader: wrong fields (name " << name << ")\n";
        }
    }
    {  // --org: the start address of a CPC program (Basic, hex in all notations, range)
        std::string error;
        for (const auto& [text, want] : std::vector<std::pair<std::string, int>>{
                 {"&4000", 0x4000}, {"&h4000", 0x4000}, {"$4000", 0x4000}, {"0x4000", 0x4000}, {"16384", 0x4000},
                 {"&40", 0x40}, {"&A1FF", 0xA1FF}, {"&3F", -1}, {"&A200", -1}, {"&10000", -1}, {"", -1},
                 {"&", -1}, {"4k", -1}, {"&G0", -1}}) {
            const std::optional<std::uint16_t> got = parseCpcOrigin(text, error);
            ++g_checks;
            if ((want < 0) != !got.has_value() || (got && *got != want) || (!got && error.empty())) {
                ++g_failures;
                std::cerr << "parseCpcOrigin(\"" << text << "\"): wrong result\n";
            }
        }
    }
    {  // a CPC program linked at another address runs there
        const fs::path lst = dir / "numbers.lst";
        Diagnostics d;
        const hir::Program h = analyze(parseProgram(read(lst), d), d);
        const std::string source = buildZ80Program(h, Z80Target::Cpc, d, std::uint16_t{0x4000});
        harness::Z80Options opt;
        opt.maxInstructions = 500'000'000;
        opt.origin = 0x4000;
        opt.fill = 0xE5;
        opt.withoutBss = true;
        const harness::Z80Run run = harness::runZ80Source(source, harness::Z80Platform::Cpc, opt);
        const fs::path cpcOut = fs::path(lst).replace_extension(".cpc.out");
        const fs::path z80Out = fs::path(lst).replace_extension(".z80.out");
        const std::string expected = read(fs::exists(cpcOut) ? cpcOut : fs::exists(z80Out) ? z80Out : fs::path(lst).replace_extension(".out"));
        ++g_checks;
        if (!d.empty() || source.find("ORG     16384") == std::string::npos || !run.error.empty() ||
            withoutCr(run.console) != withoutCr(expected)) {
            ++g_failures;
            std::cerr << "numbers (cpc, ORG &4000): " << (d.empty() ? run.error : d[0].message) << "\n--- actual\n"
                      << withoutCr(run.console) << "\n";
        } else {
            std::cout << "numbers (cpc, ORG &4000): ok\n";
        }
    }
    std::cout << ran << " programs run, " << skipped << " skipped (not yet supported by the backend)\n";
    return report("z80_programs");
}
