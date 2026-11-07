#pragma once

// Lexer for GFA-BASIC .LST sources (docs/architecture.md#frontend): one statement per line, case
// insensitive, comments with REM, ' (at the start) and ! (at the end, except in DATA lines).

#include <string>
#include <string_view>
#include <vector>

#include "diagnostic.hpp"

namespace basic {

enum class Tok {
    Ident,   // name in upper case, including a type postfix (! | & % # $), e.g. "A%", "LEFT$"
    Number,  // literal as written: 12, 1.5E3, .5, &HFF, $FF, &O17, &X101, %101
    String,  // contents without the quotes
    Punct,   // + - * / ^ \ = == <> < > <= >= ( ) , ; ' : @ # ~ .
    End,
};

struct Token {
    Tok kind = Tok::End;
    std::string text;
    int col = 0;  // 1-based
    bool spaceBefore = false;
};

struct SourceLine {
    int number = 0;       // 1-based line number in the file
    std::string text;     // the line without line terminator
    std::vector<Token> tokens;  // empty for blank and comment lines; always ends with Tok::End
};

// Splits the source into lines and tokens. Lexical errors are appended to `diags`.
std::vector<SourceLine> lexSource(std::string_view source, Diagnostics& diags);

// Tokens of one line (used by the lexer and for DATA/label handling in tests).
std::vector<Token> lexLine(std::string_view line, int lineNumber, Diagnostics& diags);

}  // namespace basic
