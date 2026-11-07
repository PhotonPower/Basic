#pragma once

// Abstract syntax tree. The parser produces one statement per source line; block structure
// (IF ... ENDIF, FOR ... NEXT, PROCEDURE ... RETURN) is checked and built by the semantic pass,
// like the structure test (F10) of the GFA editor.

#include <memory>
#include <string>
#include <vector>

namespace basic::ast {

struct Pos {
    int line = 0;
    int col = 0;
};

enum class ExprKind {
    Number,  // text = literal as written
    String,  // text = contents
    Var,     // text = name with postfix; args = indices; wholeArray for a()
    Call,    // text = function name; args; user = @name / FN name
    Unary,   // text = "-", "+", "NOT" or "#" (file channel argument of a command); args[0]
    Binary,  // text = operator ("+", "MOD", "AND", "==", ...); args[0], args[1]
};

struct Expr {
    ExprKind kind = ExprKind::Number;
    Pos pos;
    std::string text;
    std::vector<std::unique_ptr<Expr>> args;
    bool wholeArray = false;  // Var: a()
    bool parens = false;      // Var/Call: written with parentheses
    bool user = false;        // Call: user function (@f / FN f)
};

using ExprPtr = std::unique_ptr<Expr>;

enum class StmtKind {
    Assign,      // args: target, value
    Print,       // channel (optional), usingFmt (optional), items
    If, ElseIf, Else, EndIf,
    For,         // args: var, start, end, [step]; variant 1 = DOWNTO
    Next,        // args: [var]
    While, Wend,
    Repeat, Until,
    Do, Loop,    // variant 0 plain, 1 WHILE, 2 UNTIL; args: [cond]
    ExitIf,
    Select, Case, Default, Cont, EndSelect,
    Procedure, Function, EndFunc,  // name, params
    Return,      // args: [value]
    Local,       // args: variables
    Call,        // name, args (procedure call: bare, @name or GOSUB)
    Goto,        // name = label
    Label,       // name
    Dim,         // args: arrays with dimensions
    Data,        // data
    Read,        // args: targets
    Restore,     // name = label or empty
    DefFn,       // name, params, args: [expression]
    OnError,     // name = procedure, empty for ON ERROR
    Resume,      // variant 0 plain, 1 NEXT, 2 label (name)
    Void,        // args: [expression]
    Command,     // any other command: name, args (comma separated expressions)
};

struct PrintItem {
    std::unique_ptr<Expr> expr;  // may be null (e.g. PRINT ,x)
    char sep = 0;                // ';' ',' '\'' or 0 (none)
};

struct CaseItem {
    std::unique_ptr<Expr> lo, hi;  // CASE x: lo; CASE x TO y; CASE TO y: hi only; CASE x TO: lo only
    bool range = false;
};

struct Param {
    std::string name;
    bool byRef = false;  // VAR
    bool array = false;  // name()
};

struct DataItem {
    std::string text;
    bool quoted = false;
};

struct Stmt {
    StmtKind kind = StmtKind::Command;
    Pos pos;
    std::string name;
    std::vector<ExprPtr> args;
    int variant = 0;
    ExprPtr channel;   // PRINT #n
    ExprPtr usingFmt;  // PRINT USING f$
    std::vector<PrintItem> print;
    std::vector<CaseItem> cases;
    std::vector<Param> params;
    std::vector<DataItem> data;
};

struct Program {
    std::vector<Stmt> stmts;
};

std::string dump(const Expr& e);
std::string dump(const Stmt& s);
std::string dump(const Program& p);

}  // namespace basic::ast
