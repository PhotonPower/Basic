#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// Runs MC68000 code in the emulator of the sister project PhotonPower/MC68000 and captures the
/// console output of the program. This is the execution side of the differential tests (docs/testing.md).
namespace basic::harness {

enum class M68kPlatform {
    Test,   ///< m68k-test: MOVE.B to $00FF0000 is console output, MOVE.L $00FF0004 reads the input; STOP ends
    Amiga,  ///< amiga: AmigaDOS hunk program with the sister project's OS emulation (dos Write -> console)
};

struct M68kRun {
    bool finished = false;   ///< STOP (test) resp. return from the program / Exit() (amiga)
    std::string console;
    std::string error;
    std::uint64_t instructions = 0;
    std::uint64_t bytes = 0;  ///< runM68kSource: size of the program (test: the image; amiga: the hunk file)
    std::vector<std::string> unemulatedCalls;  ///< amiga: library calls that only were logged
    std::string gfx;         ///< test: digest of the graphics drawn through the ports (D25), "" if none
};

struct M68kOptions {
    std::uint32_t origin = 0x1000;   ///< test: load address and entry point; amiga: base of the first hunk
    std::uint64_t maxInstructions = 10'000'000;
    std::string input;                ///< test: keyboard input, read byte by byte from $00FF0004 (-1 at the end)
};

/// Assembles `source` with the sister project's assembler (test: plain program with ORG semantics;
/// amiga: the source must use SECTION and becomes a hunk file) and runs it.
[[nodiscard]] M68kRun runM68kSource(const std::string& source, M68kPlatform platform, const M68kOptions& options = {});

/// Runs a raw image (test) or an AmigaDOS hunk file (amiga).
[[nodiscard]] M68kRun runM68kImage(const std::vector<std::uint8_t>& image, M68kPlatform platform,
                                   const M68kOptions& options = {});

}  // namespace basic::harness
