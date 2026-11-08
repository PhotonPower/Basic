// The cpc programs on the real CPC 6128 firmware (docs/testing.md): every tests/programs/<name>.lst
// without keyboard input and without files (AMSDOS is a ROM of its own, not part of the image) is
// compiled for cpc, CALLed in a booted 6128 in the emulator of PhotonPower/Z80 and its output
// compared as in z80_programs. Needs the 32 KiB ROM image (lower ROM and BASIC) in CPC_ROM; without it
// the test reports that it was skipped. BASIC_Z80_PROGRAM=<file> runs one program,
// BASIC_CPC_SCREENS=<dir> writes the screen after each program as <dir>/<name>.png. A few programs
// also go through a snapshot (basicc -o x.sna): it is loaded into a fresh machine and must print
// the same and return to the firmware.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "backend/z80/cpc_snapshot.hpp"
#include "backend/z80/z80_backend.hpp"
#include "frontend/parser.hpp"
#include "frontend/sema.hpp"
#include "test_util.hpp"
#include "z80/assembler.hpp"
#include "z80/json.hpp"
#include "z80/session.hpp"
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

std::string withoutCr(const std::string& t) {
    std::string r;
    for (std::size_t k = 0; k < t.size(); ++k)
        if (!(t[k] == '\r' && k + 1 < t.size() && t[k + 1] == '\n')) r += t[k];
    return r;
}

// The program as a .sna, loaded into a fresh machine (load_sna) and run until the firmware continues
// where the snapshot was taken -> what reached TXT OUTPUT, or "error: ..."
std::string throughSnapshot(const std::string& source, const std::string& rom, const fs::path& file) {
    z80::AssemblerOptions options;
    options.origin = z80Origin(Z80Target::Cpc);
    const z80::AssemblyResult assembled = z80::Assembler(options).assemble(source);
    if (!assembled.ok()) return "error: assembler";
    std::vector<std::uint8_t> image = assembled.flatten();
    if (const auto bss = assembled.symbols.find("PRG_BSS"); bss != assembled.symbols.end())
        image.resize(std::min<std::size_t>(image.size(), bss->second.value - options.origin));
    const CpcSnapshot sna = cpcSnapshot(image, options.origin, rom);
    if (!sna.error.empty()) return "error: " + sna.error;
    {
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(sna.bytes.data()), static_cast<std::streamsize>(sna.bytes.size()));
    }
    z80::Session session{z80::SessionOptions{true}};
    std::string console, error;
    auto cmd = [&](z80::Json request) {
        const z80::Json r = session.handle(request);
        const z80::Json* ok = r.find("ok");
        if ((ok == nullptr || !ok->asBool()) && error.empty()) error = request.dump() + ": " + r.dump();
        return r;
    };
    cmd(z80::Json::parse(R"({"cmd":"machine","type":"cpc"})"));
    cmd(z80::Json::object().set("cmd", "load_rom").set("file", rom).set("which", "both"));
    cmd(z80::Json::object().set("cmd", "load_sna").set("file", file.string()));
    session.cpu().setExecHook(0xBB5A, 0xBB5A, [&console](z80::Cpu& c) {
        console += static_cast<char>(c.regs().a());
        return false;
    });
    const z80::Json r = cmd(z80::Json::object().set("cmd", "run").set("until", static_cast<int>(sna.done))
                                .set("irq_every_cycles", 13312).set("max_instructions", 2'000'000'000));
    if (error.empty() && session.cpu().regs().pc != sna.done) error = "did not return: " + r.dump().substr(0, 300);
    return error.empty() ? console : "error: " + error;
}

}  // namespace

int main() {
    const char* rom = std::getenv("CPC_ROM");
    if (rom == nullptr || !fs::exists(rom)) {
        std::cout << "z80_firmware: skipped (set CPC_ROM to a 32 KiB CPC 6128 ROM image)\n";
        return 0;
    }
    harness::CpcFirmware cpc(rom);
    CHECK(cpc.booted());
    if (!cpc.booted()) {
        std::cerr << cpc.error() << "\n";
        return 1;
    }
    const fs::path dir = fs::path(BASIC_SOURCE_DIR) / "tests" / "programs";
    int ran = 0, skipped = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".lst") continue;
        if (const char* only = std::getenv("BASIC_Z80_PROGRAM"); only && entry.path().filename() != only) continue;
        const std::string name = entry.path().stem().string();
        if (fs::exists(fs::path(entry.path()).replace_extension(".in"))) continue;
        Diagnostics d;
        const ast::Program a = parseProgram(read(entry.path()), d);
        const hir::Program h = analyze(a, d);
        const std::string source = d.empty() ? buildZ80Program(h, Z80Target::Cpc, d) : "";
        if (!d.empty() || source.find("rt_file_") != std::string::npos) {
            ++skipped;
            continue;
        }
        std::string expected;
        for (const char* ext : {".cpc.out", ".z80.out", ".out"})
            if (const fs::path p = fs::path(entry.path()).replace_extension(ext); expected.empty() && fs::exists(p)) expected = read(p);
        if (const fs::path e = fs::path(entry.path()).replace_extension(".err"); fs::exists(e))
            expected += "\nruntime error " + std::to_string(std::stoi(read(e))) + "\n";
        const char* shots = std::getenv("BASIC_CPC_SCREENS");  // the screens as PNG files (to look at)
        const harness::Z80Run run = cpc.run(source, z80Origin(Z80Target::Cpc), 2'000'000'000,
                                            shots != nullptr ? (fs::path(shots) / (name + ".png")).string() : "");
        ++ran;
        ++g_checks;
        const std::string got = withoutCr(run.console);
        if (!run.finished || got != withoutCr(expected)) {
            ++g_failures;
            std::cerr << name << " (6128 firmware): " << run.error << "\n--- expected\n" << expected << "\n--- actual\n" << got << "\n";
        } else {
            std::cout << name << " (6128 firmware): ok (" << run.instructions << " instructions)\n";
        }
    }
    std::cout << ran << " programs run on the firmware, " << skipped << " skipped (unsupported, files)\n";
    for (const char* name : {"numbers", "matrix"}) {  // through a snapshot (TXT OUTPUT only)
        if (const char* only = std::getenv("BASIC_Z80_PROGRAM"); only && std::string(name) + ".lst" != only) continue;
        const fs::path lst = dir / (std::string(name) + ".lst");
        Diagnostics d;
        const hir::Program h = analyze(parseProgram(read(lst), d), d);
        const std::string source = buildZ80Program(h, Z80Target::Cpc, d);
        CHECK(d.empty());
        const std::string got = withoutCr(throughSnapshot(source, rom, fs::temp_directory_path() / (std::string(name) + "_basic_test.sna")));
        const std::string expected = withoutCr(read(fs::path(lst).replace_extension(".out")));
        ++g_checks;
        if (got != expected) {
            ++g_failures;
            std::cerr << name << " (snapshot): \n--- expected\n" << expected << "\n--- actual\n" << got << "\n";
        } else {
            std::cout << name << " (snapshot): ok\n";
        }
    }
    return report("z80_firmware");
}
