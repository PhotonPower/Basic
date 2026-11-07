#pragma once

// HIR: the typed, structured program after semantic analysis (docs/ir.md).
//
// Variables: GFA-BASIC binds names dynamically (LOCAL variables are visible in called routines).
// Every variable therefore has exactly one storage slot; LOCAL and parameters save the old value
// on entry and restore it on exit. Variables are identified by their index in Program::vars.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace basic::hir {

enum class Type : std::uint8_t { Bool, Byte, Word, Int, Float, Str };

inline bool isNumeric(Type t) { return t != Type::Str; }
inline bool isInteger(Type t) { return t == Type::Bool || t == Type::Byte || t == Type::Word || t == Type::Int; }
const char* typeName(Type t);
char typePostfix(Type t);

struct Var {
    std::string name;  // without postfix, upper case
    Type type = Type::Float;
    bool array = false;
    std::string display() const;  // e.g. "A%", "B$()"
};

enum class Op : std::uint8_t {
    ConstInt,    // ival
    ConstFloat,  // fval (binary64 bit pattern)
    ConstStr,    // sval
    Load,        // var
    LoadElem,    // var, args = indices (Int)
    Conv,        // args[0] -> type, with the semantics of an assignment (truncation, range errors 2/3/4)
    ToInt,       // args[0] (Float) -> Int by truncation, range error 2 (operands of AND, DIV, SELECT ...)
    Neg, Add, Sub, Mul, Div, Pow,  // Float
    IDiv, IMod,                    // Int: truncation towards 0, remainder with the dividend's sign
    Not, And, Or, Xor, Imp, Eqv,   // Int, bitwise
    Eq, Ne, Lt, Gt, Le, Ge,        // args Float or Str; result Int (-1 / 0)
    ApproxEq,                      // ==, args Float; result Int
    Concat,                        // Str
    Builtin,                       // name, args converted to the declared types
    CallFunc,                      // func (index in Program::funcs), args
};

struct Expr {
    Op op = Op::ConstInt;
    Type type = Type::Int;
    int var = -1;
    int func = -1;
    std::int32_t ival = 0;
    std::uint64_t fval = 0;
    std::string sval;  // ConstStr: value; Builtin: name
    std::vector<std::unique_ptr<Expr>> args;
};

using ExprPtr = std::unique_ptr<Expr>;

struct Stmt;
using Block = std::vector<std::unique_ptr<Stmt>>;

enum class SKind : std::uint8_t {
    Assign,    // target (Load/LoadElem), value (converted to the target's type)
    Print,     // items; channel; usingFmt
    If,        // branches (cond, body); elseBody
    For,       // target, start (converted), end, step (Float); body
    While,     // cond, body
    Repeat,    // body, cond (until)
    Do,        // pre (0 none, 1 while, 2 until), cond; body; post, cond2
    ExitIf,    // cond
    Select,    // value (Int); cases; elseBody
    Call,      // proc, args (by value: converted; VAR: Load of the variable / whole array)
    Return,    // end of procedure / RETURN value in a function (value)
    Local,     // vars: saved, cleared, restored at the end of the routine
    Goto,      // label
    Label,     // label
    Read,      // targets
    Restore,   // label (-1: first DATA)
    OnError,   // proc (-1: ON ERROR)
    Resume,    // mode 0 plain, 1 NEXT, 2 label
    Cmd,       // name, args, targets: commands without own structure (INC, SWAP, DIM, END ...)
};

struct PrintItem {
    enum Kind : std::uint8_t { Value, Tab, Spc } kind = Value;
    ExprPtr expr;  // may be null
    char sep = 0;  // ';' ',' '\'' or 0
};

struct CaseItem {
    ExprPtr lo, hi;  // Int; range: lo/hi may be null (open)
    bool range = false;
};

struct Case {
    std::vector<CaseItem> items;
    Block body;
    bool cont = false;  // CONT at the end: fall through into the next block
};

struct Branch {
    ExprPtr cond;
    Block body;
};

struct Stmt {
    SKind kind = SKind::Cmd;
    int line = 0;
    std::string name;  // Cmd
    ExprPtr target, value, start, end, step, cond, cond2, channel, usingFmt;
    std::vector<ExprPtr> args, targets;
    std::vector<PrintItem> items;
    std::vector<Branch> branches;
    std::vector<Case> cases;
    Block body, elseBody;
    std::vector<int> vars;
    int proc = -1;
    int label = -1;
    int pre = 0, post = 0, mode = 0;
    bool downto = false;
};

struct Param {
    int var = -1;
    bool byRef = false;
};

struct Routine {
    std::string name;
    enum Kind : std::uint8_t { Procedure, Function, DefFn } kind = Procedure;
    Type result = Type::Float;  // functions
    std::vector<Param> params;
    Block body;     // Procedure, Function
    ExprPtr expr;   // DefFn
    int line = 0;
};

struct DataItem {
    std::string text;
    bool quoted = false;
    int line = 0;
};

struct Label {
    std::string name;
    int routine = -1;  // -1: main program
    int data = 0;      // index of the first DATA item after the label (RESTORE)
};

struct Program {
    std::vector<Var> vars;
    std::vector<Routine> routines;  // procedures, functions and DEFFN
    std::vector<Label> labels;
    std::vector<DataItem> data;
    Block main;
};

std::string dump(const Program& p);
std::string dump(const Program& p, const Expr& e);

}  // namespace basic::hir
