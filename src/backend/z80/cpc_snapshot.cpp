#include "backend/z80/cpc_snapshot.hpp"

#include <array>

#include "z80/assembler.hpp"
#include "z80/json.hpp"
#include "z80/session.hpp"
#include "z80/snapshot.hpp"

namespace basic {

namespace {

constexpr std::uint16_t kLoader = 0xA600;  // above the stack of the program (platform/cpc), below HIMEM

std::uint32_t number(const z80::Json* j) {
    if (j == nullptr) return 0;
    if (j->isNumber()) return static_cast<std::uint32_t>(j->asInt());
    return static_cast<std::uint32_t>(std::stoul(j->asString(), nullptr, 0));
}

}  // namespace

CpcSnapshot cpcSnapshot(const std::vector<std::uint8_t>& image, std::uint16_t origin, const std::string& romFile) {
    CpcSnapshot out;
    z80::Session session{z80::SessionOptions{true}};
    auto cmd = [&](const z80::Json& request) {
        const z80::Json r = session.handle(request);
        const z80::Json* ok = r.find("ok");
        if ((ok == nullptr || !ok->asBool()) && out.error.empty()) out.error = request.dump() + ": " + r.dump();
        return r;
    };
    cmd(z80::Json::parse(R"({"cmd":"machine","type":"cpc"})"));
    z80::Json load = z80::Json::object();
    load.set("cmd", "load_rom").set("file", romFile).set("which", "both");
    cmd(load);
    cmd(z80::Json::parse(R"({"cmd":"set","regs":{"PC":0,"SP":"0xBFFF"}})"));
    cmd(z80::Json::parse(R"({"cmd":"run","irq_every_cycles":13312,"max_instructions":30000000})"));  // the BASIC prompt
    const z80::Json video = cmd(z80::Json::parse(R"({"cmd":"video"})"));
    if (!out.error.empty()) return out;

    z80::Registers& r = session.cpu().regs();
    out.resume = r.pc;
    out.loader = kLoader;
    const std::string loader =
        "        PUSH    AF\n        PUSH    BC\n        PUSH    DE\n        PUSH    HL\n        PUSH    IX\n        PUSH    IY\n"
        "        CALL    &B909\n"  // KL L ROM DISABLE -> A = the old state
        "        PUSH    AF\n"
        "        CALL    " + std::to_string(origin) + "\n"
        "        POP     AF\n"
        "        CALL    &B90C\n"  // KL ROM RESTORE
        "        POP     IY\n        POP     IX\n        POP     HL\n        POP     DE\n        POP     BC\n        POP     AF\n"
        "        JP      " + std::to_string(out.resume) + "\n";
    z80::AssemblerOptions options;
    options.origin = kLoader;
    const z80::AssemblyResult assembled = z80::Assembler(options).assemble(loader);
    if (!assembled.ok()) {
        out.error = "loader: " + assembled.formatDiagnostics("<loader>");
        return out;
    }
    if (origin + image.size() > kLoader - 0x400) {
        out.error = "the program reaches the stack and the loader";
        return out;
    }
    z80::Memory& m = session.memory();
    for (std::size_t k = 0; k < image.size(); ++k) m.write(static_cast<z80::Word>(origin + k), image[k]);
    const std::vector<std::uint8_t> code = assembled.flatten();
    out.done = static_cast<std::uint16_t>(kLoader + code.size() - 3);
    for (std::size_t k = 0; k < code.size(); ++k) m.write(static_cast<z80::Word>(kLoader + k), code[k]);

    // version 1 header (256 bytes), then the 64 KiB of RAM
    std::vector<std::uint8_t>& f = out.bytes;
    f.assign(0x100 + 0x10000, 0);
    const std::string magic = "MV - SNA";
    for (std::size_t k = 0; k < magic.size(); ++k) f[k] = static_cast<std::uint8_t>(magic[k]);
    f[0x10] = 1;
    auto reg = [&](const char* name) { return z80::getRegister(r, name).value_or(0); };
    auto w16 = [&](std::size_t at, std::uint32_t v) {
        f[at] = static_cast<std::uint8_t>(v);
        f[at + 1] = static_cast<std::uint8_t>(v >> 8);
    };
    w16(0x11, (reg("F") & 0xFF) | (reg("A") & 0xFF) << 8);  // F A
    w16(0x13, reg("C") | reg("B") << 8);                     // C B
    w16(0x15, reg("E") | reg("D") << 8);                     // E D
    w16(0x17, reg("L") | reg("H") << 8);                     // L H
    f[0x19] = static_cast<std::uint8_t>(reg("R"));
    f[0x1A] = static_cast<std::uint8_t>(reg("I"));
    f[0x1B] = static_cast<std::uint8_t>(reg("IFF1"));
    f[0x1C] = static_cast<std::uint8_t>(reg("IFF2"));
    w16(0x1D, reg("IX"));
    w16(0x1F, reg("IY"));
    w16(0x21, reg("SP"));
    w16(0x23, kLoader);  // PC: the loader
    f[0x25] = static_cast<std::uint8_t>(reg("IM"));
    const std::uint32_t af2 = reg("AF'"), bc2 = reg("BC'"), de2 = reg("DE'"), hl2 = reg("HL'");
    f[0x26] = static_cast<std::uint8_t>(af2);
    f[0x27] = static_cast<std::uint8_t>(af2 >> 8);
    f[0x28] = static_cast<std::uint8_t>(bc2);
    f[0x29] = static_cast<std::uint8_t>(bc2 >> 8);
    f[0x2A] = static_cast<std::uint8_t>(de2);
    f[0x2B] = static_cast<std::uint8_t>(de2 >> 8);
    f[0x2C] = static_cast<std::uint8_t>(hl2);
    f[0x2D] = static_cast<std::uint8_t>(hl2 >> 8);
    // Gate Array: pens, border, mode and ROM state
    if (const z80::Json* pens = video.find("pens"))
        for (const z80::Json& p : pens->items()) {
            const std::uint32_t pen = number(p.find("pen"));
            if (pen < 16) f[0x2F + pen] = static_cast<std::uint8_t>(number(p.find("hw")) & 0x1F);
        }
    f[0x3F] = f[0x2F];  // border: as pen 0 after the cold start (blue)
    const z80::Json* lower = video.find("lower_rom");
    const z80::Json* upper = video.find("upper_rom");
    f[0x40] = static_cast<std::uint8_t>(0x80 | (number(video.find("mode")) & 3) | (lower && lower->asBool() ? 0 : 4) | (upper && upper->asBool() ? 0 : 8));
    // CRTC: the firmware's values (those the emulator reports, else the defaults of a CPC)
    std::array<std::uint8_t, 18> crtc = {63, 40, 46, 0x8E, 38, 0, 25, 30, 0, 7, 0, 0, 0x30, 0, 0, 0, 0, 0};
    if (const z80::Json* c = video.find("crtc"))
        for (const auto& [name, value] : c->members())
            if (name.size() > 1 && name[0] == 'R') {
                const unsigned k = static_cast<unsigned>(std::stoul(name.substr(1)));
                if (k < crtc.size()) crtc[k] = static_cast<std::uint8_t>(number(&value));
            }
    for (std::size_t k = 0; k < crtc.size(); ++k) f[0x43 + k] = crtc[k];
    f[0x59] = 0x82;      // PPI control: A out, B in, C out
    f[0x5B + 7] = 0x3F;  // PSG mixer: all channels off
    w16(0x6B, 64);       // 64 KiB
    for (unsigned a = 0; a < 0x10000; ++a) f[0x100 + a] = m.readRam(static_cast<z80::Word>(a));
    return out;
}

}  // namespace basic
