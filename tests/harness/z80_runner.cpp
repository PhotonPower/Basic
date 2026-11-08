#include "z80_runner.hpp"

#include <algorithm>
#include <cstdio>

#include "z80/assembler.hpp"
#include "z80/cpu.hpp"
#include "z80/memory.hpp"

namespace basic::harness {

namespace {

constexpr std::uint16_t kSentinel = 0x0000;     // return address of the entry: reaching it ends the run
constexpr std::uint16_t kCpcStackTop = 0xBFF0;  // below the screen; the firmware area is not used by the stubs
constexpr std::uint16_t kTestStackTop = 0xFFF0;
constexpr std::uint16_t kFirmwareFirst = 0xB900;  // CPC firmware jump blocks
constexpr std::uint16_t kFirmwareLast = 0xBDFF;
constexpr std::uint16_t kTxtOutput = 0xBB5A;
constexpr std::uint16_t kKmWaitChar = 0xBB06;
constexpr std::uint16_t kKmReadChar = 0xBB09;
constexpr std::uint16_t kKlTimePlease = 0xBD0D;
constexpr std::uint16_t kTxtSetCursor = 0xBB75;
constexpr std::uint16_t kTxtClearWindow = 0xBB6C;
constexpr std::uint8_t kTestConsolePort = 0x01;

std::string hex4(unsigned v) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "&%04X", v & 0xFFFFu);
    return buf;
}

}  // namespace

Z80Run runZ80Image(const std::vector<std::uint8_t>& image, Z80Platform platform, const Z80Options& options) {
    Z80Run result;
    if (image.size() + options.origin > 0x10000) {
        result.error = "image does not fit into 64 KiB";
        return result;
    }
    z80::Memory memory;
    if (options.fill != 0)
        for (unsigned a = 0; a < 0x10000; ++a) memory.write(static_cast<z80::Word>(a), options.fill);
    for (std::size_t i = 0; i < image.size(); ++i)
        memory.write(static_cast<z80::Word>(options.origin + i), image[i]);

    z80::Cpu cpu(memory);
    z80::Registers& r = cpu.regs();
    r.pc = options.origin;
    r.sp = platform == Z80Platform::Cpc ? kCpcStackTop : kTestStackTop;

    std::string firmwareError;
    if (platform == Z80Platform::Test) {
        cpu.setPortHandlers([&options, inputPos = std::size_t{0}](z80::Word port) mutable -> z80::Byte {
                                if ((port & 0xFF) == 0x03) return inputPos < options.input.size() ? 1 : 0;
                                if ((port & 0xFF) == 0x02 && inputPos < options.input.size())
                                    return static_cast<z80::Byte>(options.input[inputPos++]);
                                return 0xFF;
                            },
                            [&result](z80::Word port, z80::Byte value) {
                                if ((port & 0xFF) == kTestConsolePort) result.console += static_cast<char>(value);
                            });
    } else {
        // The program is CALLed like from BASIC: RET from the entry lands on the sentinel.
        r.sp = static_cast<z80::Word>(r.sp - 2);
        memory.write(r.sp, static_cast<z80::Byte>(kSentinel & 0xFF));
        memory.write(static_cast<z80::Word>(r.sp + 1), static_cast<z80::Byte>(kSentinel >> 8));
        cpu.addBreakpoint(kSentinel);
        cpu.setExecHook(kFirmwareFirst, kFirmwareLast, [&, inputPos = std::size_t{0}, time = std::uint32_t{0}](z80::Cpu& c) mutable {
            z80::Registers& regs = c.regs();
            const z80::Word address = regs.pc;
            if (address == kTxtOutput) {
                result.console += static_cast<char>(regs.a());
            } else if (address == kKmWaitChar || address == kKmReadChar) {
                // the next byte of the input with CF set; at its end CF clear (KM WAIT CHAR never
                // returns that on a real CPC; plat_getc takes it as the end of the input)
                const bool have = inputPos < options.input.size();
                const z80::Byte a = have ? static_cast<z80::Byte>(options.input[inputPos++]) : regs.a();
                regs.setAf(static_cast<z80::Word>(a << 8 | ((regs.f() & ~1) | (have ? 1 : 0))));
            } else if (address == kTxtSetCursor) {  // written as the Amiga console does (ESC [ line ; column H)
                result.console += "\x1b[" + std::to_string(regs.hl() & 0xFF) + ";" + std::to_string(regs.hl() >> 8) + "H";
            } else if (address == kTxtClearWindow) {
                result.console += "\x1b[H\x1b[J";
            } else if (address == kKlTimePlease) {  // a clock that advances by 3 (1/100 s) per call
                time += 3;
                regs.setDe(static_cast<z80::Word>(time >> 16));
                regs.setHl(static_cast<z80::Word>(time));
            } else if (firmwareError.empty()) {
                firmwareError = "firmware call " + hex4(address) + " is not emulated";
                regs.pc = kSentinel;  // stop at the sentinel breakpoint
                return true;
            }
            // emulate RET
            const z80::Word ret = static_cast<z80::Word>(c.memory().read(regs.sp) |
                                                         (c.memory().read(static_cast<z80::Word>(regs.sp + 1)) << 8));
            regs.sp = static_cast<z80::Word>(regs.sp + 2);
            regs.pc = ret;
            return true;
        });
    }

    z80::RunLimits limits;
    limits.maxInstructions = options.maxInstructions;
    const z80::RunResult run = cpu.run(limits);
    result.instructions = cpu.instructionCount();  // since reset: this run only
    result.cycles = cpu.cycles();

    if (!firmwareError.empty()) {
        result.error = firmwareError;
    } else if (platform == Z80Platform::Test && run.reason == z80::StopReason::Halted) {
        result.finished = true;
    } else if (platform == Z80Platform::Cpc && run.reason == z80::StopReason::Breakpoint && run.pc == kSentinel) {
        result.finished = true;
    } else {
        result.error = std::string("stopped: ") + z80::toString(run.reason) + " at " + hex4(run.pc);
    }
    return result;
}

Z80Run runZ80Source(const std::string& source, Z80Platform platform, const Z80Options& options) {
    z80::AssemblerOptions asmOptions;
    asmOptions.origin = options.origin;
    const z80::AssemblyResult assembled = z80::Assembler(asmOptions).assemble(source);
    if (!assembled.ok()) {
        Z80Run failed;
        failed.error = assembled.formatDiagnostics("<source>");
        return failed;
    }
    if (assembled.empty()) return runZ80Image({}, platform, options);
    if (assembled.lowAddress() != options.origin) {
        Z80Run failed;
        failed.error = "the program must start at the origin " + hex4(options.origin);
        return failed;
    }
    std::vector<std::uint8_t> image = assembled.flatten();
    if (options.withoutBss)
        for (const auto& [name, sym] : assembled.symbols)
            if ((name == "PRG_BSS" || name == "prg_bss") && sym.value >= options.origin)
                image.resize(std::min<std::size_t>(image.size(), static_cast<std::size_t>(sym.value - options.origin)));
    Z80Run run = runZ80Image(image, platform, options);
    for (const auto& [name, sym] : assembled.symbols)
        if (name == "PRG_END" || name == "prg_end") run.end = static_cast<std::uint32_t>(sym.value);
    return run;
}

}  // namespace basic::harness
