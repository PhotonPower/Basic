#pragma once

// Parser: recursive descent over one line at a time. Operator precedence as determined on the
// original (docs/language.md, probe4/probe5), from strong to weak:
//   ^ (left assoc) | unary - + | * / | MOD DIV \ | + - | comparisons | AND OR XOR IMP EQV | NOT

#include <string_view>

#include "ast.hpp"
#include "diagnostic.hpp"

namespace basic {

// Parses a whole program. Syntax errors are appended to `diags`; the statement concerned is left
// out of the program.
ast::Program parseProgram(std::string_view source, Diagnostics& diags);

}  // namespace basic
