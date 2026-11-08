#include "basic/target.hpp"

#include <array>
#include <sstream>

namespace basic {

namespace {

constexpr std::array kGroups{Group::Core,     Group::Float,    Group::String,     Group::Print,
                             Group::Input,    Group::FileSeq,  Group::FileRandom, Group::Memory,
                             Group::Graphics, Group::Gui,      Group::Sprites,    Group::Sound,
                             Group::Matrix,   Group::AmigaOs};

constexpr GroupSet kComputing = Group::Core | Group::Float | Group::String | Group::Print | Group::Memory |
                                Group::Matrix;

constexpr std::array kTargets{
    TargetInfo{"pc", Cpu::Host, "pc", "C++ source + runtime (reference, tests)",
               "C++ on the PC; reference implementation for the differential tests",
               kComputing | Group::Input | Group::FileSeq | Group::FileRandom},
    TargetInfo{"amiga", Cpu::M68000, "amiga", "AmigaDOS executable (hunk)",
               "Commodore Amiga, MC68000, AmigaOS (native platform of GFA-BASIC)",
               kComputing | Group::Input | Group::FileSeq | Group::FileRandom | Group::Graphics |
                   Group::Gui | Group::Sprites | Group::Sound | Group::AmigaOs},
    TargetInfo{"cpc", Cpu::Z80, "cpc", "binary with AMSDOS header (+ .sna snapshot)",
               "Amstrad CPC, Z80, firmware and AMSDOS", kComputing | Group::Input | Group::FileSeq |
                                                         Group::Graphics},
    TargetInfo{"z80-test", Cpu::Z80, "z80-test", "raw binary",
               "bare Z80 (emulator, FPGA template): console on port &01, end with HALT",
               kComputing | Group::Input | Group::FileSeq},
    TargetInfo{"m68k-test", Cpu::M68000, "m68k-test", "raw binary / S-Record",
               "bare MC68000 (emulator, FPGA template): console at $00FF0000, end with STOP",
               kComputing | Group::Input | Group::FileSeq},
};

}  // namespace

std::span<const Group> allGroups() { return kGroups; }

std::string_view groupName(Group group) {
    switch (group) {
        case Group::Core: return "core";
        case Group::Float: return "float";
        case Group::String: return "string";
        case Group::Print: return "print";
        case Group::Input: return "input";
        case Group::FileSeq: return "file-seq";
        case Group::FileRandom: return "file-random";
        case Group::Memory: return "memory";
        case Group::Graphics: return "graphics";
        case Group::Gui: return "gui";
        case Group::Sprites: return "sprites";
        case Group::Sound: return "sound";
        case Group::Matrix: return "matrix";
        case Group::AmigaOs: return "amiga-os";
    }
    return "?";
}

std::string_view cpuName(Cpu cpu) {
    switch (cpu) {
        case Cpu::Host: return "host";
        case Cpu::M68000: return "m68000";
        case Cpu::Z80: return "z80";
    }
    return "?";
}

std::span<const TargetInfo> targets() { return kTargets; }

std::optional<TargetInfo> findTarget(std::string_view name) {
    for (const TargetInfo& t : kTargets)
        if (t.name == name) return t;
    return std::nullopt;
}

std::string formatTargets() {
    std::ostringstream out;
    for (const TargetInfo& t : kTargets) {
        out << t.name << "  (" << cpuName(t.cpu) << ", platform " << t.platform << ")\n"
            << "    " << t.description << "\n"
            << "    output: " << t.output << "\n"
            << "    groups:";
        for (Group g : kGroups)
            if (contains(t.supported, g)) out << ' ' << groupName(g);
        out << "\n";
    }
    return out.str();
}

}  // namespace basic
