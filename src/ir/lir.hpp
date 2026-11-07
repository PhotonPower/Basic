#pragma once

// LIR: three-address code in basic blocks for the assembler backends (docs/ir.md, D17).
// Values live in typed virtual registers; floating point and everything that is not a simple
// integer operation is a runtime call (rtcall), so that every backend can decide what to inline.

#include <cstdint>
#include <string>
#include <vector>

#include "hir.hpp"

namespace basic::lir {

enum class VT : std::uint8_t { I32, F64 };  // types of virtual registers

struct Reg {
    int id = -1;
    VT type = VT::I32;
};

enum class Kind : std::uint8_t {
    ConstI,   // dst = imm
    ConstF,   // dst = fimm
    Load,     // dst = var (widened: Bool/Byte/Word -> I32)
    Store,    // var = a (the value already fits the variable)
    Op,       // dst = a <op> b (I32): add sub and or xor not neg (b unused for not/neg)
    Cmp,      // dst = (a <cond> b) ? -1 : 0, I32 operands
    RtCall,   // [dst =] symbol(args): runtime routine, conventions in docs/runtime.md
    Call,     // [dst =] routine(args) (procedure or function of the program)
    Jump,     // goto label
    BranchZ,  // if a == 0 goto label
    BranchNZ, // if a != 0 goto label
    Label,    // label:
    Return,   // return [a] (functions)
    Save,     // push the value of var (LOCAL, parameters)
    Clear,    // var = 0 (after Save: LOCAL)
    Restore,  // pop the value of var
    PrintStr, // print the string constant `text`
    Line,     // BASIC source line (statement boundary)
};

struct Instr {
    Kind kind = Kind::Label;
    Reg dst, a, b;
    std::int32_t imm = 0;
    std::uint64_t fimm = 0;
    int var = -1;
    int label = -1;
    int routine = -1;
    std::string op;      // Op, Cmp: "add", "eq", ...; RtCall: runtime symbol
    std::string text;    // PrintStr
    std::vector<Reg> args;
};

struct Function {
    std::string name;          // "main" or the routine name
    int routine = -1;          // index in hir::Program::routines, -1 for main
    std::vector<Reg> params;   // incoming arguments (stored into the parameter variables)
    std::vector<Instr> code;
    int regs = 0;              // number of virtual registers
    std::vector<VT> regTypes;
    int labels = 0;
    bool returnsValue = false;
    VT result = VT::F64;
};

struct Program {
    std::vector<Function> functions;  // main first
    const hir::Program* hir = nullptr;
};

std::string dump(const Program& p);

}  // namespace basic::lir
