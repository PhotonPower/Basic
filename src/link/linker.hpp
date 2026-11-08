#pragma once

// Source-level linker for the assembler runtime (docs/runtime.md#linken). The assemblers of the
// sister projects have no linker; the compiler therefore picks the runtime modules a program
// needs and joins them into one assembler source.

#include <map>
#include <string>
#include <vector>

#include "frontend/diagnostic.hpp"

namespace basic::link {

enum class Section { Code, Data, Bss };

// One runtime module: a file with a header of ";! key value" lines (docs/runtime.md#modulformat).
struct Module {
    std::string name;
    std::string file;
    std::vector<std::string> provides;
    std::vector<std::string> requires_;
    std::string group;
    std::string doc;
    struct Part {
        Section section = Section::Code;
        std::string text;
    };
    std::vector<Part> parts;  // in source order; ";! section data|bss|code" switches
};

// Parses a module. Header lines start with ";!" (Z80/68000) and come before the code; a
// ";! section" line may also appear later to switch the section of the following lines.
Module parseModule(const std::string& text, const std::string& file, Diagnostics& diags);

// Reads all modules (*.s) of a directory, sorted by file name.
std::vector<Module> loadModules(const std::string& dir, Diagnostics& diags);

// Checks that every label of the module is either provided or starts with "<module>__" (or is a
// local label beginning with '.'); the assemblers know no other scoping.
void checkLabels(const Module& m, Diagnostics& diags);

struct Library {
    std::vector<Module> modules;
    std::map<std::string, std::size_t> bySymbol;  // symbol -> module index
};

// Builds the symbol index; reports symbols provided twice.
Library makeLibrary(std::vector<Module> modules, Diagnostics& diags);

struct Program {
    std::string code;   // generated code (the program itself)
    std::string data;   // generated data
    std::string bss;    // generated BSS
    std::vector<std::string> uses;      // runtime symbols the program refers to
    std::vector<std::string> provides;  // symbols the program defines for the runtime (e.g. main)
};

enum class Layout {
    Plain,     // one ORG program (m68k-test): code, data, then BSS (each EVEN)
    PlainZ80,  // the same for the Z80 (z80-test, cpc): no alignment; the label prg_end follows the BSS
    Sections,  // SECTION code/data/bss (amiga hunk program)
};

struct Result {
    std::string source;                  // the complete assembler source
    std::vector<std::string> modules;    // linked modules in output order
    std::string map;                     // which module was pulled in by which symbol
};

// Closure over `requires` starting from `roots` (the first one is the entry point, e.g.
// plat_start) and the program's uses, then the source: `header` (ORG ...), the code of the entry
// module, the program, the other modules, data, BSS.
Result link(const Library& lib, const Program& program, const std::vector<std::string>& roots, Layout layout,
            const std::string& header, Diagnostics& diags);

}  // namespace basic::link
