#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// A CPC snapshot (.sna) of a compiled cpc program: the real 6128 firmware (a 32 KiB ROM image:
/// lower ROM and BASIC, not part of this project) is booted to the BASIC prompt in the emulator of
/// PhotonPower/Z80, the program is loaded at its origin, and the snapshot starts a small loader that
/// CALLs it as BASIC would and then returns to the prompt (docs/targets.md#cpc-z80).
namespace basic {

struct CpcSnapshot {
    std::vector<std::uint8_t> bytes;  ///< the .sna file (version 1, 64 KiB), empty after an error
    std::uint16_t loader = 0;         ///< where the snapshot starts (the loader)
    std::uint16_t resume = 0;         ///< where the firmware continues after the program
    std::uint16_t done = 0;           ///< the last instruction of the loader (JP resume)
    std::string error;
};

[[nodiscard]] CpcSnapshot cpcSnapshot(const std::vector<std::uint8_t>& image, std::uint16_t origin, const std::string& romFile);

}  // namespace basic
