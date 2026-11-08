#pragma once

// Z80 backend: LIR -> assembler source for the assembler of PhotonPower/Z80 (D30). Every virtual
// register lives in an 8-byte slot of the stack frame (IX); runtime routines get pointers: numbers
// as the address of their slot, strings and addresses as the value itself (docs/runtime.md).

#include <cstdint>
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

// The 128-byte AMSDOS header of a binary file (type 2): name from `fileName` (8.3, upper case), load
// and entry address, length, checksum over bytes 0-66.
std::vector<std::uint8_t> amsdosHeader(const std::string& fileName, std::uint16_t load, std::uint16_t entry, std::size_t length);

// The whole Z80 pipeline after the frontend: lowering, backend, source-level linker with the
// runtime and platform modules. Returns the linked assembler source (one ORG program).
std::string buildZ80Program(const hir::Program& program, Z80Target target, Diagnostics& diags);

}  // namespace basic
