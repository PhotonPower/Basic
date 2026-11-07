#pragma once

// Built-in functions and constants of GFA-BASIC that can appear in expressions. The parser needs
// the names to tell a function call from an array element; the semantic pass uses the rest.

#include <string_view>

namespace basic {

struct Builtin {
    std::string_view name;   // upper case, with postfix ("LEFT$", "VAL?")
    char result;             // 'f' float, 'i' integer, 's' string, '?' like the arguments (MIN/MAX)
    int minArgs, maxArgs;    // maxArgs -1 = any number
    std::string_view args;   // per argument: 'n' numeric, 's' string, 'x' numeric or string, 'a' array
    bool printOnly = false;  // TAB, SPC, AT: only inside PRINT
};

const Builtin* findBuiltin(std::string_view name);

}  // namespace basic
