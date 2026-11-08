#pragma once

// Calls single runtime routines in the Z80 emulator (conformance tests, docs/testing.md): the linked
// runtime is assembled once at &4000; every call sets the registers, runs the routine until it
// returns and reads the registers back. Operands live in memory (docs/runtime.md: HL = result,
// DE, BC = operands as pointers).

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace basic::harness {

struct Z80Regs {
    std::uint16_t af = 0, bc = 0, de = 0, hl = 0, ix = 0, iy = 0;
};

struct Z80CallResult {
    bool ok = false;
    std::string error;
    Z80Regs regs;
    std::uint64_t cycles = 0;
};

class Z80Routines {
public:
    explicit Z80Routines(const std::string& source, std::uint16_t origin = 0x4000);
    ~Z80Routines();
    Z80Routines(const Z80Routines&) = delete;
    Z80Routines& operator=(const Z80Routines&) = delete;

    [[nodiscard]] const std::string& error() const { return error_; }  // assembler errors
    [[nodiscard]] std::uint16_t symbol(const std::string& name) const;

    Z80CallResult call(const std::string& routine, const Z80Regs& in, std::uint64_t maxInstructions = 5'000'000);

    std::uint8_t read8(std::uint16_t address) const;
    void write8(std::uint16_t address, std::uint8_t value);
    std::uint64_t read64(std::uint16_t address) const;  // little-endian
    void write64(std::uint16_t address, std::uint64_t value);
    std::uint32_t read32(std::uint16_t address) const;
    void write32(std::uint16_t address, std::uint32_t value);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string error_;
    std::map<std::string, std::int64_t> symbols_;  // upper case
};

}  // namespace basic::harness
