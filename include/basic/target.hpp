#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

/// Targets of the compiler: a target is a combination of a CPU backend and a platform
/// (see docs/targets.md). Every BASIC statement belongs to a command group; a platform
/// declares which groups it supports, so that the compiler can reject a program that uses
/// something the target cannot do (see docs/language.md).
namespace basic {

enum class Cpu { Host, M68000, Z80 };

/// Command groups. The values are bits so that sets of groups fit into one integer.
enum class Group : std::uint32_t {
    Core = 1u << 0,        ///< variables, control flow, procedures, integer arithmetic
    Float = 1u << 1,       ///< floating point arithmetic and functions
    String = 1u << 2,      ///< string variables and functions
    Print = 1u << 3,       ///< PRINT, PRINT USING, text output
    Input = 1u << 4,       ///< INPUT, INKEY$, keyboard
    FileSeq = 1u << 5,     ///< sequential files (OPEN, PRINT #, INPUT #, CLOSE)
    FileRandom = 1u << 6,  ///< random access files (FIELD, GET, PUT, SEEK), several channels
    Memory = 1u << 7,      ///< PEEK/POKE, VARPTR, BYTE{} ..., MALLOC, ABSOLUTE
    Graphics = 1u << 8,    ///< PLOT, DRAW, LINE, COLOR, ... (chapter 8)
    Gui = 1u << 9,         ///< screens, windows, menus, requesters, events (chapter 9)
    Sprites = 1u << 10,    ///< sprites, BOBs, joystick (chapter 10)
    Sound = 1u << 11,      ///< sound and speech
    Matrix = 1u << 12,     ///< vectors and matrices (chapter 13, GFA-BASIC 3.5)
    AmigaOs = 1u << 13,    ///< direct AmigaOS library calls (chapter 11)
};

using GroupSet = std::uint32_t;

[[nodiscard]] constexpr GroupSet operator|(Group a, Group b) {
    return static_cast<GroupSet>(a) | static_cast<GroupSet>(b);
}
[[nodiscard]] constexpr GroupSet operator|(GroupSet a, Group b) { return a | static_cast<GroupSet>(b); }
[[nodiscard]] constexpr bool contains(GroupSet set, Group g) { return (set & static_cast<GroupSet>(g)) != 0; }

/// All groups in declaration order (for listings).
[[nodiscard]] std::span<const Group> allGroups();
/// Short lower-case name used on the command line and in the docs ("file-random").
[[nodiscard]] std::string_view groupName(Group group);
[[nodiscard]] std::string_view cpuName(Cpu cpu);

struct TargetInfo {
    std::string_view name;         ///< command line name, e.g. "cpc"
    Cpu cpu;
    std::string_view platform;     ///< platform layer, directory name under platform/
    std::string_view output;       ///< what the compiler writes
    std::string_view description;
    GroupSet supported;            ///< groups the platform implements (planned scope)
};

/// The known targets (pc, amiga, cpc, z80-test, m68k-test).
[[nodiscard]] std::span<const TargetInfo> targets();
[[nodiscard]] std::optional<TargetInfo> findTarget(std::string_view name);

/// Human-readable table of all targets and their groups.
[[nodiscard]] std::string formatTargets();

}  // namespace basic
