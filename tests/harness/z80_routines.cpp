#include "z80_routines.hpp"

#include <cctype>

#include "z80/assembler.hpp"
#include "z80/cpu.hpp"
#include "z80/memory.hpp"

namespace basic::harness {

namespace {
constexpr std::uint16_t kStackTop = 0xFF00;
constexpr std::uint16_t kReturn = 0xFE00;  // return address of every call (no code there)
}  // namespace

struct Z80Routines::Impl {
    z80::Memory memory;
    z80::Cpu cpu{memory};
};

Z80Routines::Z80Routines(const std::string& source, std::uint16_t origin) : impl_(std::make_unique<Impl>()) {
    z80::AssemblerOptions options;
    options.origin = origin;
    const z80::AssemblyResult r = z80::Assembler(options).assemble(source);
    for (const auto& d : r.diagnostics) error_ += "line " + std::to_string(d.line) + ": " + d.message + "\n";
    if (!r.ok()) return;
    for (const auto& seg : r.segments)
        for (std::size_t i = 0; i < seg.bytes.size(); ++i) impl_->memory.write(static_cast<z80::Word>(seg.start + i), seg.bytes[i]);
    for (const auto& [name, sym] : r.symbols) symbols_[name] = sym.value;
}

Z80Routines::~Z80Routines() = default;

std::uint16_t Z80Routines::symbol(const std::string& name) const {
    std::string u;
    for (char c : name) u += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const auto it = symbols_.find(u);
    return it == symbols_.end() ? 0 : static_cast<std::uint16_t>(it->second);
}

Z80CallResult Z80Routines::call(const std::string& routine, const Z80Regs& in, std::uint64_t maxInstructions) {
    Z80CallResult result;
    const std::uint16_t entry = symbol(routine);
    if (entry == 0) {
        result.error = "unknown routine " + routine;
        return result;
    }
    z80::Cpu& cpu = impl_->cpu;
    z80::Registers& r = cpu.regs();
    r.setAf(in.af);
    r.setBc(in.bc);
    r.setDe(in.de);
    r.setHl(in.hl);
    r.ix = in.ix;
    r.iy = in.iy;
    r.sp = static_cast<z80::Word>(kStackTop - 2);
    impl_->memory.write(r.sp, static_cast<z80::Byte>(kReturn & 0xFF));
    impl_->memory.write(static_cast<z80::Word>(r.sp + 1), static_cast<z80::Byte>(kReturn >> 8));
    r.pc = entry;
    cpu.addBreakpoint(kReturn);
    z80::RunLimits limits;
    limits.maxInstructions = maxInstructions;
    const std::uint64_t before = cpu.cycles();
    cpu.run(limits);
    result.cycles = cpu.cycles() - before;
    if (r.pc != kReturn || r.sp != kStackTop) {
        std::string near;  // the label at or just below pc
        std::int64_t best = -1;
        for (const auto& [name, value] : symbols_)
            if (value <= r.pc && value > best) {
                best = value;
                near = name;
            }
        result.error = routine + ": did not return (pc " + std::to_string(r.pc) + " = " + near + "+" +
                       std::to_string(r.pc - best) + ", sp " + std::to_string(r.sp) + ")";
        return result;
    }
    result.regs = {r.af(), r.bc(), r.de(), r.hl(), r.ix, r.iy};
    result.ok = true;
    return result;
}

std::uint8_t Z80Routines::read8(std::uint16_t address) const { return impl_->memory.read(address); }
void Z80Routines::write8(std::uint16_t address, std::uint8_t value) { impl_->memory.write(address, value); }
std::uint64_t Z80Routines::read64(std::uint16_t address) const {
    std::uint64_t v = 0;
    for (int k = 7; k >= 0; --k) v = (v << 8) | read8(static_cast<std::uint16_t>(address + k));
    return v;
}
void Z80Routines::write64(std::uint16_t address, std::uint64_t value) {
    for (int k = 0; k < 8; ++k) write8(static_cast<std::uint16_t>(address + k), static_cast<std::uint8_t>(value >> (8 * k)));
}
std::uint32_t Z80Routines::read32(std::uint16_t address) const { return static_cast<std::uint32_t>(read64(address)); }
void Z80Routines::write32(std::uint16_t address, std::uint32_t value) {
    for (int k = 0; k < 4; ++k) write8(static_cast<std::uint16_t>(address + k), static_cast<std::uint8_t>(value >> (8 * k)));
}

}  // namespace basic::harness
