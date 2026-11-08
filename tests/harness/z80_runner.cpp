#include "z80_runner.hpp"

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
        cpu.setExecHook(kFirmwareFirst, kFirmwareLast, [&](z80::Cpu& c) {
            z80::Registers& regs = c.regs();
            const z80::Word address = regs.pc;
            if (address == kTxtOutput) {
                result.console += static_cast<char>(regs.a());
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
    return runZ80Image(assembled.flatten(), platform, options);
}

}  // namespace basic::harness
