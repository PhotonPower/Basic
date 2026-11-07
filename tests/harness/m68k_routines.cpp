#include "m68k_routines.hpp"

#include "m68k/assembler.hpp"
#include "m68k/cpu.hpp"
#include "m68k/memory.hpp"

namespace basic::harness {

namespace {
constexpr std::uint32_t kStackTop = 0x80000;
constexpr std::uint32_t kReturn = 0x7FF00;  // return address of every call (no code there)
}  // namespace

struct M68kRoutines::Impl {
    m68k::Memory memory;
    m68k::Cpu cpu{memory};
};

M68kRoutines::M68kRoutines(const std::string& source, std::uint32_t origin) : impl_(std::make_unique<Impl>()) {
    m68k::AssemblerOptions options;
    options.origin = origin;
    const m68k::AssemblyResult r = m68k::assemble(source, options);
    for (const auto& d : r.errors) error_ += "line " + std::to_string(d.line) + ": " + d.message + "\n";
    if (!r.ok()) return;
    for (const auto& seg : r.segments)
        for (std::size_t i = 0; i < seg.bytes.size(); ++i)
            impl_->memory.write8(static_cast<m68k::Address>(seg.address + i), seg.bytes[i]);
    symbols_ = r.symbols;
}

M68kRoutines::~M68kRoutines() = default;

std::uint32_t M68kRoutines::symbol(const std::string& name) const {
    const auto it = symbols_.find(name);
    return it == symbols_.end() ? 0 : static_cast<std::uint32_t>(it->second);
}

M68kCallResult M68kRoutines::call(const std::string& routine, const M68kRegs& in, std::uint64_t maxInstructions) {
    M68kCallResult result;
    const auto it = symbols_.find(routine);
    if (it == symbols_.end()) {
        result.error = "unknown routine " + routine;
        return result;
    }
    m68k::Cpu& cpu = impl_->cpu;
    m68k::CpuState st = cpu.state();
    for (int k = 0; k < 8; ++k) st.regs.d[static_cast<std::size_t>(k)] = in.d[static_cast<std::size_t>(k)];
    for (int k = 0; k < 7; ++k) st.regs.a[static_cast<std::size_t>(k)] = in.a[static_cast<std::size_t>(k)];
    st.regs.a[7] = kStackTop;
    st.regs.sr = 0x2700;
    st.halted = st.stopped = false;
    cpu.setState(st);
    const m68k::RunResult run = cpu.call(static_cast<m68k::Address>(it->second), kReturn, maxInstructions);
    result.instructions = run.instructions;
    const m68k::Registers& regs = cpu.registers();
    if (regs.pc != kReturn || regs.a[7] != kStackTop) {
        result.error = routine + ": did not return (reason " + std::to_string(static_cast<int>(run.reason)) + ")";
        return result;
    }
    for (int k = 0; k < 8; ++k) result.regs.d[static_cast<std::size_t>(k)] = regs.d[static_cast<std::size_t>(k)];
    for (int k = 0; k < 7; ++k) result.regs.a[static_cast<std::size_t>(k)] = regs.a[static_cast<std::size_t>(k)];
    result.ok = true;
    return result;
}

std::uint8_t M68kRoutines::read8(std::uint32_t address) const { return impl_->memory.read8(address); }
std::uint32_t M68kRoutines::read32(std::uint32_t address) const { return impl_->memory.read32(address); }
void M68kRoutines::write8(std::uint32_t address, std::uint8_t value) { impl_->memory.write8(address, value); }
void M68kRoutines::write32(std::uint32_t address, std::uint32_t value) { impl_->memory.write32(address, value); }

}  // namespace basic::harness
