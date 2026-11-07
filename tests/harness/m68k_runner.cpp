#include "m68k_runner.hpp"

#include "rt_gfx.hpp"
#include "rt_gui.hpp"

#include <sstream>

#include <cstdio>
#include <exception>

#include "m68k/amiga.hpp"
#include "m68k/amiga_os.hpp"
#include "m68k/assembler.hpp"
#include "m68k/cpu.hpp"
#include "m68k/hunk.hpp"
#include "rt_gfx.hpp"
#include "m68k/memory.hpp"

namespace basic::harness {

namespace {

constexpr m68k::Address kTestConsole = 0x00FF0000;
constexpr m68k::Address kTestInput = 0x00FF0004;  // long read: next byte of the input, or -1
constexpr m68k::Address kTestStackTop = 0x00080000;
// graphics ports of m68k-test (platform/m68k-test/plat_gfx.s, D25)
constexpr m68k::Address kGfxCommand = 0x00FF0010, kGfxArg = 0x00FF0014, kGfxStrAddr = 0x00FF0018,
                        kGfxStrLen = 0x00FF001C, kGfxMode = 0x00FF0020, kGfxPoint = 0x00FF0024;
// requesters and menus (platform/m68k-test/plat_gui.s, D26): text dialogs with brt::Gui
constexpr m68k::Address kGuiNum = 0x00FF0030, kGuiStrAddr = 0x00FF0034, kGuiStrLen = 0x00FF0038,
                        kGuiCommand = 0x00FF003C, kGuiResult = 0x00FF0040, kGuiResultLen = 0x00FF0044,
                        kGuiResultChar = 0x00FF0048;

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
    brt::Gfx gfx;
    std::vector<std::int32_t> gfxArgs;
    std::uint32_t strAddr = 0, strLen = 0, gfxMode = 0;
    brt::Gui gui;
    brt::Output guiOut;
    std::vector<std::int32_t> guiNums;
    std::vector<std::string> guiStrs;
    std::uint32_t guiStrAddr = 0;
    std::int32_t guiResult = 0;
    std::string guiText;  // the string result
    std::size_t guiTextPos = 0;
    std::size_t inputPos = 0;
    const auto readString = [&](std::uint32_t addr, std::uint32_t len) {
        std::string r;
        for (std::uint32_t k = 0; k < len; ++k) r += static_cast<char>(memory.read8(static_cast<m68k::Address>(addr + k)));
        return r;
    };
    // one command of rt_gui: the dialogs read the input where the keyboard port stands
    const auto guiCommand = [&](std::uint32_t code) {
        std::istringstream in(inputPos < options.input.size() ? options.input.substr(inputPos) : std::string());
        gui.in = &in;
        guiResult = 0;
        guiText.clear();
        guiTextPos = 0;
        const auto num = [&](std::size_t k) { return k < guiNums.size() ? guiNums[k] : 0; };
        const auto str = [&](std::size_t k) { return k < guiStrs.size() ? guiStrs[k] : std::string(); };
        const std::string name = code < static_cast<std::uint32_t>(brt::kGuiCommandCount) ? brt::kGuiCommands[code] : "";
        if (name == "ALERT") guiResult = gui.alert(guiOut, num(0), str(0), num(1), str(1));
        else if (name == "FILESELECT") guiText = gui.fileselect(guiOut, str(0), str(1), str(2));
        else if (name == "FORM INPUT") guiText = gui.formInput(guiOut, num(0), str(0));
        else if (name == "MENU") gui.menu(guiOut, guiStrs);
        else if (name == "MENU FLAGS") gui.menuFlags(num(0), num(1));
        else if (name == "MENU TEXT") gui.menuText(num(0), str(0));
        else if (name == "MENU KILL") gui.menuKill();
        else if (name == "MENU VALUE") guiResult = gui.value(num(0));
        else if (name == "ON MENU") {
            try {
                guiResult = gui.onMenu(guiOut) ? 1 : 0;
            } catch (const brt::Gui::EndOfInput&) {
                guiResult = -1;
            }
        }
        const std::streamoff used = in.tellg();
        inputPos = used < 0 ? options.input.size() : inputPos + static_cast<std::size_t>(used);
        if (guiText.size() > 255) guiText.resize(255);
        result.console += guiOut.buf;
        guiOut.buf.clear();
        guiNums.clear();
        guiStrs.clear();
    };
    cpu.writeOverride = [&](m68k::Address a, m68k::Size s, m68k::Long v) {
        if (a == kTestConsole && s == m68k::Size::B) {
            result.console += static_cast<char>(v & 0xFF);
            return true;
        }
        if (s != m68k::Size::L) return false;
        if (a == kGuiNum) guiNums.push_back(static_cast<std::int32_t>(v));
        else if (a == kGuiStrAddr) guiStrAddr = v;
        else if (a == kGuiStrLen) guiStrs.push_back(readString(guiStrAddr, v));
        else if (a == kGuiCommand) guiCommand(v);
        else if (a == kGfxArg) gfxArgs.push_back(static_cast<std::int32_t>(v));
        else if (a == kGfxStrAddr) strAddr = v;
        else if (a == kGfxStrLen) strLen = v;
        else if (a == kGfxMode) gfxMode = v;
        else if (a == kGfxCommand) {
            std::string str;
            for (std::uint32_t k = 0; k < strLen; ++k) str += static_cast<char>(memory.read8(static_cast<m68k::Address>(strAddr + k)));
            if (static_cast<int>(v) < brt::kGfxCommandCount) gfx.command(brt::kGfxCommands[v], gfxArgs, str, static_cast<int>(gfxMode));
            gfxArgs.clear();
            strAddr = strLen = gfxMode = 0;
        } else {
            return false;
        }
        return true;
    };
    cpu.readOverride = [&](m68k::Address a, m68k::Size s, m68k::Long& v) {
        if (s == m68k::Size::L && (a == kGuiResult || a == kGuiResultLen || a == kGuiResultChar)) {
            if (a == kGuiResult) v = static_cast<m68k::Long>(guiResult);
            else if (a == kGuiResultLen) v = static_cast<m68k::Long>(guiText.size());
            else v = guiTextPos < guiText.size() ? static_cast<std::uint8_t>(guiText[guiTextPos++]) : 0;
            return true;
        }
        if (a == kGfxPoint && s == m68k::Size::L) {  // POINT(x, y) of the last two arguments
            const std::int32_t x = gfxArgs.size() >= 2 ? gfxArgs[gfxArgs.size() - 2] : 0;
            const std::int32_t y = gfxArgs.empty() ? 0 : gfxArgs.back();
            v = static_cast<m68k::Long>(gfx.point(x, y));
            gfxArgs.clear();
            return true;
        }
        if (a != kTestInput || s != m68k::Size::L) return false;
        v = inputPos < options.input.size() ? static_cast<std::uint8_t>(options.input[inputPos++]) : 0xFFFFFFFFu;
        return true;
    };
    m68k::CpuState st = cpu.state();
    st.regs.pc = options.origin;
    st.regs.a[7] = kTestStackTop;
    cpu.setState(st);
    const m68k::RunResult run = cpu.run(options.maxInstructions);
    result.instructions = run.instructions;
    if (gfx.used()) result.gfx = gfx.digest();
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
