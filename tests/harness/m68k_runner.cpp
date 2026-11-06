#include "m68k_runner.hpp"

#include <cstdio>
#include <exception>

#include "m68k/amiga.hpp"
#include "m68k/amiga_os.hpp"
#include "m68k/assembler.hpp"
#include "m68k/cpu.hpp"
#include "m68k/hunk.hpp"
#include "m68k/memory.hpp"

namespace basic::harness {

namespace {

constexpr m68k::Address kTestConsole = 0x00FF0000;
constexpr m68k::Address kTestStackTop = 0x00080000;

std::string hex8(std::uint32_t v) {
    char buf[12];
    std::snprintf(buf, sizeof buf, "$%08X", v);
    return buf;
}

M68kRun runTest(const std::vector<std::uint8_t>& image, const M68kOptions& options) {
    M68kRun result;
    m68k::Memory memory;
    for (std::size_t i = 0; i < image.size(); ++i)
        memory.write8(static_cast<m68k::Address>(options.origin + i), image[i]);
    m68k::Cpu cpu(memory);
    cpu.writeOverride = [&result](m68k::Address a, m68k::Size s, m68k::Long v) {
        if (a == kTestConsole && s == m68k::Size::B) {
            result.console += static_cast<char>(v & 0xFF);
            return true;
        }
        return false;
    };
    m68k::CpuState st = cpu.state();
    st.regs.pc = options.origin;
    st.regs.a[7] = kTestStackTop;
    cpu.setState(st);
    const m68k::RunResult run = cpu.run(options.maxInstructions);
    result.instructions = run.instructions;
    if (run.reason == m68k::StopReason::Stopped) result.finished = true;
    else result.error = "stopped (reason " + std::to_string(static_cast<int>(run.reason)) + ") at " + hex8(cpu.registers().pc);
    return result;
}

M68kRun runAmiga(const std::vector<std::uint8_t>& image, const M68kOptions& options) {
    M68kRun result;
    m68k::Memory memory;
    m68k::Cpu cpu(memory);
    m68k::Amiga500 amiga(memory);
    amiga.install(cpu);
    m68k::AmigaOs os(memory, amiga);
    os.install(cpu);
    amiga.reset(cpu);
    os.reset();

    m68k::HunkImage loaded;
    try {
        loaded = m68k::loadHunkFile(image, options.origin);
    } catch (const std::exception& e) {
        result.error = std::string("hunk file: ") + e.what();
        return result;
    }
    if (loaded.end > os.heapBase()) {
        result.error = "the program reaches into the heap of the emulated OS";
        return result;
    }
    for (const m68k::Segment& seg : loaded.segments)
        for (std::size_t i = 0; i < seg.bytes.size(); ++i)
            memory.write8(static_cast<m68k::Address>(seg.address + i), seg.bytes[i]);

    cpu.registers().pc = loaded.entry;
    cpu.registers().a[7] = os.stackTop();
    os.prepareProgram(cpu);

    const m68k::RunResult run = cpu.run(options.maxInstructions);
    result.instructions = run.instructions;
    result.console = os.console();
    for (const m68k::LibCall& call : os.log())
        if (!call.emulated)
            result.unemulatedCalls.push_back(call.library + " " +
                                             (call.function.empty() ? std::to_string(call.offset) : call.function));
    if (run.reason == m68k::StopReason::Requested) result.finished = true;
    else result.error = "stopped (reason " + std::to_string(static_cast<int>(run.reason)) + ") at " + hex8(cpu.registers().pc);
    return result;
}

}  // namespace

M68kRun runM68kImage(const std::vector<std::uint8_t>& image, M68kPlatform platform, const M68kOptions& options) {
    return platform == M68kPlatform::Test ? runTest(image, options) : runAmiga(image, options);
}

M68kRun runM68kSource(const std::string& source, M68kPlatform platform, const M68kOptions& options) {
    m68k::AssemblerOptions asmOptions;
    asmOptions.origin = options.origin;
    const m68k::AssemblyResult assembled = m68k::assemble(source, asmOptions);
    if (!assembled.ok()) {
        M68kRun failed;
        for (const m68k::Diagnostic& d : assembled.errors)
            failed.error += "<source>:" + std::to_string(d.line) + ": error: " + d.message + "\n";
        return failed;
    }
    if (platform == M68kPlatform::Amiga) {
        if (!assembled.hunk) {
            M68kRun failed;
            failed.error = "an amiga program needs SECTION lines (hunk output)";
            return failed;
        }
        return runAmiga(m68k::writeHunkFile(assembled.sections), options);
    }
    if (assembled.hunk) {
        M68kRun failed;
        failed.error = "an m68k-test program must not use SECTION";
        return failed;
    }
    if (!assembled.segments.empty() && assembled.lowest() != options.origin) {
        M68kRun failed;
        failed.error = "the program must start at the origin " + hex8(options.origin);
        return failed;
    }
    return runTest(assembled.flatten(), options);
}

}  // namespace basic::harness
