#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// Runs Z80 code in the emulator of the sister project PhotonPower/Z80 and captures the console
/// output of the program. This is the execution side of the differential tests (docs/testing.md).
namespace basic::harness {

enum class Z80Platform {
    Test,  ///< z80-test: a byte written with OUT (&01),A is console output; the program ends with HALT
    Cpc,   ///< cpc: firmware calls are stubbed (TXT OUTPUT &BB5A prints A); the program ends with RET
};

struct Z80Run {
    bool finished = false;       ///< the program ended normally (HALT resp. RET from the entry)
    std::string console;         ///< everything the program printed
    std::string error;           ///< assembler errors, unknown firmware call, limit, ...
    std::uint64_t instructions = 0;
    std::uint64_t cycles = 0;    ///< T-states, useful to compare the speed of runtime routines
};

struct Z80Options {
    std::uint16_t origin = 0x4000;              ///< load address and entry point
    std::uint64_t maxInstructions = 10'000'000;
    std::string input;  ///< z80-test: keyboard input (IN from &03: 1 if a byte is left, IN from &02: the byte)
};

/// Assembles `source` with the sister project's assembler and runs it from `options.origin`.
[[nodiscard]] Z80Run runZ80Source(const std::string& source, Z80Platform platform, const Z80Options& options = {});

/// Runs an already assembled image loaded at `options.origin`.
[[nodiscard]] Z80Run runZ80Image(const std::vector<std::uint8_t>& image, Z80Platform platform,
                                 const Z80Options& options = {});

}  // namespace basic::harness
