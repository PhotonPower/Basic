#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// Runs Z80 code in the emulator of the sister project PhotonPower/Z80 and captures the console
/// output of the program. This is the execution side of the differential tests (docs/testing.md).
namespace basic::harness {

enum class Z80Platform {
    Test,  ///< z80-test: a byte written with OUT (&01),A is console output; the program ends with HALT
    Cpc,   ///< cpc: firmware calls are stubbed (TXT OUTPUT &BB5A prints A, the cursor calls print ANSI sequences); the program ends with RET
};

struct Z80Run {
    bool finished = false;       ///< the program ended normally (HALT resp. RET from the entry)
    std::string console;         ///< everything the program printed
    std::string error;           ///< assembler errors, unknown firmware call, limit, ...
    std::uint64_t instructions = 0;
    std::uint64_t cycles = 0;    ///< T-states, useful to compare the speed of runtime routines
    std::uint32_t end = 0;       ///< runZ80Source: the label prg_end (first byte after code, data and BSS), if any
};

struct Z80Options {
    std::uint16_t origin = 0x4000;              ///< load address and entry point
    std::uint64_t maxInstructions = 10'000'000;
    std::string input;  ///< z80-test: keyboard input (IN from &03: 1 if a byte is left, IN from &02: the byte)
    std::uint8_t fill = 0;    ///< contents of the memory before the program is loaded
    bool withoutBss = false;  ///< runZ80Source: load only up to prg_bss (as the CPC binary file)
};

/// Assembles `source` with the sister project's assembler and runs it from `options.origin`.
[[nodiscard]] Z80Run runZ80Source(const std::string& source, Z80Platform platform, const Z80Options& options = {});

/// Runs an already assembled image loaded at `options.origin`.
[[nodiscard]] Z80Run runZ80Image(const std::vector<std::uint8_t>& image, Z80Platform platform,
                                 const Z80Options& options = {});

/// A CPC with its real firmware (the 32 KiB ROM image of a 464, 664 or 6128: lower ROM and BASIC), booted once to
/// the BASIC prompt in the emulator of PhotonPower/Z80. `run` loads a program and CALLs it as BASIC
/// would (lower ROM off, 300 Hz interrupts); the console is what reached TXT OUTPUT, with
/// TXT SET CURSOR and TXT CLEAR WINDOW written as ANSI sequences (as the stubs of Z80Platform::Cpc).
class CpcFirmware {
public:
    explicit CpcFirmware(const std::string& romFile);
    ~CpcFirmware();
    CpcFirmware(const CpcFirmware&) = delete;
    CpcFirmware& operator=(const CpcFirmware&) = delete;

    [[nodiscard]] bool booted() const { return booted_; }
    [[nodiscard]] const std::string& error() const { return error_; }
    /// The version of the BASIC ROM (KL PROBE ROM): 0 CPC 464, 1 CPC 664, 2 CPC 6128.
    [[nodiscard]] int basicVersion() const { return version_; }
    /// Assembles `source` (origin `origin`), loads it without the BSS and CALLs `origin`. With
    /// `screenPng`, the screen afterwards is written there (as the CRT shows it).
    [[nodiscard]] Z80Run run(const std::string& source, std::uint16_t origin, std::uint64_t maxInstructions,
                             const std::string& screenPng = "");

private:
    struct Impl;
    Impl* impl_;
    bool booted_ = false;
    int version_ = -1;
    std::string error_;
};

}  // namespace basic::harness
