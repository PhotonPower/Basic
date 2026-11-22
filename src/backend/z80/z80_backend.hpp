#pragma once

// Z80 backend: LIR -> assembler source for the assembler of PhotonPower/Z80 (D30). Every virtual
// register lives in an 8-byte slot of the stack frame (IX); runtime routines get pointers: numbers
// as the address of their slot, strings and addresses as the value itself (docs/runtime.md).

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "frontend/diagnostic.hpp"
#include "ir/lir.hpp"
#include "link/linker.hpp"

namespace basic {

// The program part for the linker; features the backend does not support yet go to `diags`.
link::Program generateZ80(const lir::Program& program, Diagnostics& diags);

enum class Z80Target { Test, Cpc };

// Load address and entry of the program: z80-test &4000, cpc &0100 (RUN"name" or CALL &100).
std::uint16_t z80Origin(Z80Target target);

// The range a cpc program may start in (basicc --org): above the restart vectors of the firmware
// (&0000-&003F), below the end of the string space (&A200, platform/cpc/plat_heap.s).
constexpr std::uint16_t kCpcOriginMin = 0x0040;
constexpr std::uint16_t kCpcOriginMax = 0xA1FF;

// Parses a start address for --org: &4000, &H4000, $4000, 0x4000 or decimal. On an error (syntax,
// outside kCpcOriginMin..kCpcOriginMax) returns nothing and sets `error`.
std::optional<std::uint16_t> parseCpcOrigin(const std::string& text, std::string& error);

// The 128-byte AMSDOS header of a binary file (type 2): name from `fileName` (8.3, upper case), load
// and entry address, length, checksum over bytes 0-66.
std::vector<std::uint8_t> amsdosHeader(const std::string& fileName, std::uint16_t load, std::uint16_t entry, std::size_t length);

// The whole Z80 pipeline after the frontend: lowering, backend, source-level linker with the
// runtime and platform modules. Returns the linked assembler source (one ORG program), linked at
// `origin` or, without it, at z80Origin(target).
std::string buildZ80Program(const hir::Program& program, Z80Target target, Diagnostics& diags,
                            std::optional<std::uint16_t> origin = std::nullopt);

}  // namespace basic
