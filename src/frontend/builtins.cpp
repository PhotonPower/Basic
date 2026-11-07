#include "builtins.hpp"

#include <array>

namespace basic {

namespace {

// Reference chapters 2, 4, 5, 7. 'x' arguments accept numbers or strings (PRED/SUCC, MIN/MAX,
// STRING$ second argument, INSTR with leading position).
constexpr std::array kBuiltins = {
    Builtin{"ABS", 'f', 1, 1, "n"},       Builtin{"SGN", 'i', 1, 1, "n"},
    Builtin{"INT", 'f', 1, 1, "n"},       Builtin{"TRUNC", 'f', 1, 1, "n"},
    Builtin{"FIX", 'f', 1, 1, "n"},       Builtin{"FRAC", 'f', 1, 1, "n"},
    Builtin{"CINT", 'i', 1, 1, "n"},      Builtin{"CFLOAT", 'f', 1, 1, "n"},
    Builtin{"ROUND", 'f', 1, 2, "nn"},    Builtin{"SQR", 'f', 1, 1, "n"},
    Builtin{"ODD", 'i', 1, 1, "n"},       Builtin{"EVEN", 'i', 1, 1, "n"},
    Builtin{"MIN", '?', 1, -1, "x"},      Builtin{"MAX", '?', 1, -1, "x"},
    Builtin{"RND", 'f', 0, 1, "n"},       Builtin{"RANDOM", 'i', 1, 1, "n"},
    Builtin{"RAND", 'i', 1, 1, "n"},      Builtin{"ADD", 'i', 2, 2, "nn"},
    Builtin{"SUB", 'i', 2, 2, "nn"},      Builtin{"MUL", 'i', 2, 2, "nn"},
    Builtin{"DIV", 'i', 2, 2, "nn"},      Builtin{"MOD", 'i', 2, 2, "nn"},
    Builtin{"PRED", '?', 1, 1, "x"},      Builtin{"SUCC", '?', 1, 1, "x"},
    Builtin{"BCLR", 'i', 2, 2, "nn"},     Builtin{"BSET", 'i', 2, 2, "nn"},
    Builtin{"BCHG", 'i', 2, 2, "nn"},     Builtin{"BTST", 'i', 2, 2, "nn"},
    Builtin{"SHL", 'i', 2, 2, "nn"},      Builtin{"SHR", 'i', 2, 2, "nn"},
    Builtin{"SHL&", 'i', 2, 2, "nn"},     Builtin{"SHR&", 'i', 2, 2, "nn"},
    Builtin{"SHL|", 'i', 2, 2, "nn"},     Builtin{"SHR|", 'i', 2, 2, "nn"},
    Builtin{"ROL", 'i', 2, 2, "nn"},      Builtin{"ROR", 'i', 2, 2, "nn"},
    Builtin{"ROL&", 'i', 2, 2, "nn"},     Builtin{"ROR&", 'i', 2, 2, "nn"},
    Builtin{"ROL|", 'i', 2, 2, "nn"},     Builtin{"ROR|", 'i', 2, 2, "nn"},
    Builtin{"AND", 'i', 2, 2, "nn"},      Builtin{"OR", 'i', 2, 2, "nn"},
    Builtin{"XOR", 'i', 2, 2, "nn"},      Builtin{"IMP", 'i', 2, 2, "nn"},
    Builtin{"EQV", 'i', 2, 2, "nn"},      Builtin{"SWAP", 'i', 1, 1, "n"},
    Builtin{"BYTE", 'i', 1, 1, "n"},      Builtin{"CARD", 'i', 1, 1, "n"},
    Builtin{"WORD", 'i', 1, 1, "n"},      Builtin{"TRUE", 'i', 0, 0, ""},
    Builtin{"FALSE", 'i', 0, 0, ""},      Builtin{"PI", 'f', 0, 0, ""},
    Builtin{"ERR", 'i', 0, 0, ""},        Builtin{"TIMER", 'i', 0, 0, ""},
    Builtin{"EXP", 'f', 1, 1, "n"},       Builtin{"LOG", 'f', 1, 1, "n"},
    Builtin{"LOG10", 'f', 1, 1, "n"},     Builtin{"SIN", 'f', 1, 1, "n"},
    Builtin{"COS", 'f', 1, 1, "n"},       Builtin{"TAN", 'f', 1, 1, "n"},
    Builtin{"ASIN", 'f', 1, 1, "n"},      Builtin{"ACOS", 'f', 1, 1, "n"},
    Builtin{"ATN", 'f', 1, 1, "n"},       Builtin{"SINQ", 'f', 1, 1, "n"},
    Builtin{"COSQ", 'f', 1, 1, "n"},      Builtin{"DEG", 'f', 1, 1, "n"},
    Builtin{"RAD", 'f', 1, 1, "n"},       Builtin{"LEFT$", 's', 1, 2, "sn"},
    Builtin{"RIGHT$", 's', 1, 2, "sn"},   Builtin{"MID$", 's', 2, 3, "snn"},
    Builtin{"LEN", 'i', 1, 1, "s"},       Builtin{"INSTR", 'i', 2, 3, "xxx"},
    Builtin{"RINSTR", 'i', 2, 3, "xxx"},  Builtin{"STRING$", 's', 2, 2, "nx"},
    Builtin{"SPACE$", 's', 1, 1, "n"},    Builtin{"CHR$", 's', 1, 1, "n"},
    Builtin{"ASC", 'i', 1, 1, "s"},       Builtin{"STR$", 's', 1, 3, "nnn"},
    Builtin{"VAL", 'f', 1, 1, "s"},       Builtin{"VAL?", 'i', 1, 1, "s"},
    Builtin{"UPPER$", 's', 1, 1, "s"},    Builtin{"TRIM$", 's', 1, 1, "s"},
    Builtin{"HEX$", 's', 1, 2, "nn"},     Builtin{"BIN$", 's', 1, 2, "nn"},
    Builtin{"OCT$", 's', 1, 2, "nn"},     Builtin{"ERR$", 's', 1, 1, "n"},
    Builtin{"MKI$", 's', 1, 1, "n"},      Builtin{"MKL$", 's', 1, 1, "n"},
    Builtin{"MKS$", 's', 1, 1, "n"},      Builtin{"MKD$", 's', 1, 1, "n"},
    Builtin{"CVI", 'i', 1, 1, "s"},       Builtin{"CVL", 'i', 1, 1, "s"},
    Builtin{"CVS", 'f', 1, 1, "s"},       Builtin{"CVD", 'f', 1, 1, "s"},
    Builtin{"POS", 'i', 1, 1, "n"},       Builtin{"INKEY$", 's', 0, 0, ""},
    Builtin{"DIM?", 'i', 1, 1, "a"},      Builtin{"TAB", 'i', 1, 1, "n", true},
    Builtin{"SPC", 'i', 1, 1, "n", true}, Builtin{"AT", 'i', 2, 2, "nn", true},
    Builtin{"EOF", 'i', 1, 1, "c"},       Builtin{"LOF", 'i', 1, 1, "c"},
    Builtin{"EXIST", 'i', 1, 1, "s"},     Builtin{"INPUT$", 's', 1, 2, "nc"},
    Builtin{"LOC", 'i', 1, 1, "c"},       Builtin{"INP", 'i', 1, 1, "c"},
    Builtin{"VARPTR", 'i', 1, 1, "v"},    Builtin{"PEEK", 'i', 1, 1, "n"},
    Builtin{"DPEEK", 'i', 1, 1, "n"},     Builtin{"LPEEK", 'i', 1, 1, "n"},
    Builtin{"MALLOC", 'i', 1, 2, "nn"},   Builtin{"MFREE", 'i', 2, 2, "nn"},
};

}  // namespace

const Builtin* findBuiltin(std::string_view name) {
    for (const Builtin& b : kBuiltins)
        if (b.name == name) return &b;
    return nullptr;
}

}  // namespace basic
