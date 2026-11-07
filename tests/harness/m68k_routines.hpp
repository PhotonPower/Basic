#pragma once

// Calls single runtime routines in the MC68000 emulator (conformance tests, docs/testing.md):
// the linked runtime is assembled once at $1000; every call sets registers, runs the routine
// until it returns and reads the registers back.

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace basic::harness {

struct M68kRegs {
    std::array<std::uint32_t, 8> d{};
    std::array<std::uint32_t, 7> a{};  // A0-A6
};

struct M68kCallResult {
    bool ok = false;
    std::string error;
    M68kRegs regs;
    std::uint64_t instructions = 0;
};

class M68kRoutines {
public:
    explicit M68kRoutines(const std::string& source, std::uint32_t origin = 0x1000);
    ~M68kRoutines();
    M68kRoutines(const M68kRoutines&) = delete;
    M68kRoutines& operator=(const M68kRoutines&) = delete;

    [[nodiscard]] const std::string& error() const { return error_; }  // assembler errors
    [[nodiscard]] std::uint32_t symbol(const std::string& name) const;

    M68kCallResult call(const std::string& routine, const M68kRegs& in, std::uint64_t maxInstructions = 1'000'000);

    std::uint8_t read8(std::uint32_t address) const;
    std::uint32_t read32(std::uint32_t address) const;
    void write8(std::uint32_t address, std::uint8_t value);
    void write32(std::uint32_t address, std::uint32_t value);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string error_;
    std::map<std::string, std::int64_t> symbols_;
};

}  // namespace basic::harness
