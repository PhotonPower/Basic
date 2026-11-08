#include "parser.hpp"

#include <set>
#include <string>

#include "builtins.hpp"
#include "lexer.hpp"

namespace basic {

namespace {

using ast::Expr;
using ast::ExprKind;
using ast::ExprPtr;
using ast::Stmt;
using ast::StmtKind;

struct SyntaxError {
    int col;
    std::string message;
};

// Commands with the generic syntax "NAME [expr {, expr}]" (reference chapters 2, 4, 5, 7).
// Graphics and windows (chapters 8, 9; D25)
const std::set<std::string, std::less<>> kGfxCommands = {
    "OPENS", "CLOSES", "OPENW", "CLOSEW", "CLEARW", "TITLEW", "TITLES", "FRONTS", "BACKS", "FRONTW", "BACKW",
    "COLOR", "SETCOLOR", "GRAPHMODE", "DEFFILL", "BOUNDARY", "DEFLINE", "PLOT", "LINE", "DRAW", "BOX",
    "PBOX", "CIRCLE", "PCIRCLE", "ELLIPSE", "PELLIPSE", "TEXT",
};

const std::set<std::string, std::less<>> kCommands = {
    "INC",    "DEC",       "ADD",    "SUB",    "MUL",    "DIV",    "SWAP",   "CLR",     "ERASE",    "ABSOLUTE", "VSYNC",
    "ALERT",  "FILESELECT", "MENU",  "SLEEP",  "RESERVE", "MKDIR",  "RMDIR",  "CHDIR",   "WAVE", "MOUSE",
    "DEFNUM", "MODE",      "RANDOMIZE", "ERROR", "QUIT", "SYSTEM", "STOP",   "END",     "EDIT",
    "CLEAR",  "LSET",      "RSET",   "DEFINT", "DEFFLT", "DEFSTR", "DEFBIT", "DEFBYT",  "DEFWRD",
    "DEFSNG", "DEFDBL",    "PAUSE",  "DELAY",  "CLS",    "NEW",    "ARRAYFILL", "INSERT", "DELETE",
    "OPTION", "TRON",      "TROFF",  "WRITE",  "LOCATE", "HTAB",   "VTAB",   "INPUT",   "POKE",
    "DPOKE",  "LPOKE",     "BMOVE",  "OPEN",   "CLOSE",  "KILL",   "NAME",   "RENAME",  "SOUND",
    "FIELD",  "GET",       "PUT",    "RECORD", "SEEK",   "RELSEEK", "OUT",
};

class Parser {
public:
    Parser(const SourceLine& line, Diagnostics& diags, const std::set<std::string>* procs = nullptr)
        : line_(line), toks_(line.tokens), diags_(diags), procs_(procs) {}

    // "2d_display": a number directly followed by a name is one name (procedure names may start
    // with digits); else a plain identifier
    static bool atDigitName(const std::vector<Token>& t, std::size_t i) {
        if (i + 1 >= t.size() || t[i].kind != Tok::Number || t[i + 1].kind != Tok::Ident || t[i + 1].spaceBefore) return false;
        for (char c : t[i].text)
            if (c < '0' || c > '9') return false;
        return true;
    }

    std::string procName(const char* what) {
        if (atDigitName(toks_, i_)) {
            std::string n = take().text;
            return n + take().text;
        }
        return expectIdent(what);
    }

    bool parse(Stmt& out) {
        try {
            out = statement();
            if (!at(Tok::End)) fail("unexpected '" + peek().text + "'");
            return true;
        } catch (const SyntaxError& e) {
            diags_.push_back({line_.number, e.col, e.message});
            return false;
        }
    }

private:
    const SourceLine& line_;
    const std::vector<Token>& toks_;
    Diagnostics& diags_;
    const std::set<std::string>* procs_ = nullptr;
    std::size_t i_ = 0;

    // ---- token helpers ----------------------------------------------------------------------
    const Token& peek(std::size_t ahead = 0) const {
        const std::size_t k = i_ + ahead;
        return k < toks_.size() ? toks_[k] : toks_.back();
    }
    bool at(Tok kind) const { return peek().kind == kind; }
    bool atPunct(std::string_view p, std::size_t ahead = 0) const {
        return peek(ahead).kind == Tok::Punct && peek(ahead).text == p;
    }
    bool atWord(std::string_view w, std::size_t ahead = 0) const {
        return peek(ahead).kind == Tok::Ident && peek(ahead).text == w;
    }
    const Token& take() { return toks_[i_ < toks_.size() - 1 ? i_++ : i_]; }
    bool acceptPunct(std::string_view p) {
        if (!atPunct(p)) return false;
        ++i_;
        return true;
    }
    bool acceptWord(std::string_view w) {
        if (!atWord(w)) return false;
        ++i_;
        return true;
    }
    [[noreturn]] void fail(const std::string& message) const { throw SyntaxError{peek().col, message}; }
    void expectPunct(std::string_view p) {
        if (!acceptPunct(p)) fail("'" + std::string(p) + "' expected");
    }
    void expectWord(std::string_view w) {
        if (!acceptWord(w)) fail(std::string(w) + " expected");
    }
    std::string expectIdent(const char* what) {
        if (!at(Tok::Ident)) fail(std::string(what) + " expected");
        return take().text;
    }
    ast::Pos pos() const { return {line_.number, peek().col}; }

    static ExprPtr make(ExprKind kind, ast::Pos p, std::string text) {
        auto e = std::make_unique<Expr>();
        e->kind = kind;
        e->pos = p;
        e->text = std::move(text);
        return e;
    }
    static ExprPtr binary(std::string op, ExprPtr l, ExprPtr r) {
        auto e = make(ExprKind::Binary, l->pos, std::move(op));
        e->args.push_back(std::move(l));
        e->args.push_back(std::move(r));
        return e;
    }

    // ---- expressions ------------------------------------------------------------------------
    ExprPtr expr() { return logic(); }

    ExprPtr logic() {
        ExprPtr l = comparison();
        for (;;) {
            const Token& t = peek();
            if (t.kind == Tok::Ident &&
                (t.text == "AND" || t.text == "OR" || t.text == "XOR" || t.text == "IMP" || t.text == "EQV")) {
                std::string op = take().text;
                l = binary(std::move(op), std::move(l), comparison());
            } else {
                return l;
            }
        }
    }

    ExprPtr comparison() {
        ExprPtr l = additive();
        for (;;) {
            const Token& t = peek();
            if (t.kind == Tok::Punct &&
                (t.text == "=" || t.text == "<>" || t.text == "<" || t.text == ">" || t.text == "<=" ||
                 t.text == ">=" || t.text == "==")) {
                std::string op = take().text;
                l = binary(std::move(op), std::move(l), additive());
            } else {
                return l;
            }
        }
    }

    ExprPtr additive() {
        ExprPtr l = modular();
        while (atPunct("+") || atPunct("-")) {
            std::string op = take().text;
            l = binary(std::move(op), std::move(l), modular());
        }
        return l;
    }

    ExprPtr modular() {
        ExprPtr l = multiplicative();
        for (;;) {
            if (atWord("MOD") || atWord("DIV")) {
                std::string op = take().text;
                l = binary(std::move(op), std::move(l), multiplicative());
            } else if (atPunct("\\")) {
                take();
                l = binary("\\", std::move(l), multiplicative());
            } else {
                return l;
            }
        }
    }

    ExprPtr multiplicative() {
        ExprPtr l = unary();
        while (atPunct("*") || atPunct("/")) {
            std::string op = take().text;
            l = binary(std::move(op), std::move(l), unary());
        }
        return l;
    }

    ExprPtr unary() {
        if (atPunct("-") || atPunct("+")) {
            const ast::Pos p = pos();
            std::string op = take().text;
            auto e = make(ExprKind::Unary, p, std::move(op));
            e->args.push_back(unary());
            return e;
        }
        return power();
    }

    ExprPtr power() {
        ExprPtr l = primary();
        while (acceptPunct("^")) l = binary("^", std::move(l), powerOperand());
        return l;
    }

    ExprPtr powerOperand() {
        if (atPunct("-") || atPunct("+")) {
            const ast::Pos p = pos();
            std::string op = take().text;
            auto e = make(ExprKind::Unary, p, std::move(op));
            e->args.push_back(powerOperand());
            return e;
        }
        return primary();
    }

    void argList(Expr& e) {
        // after '(' : arguments up to ')'; "()" marks a whole array
        e.parens = true;
        if (acceptPunct(")")) return;
        do {
            if (atPunct("#")) {  // file channel: EOF(#1), INPUT$(n,#1)
                const ast::Pos p = pos();
                take();
                auto c = make(ExprKind::Unary, p, "#");
                c->args.push_back(expr());
                e.args.push_back(std::move(c));
            } else {
                e.args.push_back(expr());
            }
        } while (acceptPunct(","));
        expectPunct(")");
    }

    ExprPtr primary() {
        const ast::Pos p = pos();
        const Token& t = peek();
        if (t.kind == Tok::Number) return make(ExprKind::Number, p, take().text);
        if (t.kind == Tok::String) return make(ExprKind::String, p, take().text);
        if (acceptPunct("(")) {
            ExprPtr e = expr();
            expectPunct(")");
            return e;
        }
        if (acceptWord("NOT")) {  // lowest precedence: applies to the rest of the expression
            auto e = make(ExprKind::Unary, p, "NOT");
            e->args.push_back(expr());
            return e;
        }
        if (atMemAccess()) return memAccess();
        if (acceptPunct("*")) {  // *x = ARRPTR(x); in SWAP *p,a() the array p points to (sema)
            auto e = make(ExprKind::Call, p, "ARRPTR");
            e->parens = true;
            e->args.push_back(lvalue());
            return e;
        }
        if (atWord("V") && atPunct(":", 1)) {  // V:x = VARPTR(x)
            take();
            take();
            auto e = make(ExprKind::Call, p, "VARPTR");
            e->parens = true;
            e->args.push_back(primary());
            return e;
        }
        if (acceptPunct("@") || acceptWord("FN")) {
            auto e = make(ExprKind::Call, p, expectIdent("function name"));
            e->user = true;
            if (acceptPunct("(")) argList(*e);
            return e;
        }
        if (t.kind == Tok::Ident) {
            std::string name = take().text;
            const Builtin* b = findBuiltin(name);
            // a builtin that needs arguments, written without '(': a variable of that name (max)
            if (b != nullptr && !(b->minArgs > 0 && !atPunct("("))) {
                auto e = make(ExprKind::Call, p, std::move(name));
                if (acceptPunct("(")) argList(*e);
                return e;
            }
            auto e = make(ExprKind::Var, p, std::move(name));
            if (acceptPunct("(")) {
                if (atPunct(")")) {
                    take();
                    e->wholeArray = true;
                    e->parens = true;
                } else {
                    argList(*e);
                }
            }
            return e;
        }
        if (t.kind == Tok::End) fail("expression expected");
        fail("unexpected '" + t.text + "'");
    }

    // A variable or array element as the target of an assignment, READ, FOR, LOCAL ...
    ExprPtr lvalue() {
        const ast::Pos p = pos();
        auto e = make(ExprKind::Var, p, expectIdent("variable"));
        if (acceptPunct("(")) {
            if (atPunct(")")) {
                take();
                e->wholeArray = true;
                e->parens = true;
            } else {
                argList(*e);
            }
        }
        return e;
    }

    // ---- statements -------------------------------------------------------------------------
    Stmt stmt(StmtKind kind, ast::Pos p) {
        Stmt s;
        s.kind = kind;
        s.pos = p;
        return s;
    }

    std::vector<ast::Param> params() {
        std::vector<ast::Param> ps;
        if (!acceptPunct("(")) return ps;
        if (acceptPunct(")")) return ps;
        bool byRef = false;  // VAR applies to all following parameters (probe10 T1, T2)
        do {
            ast::Param prm;
            byRef = acceptWord("VAR") || byRef;
            prm.byRef = byRef;
            prm.name = expectIdent("parameter");
            if (acceptPunct("(")) {
                expectPunct(")");
                prm.array = true;
            }
            ps.push_back(std::move(prm));
        } while (acceptPunct(","));
        expectPunct(")");
        return ps;
    }

    void callArgs(Stmt& s) {
        if (!acceptPunct("(")) return;
        if (acceptPunct(")")) return;
        do s.args.push_back(expr());
        while (acceptPunct(","));
        expectPunct(")");
    }

    // INPUT/LINE INPUT "text"; or "text", : the prompt becomes the first argument, variant 1 (";",
    // "? " follows) or 2 (",": no question mark)
    void inputPrompt(Stmt& s) {
        if (!at(Tok::String) || !(atPunct(";", 1) || atPunct(",", 1))) return;
        const ast::Pos p = pos();
        auto e = make(ExprKind::String, p, take().text);
        s.variant = acceptPunct(";") ? 1 : (take(), 2);
        s.args.push_back(std::move(e));
    }

    // Arguments of a generic command; "#n" (file channel) is an argument of its own kind.
    void exprList(Stmt& s) {
        if (at(Tok::End)) return;
        do {
            if (atPunct("#")) {
                const ast::Pos p = pos();
                take();
                auto e = make(ExprKind::Unary, p, "#");
                e->args.push_back(expr());
                s.args.push_back(std::move(e));
            } else {
                s.args.push_back(expr());
                if (acceptWord("AS")) s.args.push_back(expr());  // FIELD #n,len AS a$ / NAME a$ AS b$
            }
        } while (acceptPunct(","));
    }

    bool topLevelComma() const {
        int depth = 0;
        for (std::size_t k = i_; k < toks_.size(); ++k) {
            const Token& t = toks_[k];
            if (t.kind != Tok::Punct) continue;
            if (t.text == "(") ++depth;
            else if (t.text == ")") --depth;
            else if (t.text == "," && depth == 0) return true;
        }
        return false;
    }

    // BYTE{x} CARD{x} INT{x} WORD{x} LONG{x} {x} FLOAT{x} SINGLE{x} DOUBLE{x} CHAR{x}
    bool atMemAccess() const {
        if (atPunct("{")) return true;
        static const std::set<std::string_view> names = {"BYTE", "CARD", "INT", "WORD", "LONG", "FLOAT", "SINGLE", "DOUBLE", "CHAR"};
        return peek().kind == Tok::Ident && names.count(peek().text) != 0 && atPunct("{", 1);
    }

    // -> call "BYTE{}" (WORD -> INT, {} -> LONG) with the address
    ExprPtr memAccess() {
        const ast::Pos p = pos();
        std::string n = atPunct("{") ? "LONG" : take().text;
        if (n == "WORD") n = "INT";
        expectPunct("{");
        auto e = make(ExprKind::Call, p, n + "{}");
        e->parens = true;
        e->args.push_back(expr());
        expectPunct("}");
        return e;
    }

    Stmt statement() {
        // a folded procedure as LIST writes it: "> PROCEDURE name"
        if (atPunct(">") && (atWord("PROCEDURE", 1) || atWord("FUNCTION", 1))) take();
        const ast::Pos p = pos();
        // a procedure called with its bare name: one starting with digits, or one named like a
        // keyword and written in lower case (LIST writes keywords in upper case, user names in lower)
        if (procs_ != nullptr) {
            std::string name;
            if (atDigitName(toks_, i_)) name = toks_[i_].text + toks_[i_ + 1].text;
            else if (at(Tok::Ident) && peek().lower) name = peek().text;
            const std::size_t after = name.size() > peek().text.size() ? 2 : 1;  // tokens of the name
            bool assignment = false;  // an array of the same name: menü(1)=39
            if (atPunct("(", after)) {
                std::size_t k = i_ + after, depth = 0;
                for (; k < toks_.size() && toks_[k].kind != Tok::End; ++k) {
                    if (toks_[k].kind == Tok::Punct && toks_[k].text == "(") ++depth;
                    if (toks_[k].kind == Tok::Punct && toks_[k].text == ")" && --depth == 0) break;
                }
                assignment = k + 1 < toks_.size() && toks_[k + 1].kind == Tok::Punct && toks_[k + 1].text == "=";
            }
            if (!name.empty() && !assignment && procs_->count(name) != 0 && (peek(after).kind == Tok::End || atPunct("(", after))) {
                take();
                if (name.size() > toks_[i_ - 1].text.size()) take();
                Stmt s = stmt(StmtKind::Call, p);
                s.name = name;
                callArgs(s);
                return s;
            }
        }
        if (atMemAccess()) {  // BYTE{x}=y: command "BYTE{}" address, value
            ExprPtr target = memAccess();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = target->text;
            s.args.push_back(std::move(target->args[0]));
            expectPunct("=");
            s.args.push_back(expr());
            return s;
        }
        // label: "name:" alone on the line (names may start with a digit)
        if ((at(Tok::Ident) || at(Tok::Number)) && atPunct(":", 1) && peek(2).kind == Tok::End) {
            Stmt s = stmt(StmtKind::Label, p);
            s.name = take().text;
            take();
            return s;
        }
        if (acceptPunct("@")) {
            Stmt s = stmt(StmtKind::Call, p);
            s.name = procName("procedure name");
            callArgs(s);
            return s;
        }
        if (acceptPunct("~")) {
            Stmt s = stmt(StmtKind::Void, p);
            s.args.push_back(expr());
            return s;
        }
        if (!at(Tok::Ident)) fail("statement expected");
        const std::string word = peek().text;

        if (word == "LET") {
            take();
            return assignment(p);
        }
        if (word == "PRINT" || word == "LPRINT") return print(p);
        if (word == "IF") {
            take();
            Stmt s = stmt(StmtKind::If, p);
            s.args.push_back(expr());
            acceptWord("THEN");
            return s;
        }
        if (word == "ELSE") {
            take();
            if (acceptWord("IF")) {
                Stmt s = stmt(StmtKind::ElseIf, p);
                s.args.push_back(expr());
                acceptWord("THEN");
                return s;
            }
            return stmt(StmtKind::Else, p);
        }
        if (word == "ENDIF") return take(), stmt(StmtKind::EndIf, p);
        if (word == "FOR") {
            take();
            Stmt s = stmt(StmtKind::For, p);
            s.args.push_back(lvalue());
            expectPunct("=");
            s.args.push_back(expr());
            if (acceptWord("DOWNTO")) {
                s.variant = 1;
                s.args.push_back(expr());
            } else {
                expectWord("TO");
                s.args.push_back(expr());
                if (acceptWord("STEP")) s.args.push_back(expr());
            }
            return s;
        }
        if (word == "NEXT" || word == "ENDFOR") {
            take();
            Stmt s = stmt(StmtKind::Next, p);
            if (at(Tok::Ident)) s.args.push_back(lvalue());
            return s;
        }
        if (word == "WHILE") {
            take();
            Stmt s = stmt(StmtKind::While, p);
            s.args.push_back(expr());
            return s;
        }
        if (word == "WEND" || word == "ENDWHILE") return take(), stmt(StmtKind::Wend, p);
        if (word == "REPEAT") return take(), stmt(StmtKind::Repeat, p);
        if (word == "UNTIL" || word == "ENDREPEAT") {
            take();
            Stmt s = stmt(StmtKind::Until, p);
            s.args.push_back(expr());
            return s;
        }
        if (word == "DO" || word == "LOOP" || word == "ENDDO") {
            take();
            Stmt s = stmt(word == "DO" ? StmtKind::Do : StmtKind::Loop, p);
            if (acceptWord("WHILE")) s.variant = 1;
            else if (acceptWord("UNTIL")) s.variant = 2;
            if (s.variant != 0) s.args.push_back(expr());
            return s;
        }
        if (word == "EXIT") {
            take();
            expectWord("IF");
            Stmt s = stmt(StmtKind::ExitIf, p);
            s.args.push_back(expr());
            return s;
        }
        if (word == "SELECT") {
            take();
            Stmt s = stmt(StmtKind::Select, p);
            s.args.push_back(expr());
            return s;
        }
        if (word == "CASE") {
            take();
            Stmt s = stmt(StmtKind::Case, p);
            do {
                ast::CaseItem c;
                if (acceptWord("TO")) {
                    c.range = true;
                    c.hi = expr();
                } else {
                    c.lo = expr();
                    if (acceptWord("TO")) {
                        c.range = true;
                        if (!at(Tok::End) && !atPunct(",")) c.hi = expr();
                    }
                }
                s.cases.push_back(std::move(c));
            } while (acceptPunct(","));
            return s;
        }
        if (word == "DEFAULT" || word == "OTHERWISE") return take(), stmt(StmtKind::Default, p);
        if (word == "CONT") return take(), stmt(StmtKind::Cont, p);
        if (word == "ENDSELECT") return take(), stmt(StmtKind::EndSelect, p);
        if (word == "PROCEDURE" || (word == "SUB" && !topLevelCommaAfter(1))) {
            take();
            Stmt s = stmt(StmtKind::Procedure, p);
            s.name = procName("procedure name");
            s.params = params();
            return s;
        }
        if (word == "FUNCTION") {
            take();
            Stmt s = stmt(StmtKind::Function, p);
            s.name = expectIdent("function name");
            s.params = params();
            return s;
        }
        if (word == "ENDFUNC") return take(), stmt(StmtKind::EndFunc, p);
        if (word == "RETURN" || word == "ENDPROC" || word == "ENDSUB") {
            take();
            Stmt s = stmt(StmtKind::Return, p);
            if (word == "RETURN" && !at(Tok::End)) s.args.push_back(expr());
            return s;
        }
        if (word == "LOCAL") {
            take();
            Stmt s = stmt(StmtKind::Local, p);
            do s.args.push_back(lvalue());
            while (acceptPunct(","));
            return s;
        }
        if (word == "GOSUB") {
            take();
            Stmt s = stmt(StmtKind::Call, p);
            s.name = procName("procedure name");
            callArgs(s);
            return s;
        }
        if (word == "GOTO") {
            take();
            Stmt s = stmt(StmtKind::Goto, p);
            s.name = labelName();
            return s;
        }
        if (word == "DIM") {
            take();
            Stmt s = stmt(StmtKind::Dim, p);
            do {
                ExprPtr a = lvalue();
                if (a->args.empty()) fail("array dimensions expected");
                s.args.push_back(std::move(a));
            } while (acceptPunct(","));
            return s;
        }
        if (word == "DATA") {
            take();
            Stmt s = stmt(StmtKind::Data, p);
            s.data = dataItems();
            return s;
        }
        if (word == "READ") {
            take();
            Stmt s = stmt(StmtKind::Read, p);
            do s.args.push_back(lvalue());
            while (acceptPunct(","));
            return s;
        }
        if (word == "RESTORE") {
            take();
            Stmt s = stmt(StmtKind::Restore, p);
            if (!at(Tok::End)) s.name = labelName();
            return s;
        }
        if (word == "DEFFN") {
            take();
            Stmt s = stmt(StmtKind::DefFn, p);
            s.name = expectIdent("function name");
            s.params = params();
            expectPunct("=");
            s.args.push_back(expr());
            return s;
        }
        if (word == "ON" || word == "ONERROR") {
            take();
            if (word == "ON" && (atWord("MENU") || atWord("MESSAGE"))) {  // chapter 9 (D26)
                Stmt s = stmt(StmtKind::Command, p);
                const bool message = take().text == "MESSAGE";
                std::string kind = message ? "MESSAGE" : "";
                if (!message && (atWord("KEY") || atWord("BUTTON"))) kind = take().text;
                if (acceptWord("GOSUB")) {
                    s.name = "ON MENU" + std::string(kind.empty() ? "" : " " + kind) + " GOSUB";
                    s.args.push_back(makeName(procName("procedure name")));
                } else {
                    if (!kind.empty()) fail("GOSUB expected");
                    s.name = "ON MENU";  // ON MENU [t]: look for events once
                    if (!at(Tok::End)) s.args.push_back(expr());
                }
                return s;
            }
            if (word == "ON" && !acceptWord("ERROR")) {
                Stmt s = stmt(StmtKind::Command, p);  // ON x GOSUB a,b / ON BREAK ...
                s.name = "ON";
                if (acceptWord("BREAK")) {
                    s.name = "ON BREAK";
                    if (acceptWord("GOSUB")) s.args.push_back(makeName(procName("procedure name")));
                    else if (acceptWord("CONT")) s.variant = 1;
                    return s;
                }
                s.args.push_back(expr());
                expectWord("GOSUB");
                do s.args.push_back(makeName(procName("procedure name")));
                while (acceptPunct(","));
                return s;
            }
            Stmt s = stmt(StmtKind::OnError, p);
            if (acceptWord("GOSUB")) s.name = procName("procedure name");
            return s;
        }
        if (word == "RESUME") {
            take();
            Stmt s = stmt(StmtKind::Resume, p);
            if (acceptWord("NEXT")) s.variant = 1;
            else if (!at(Tok::End)) {
                s.variant = 2;
                s.name = labelName();
            }
            return s;
        }
        if (word == "VOID") {
            take();
            Stmt s = stmt(StmtKind::Void, p);
            s.args.push_back(expr());
            return s;
        }
        if (word == "MID$" && atPunct("(", 1)) return assignment(p);  // MID$(a$,x,y)=b$
        if (word == "FORM" && atWord("INPUT", 1)) {  // FORM INPUT n AS a$ / FORM INPUT n,a$
            take();
            take();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = "FORM INPUT";
            s.args.push_back(expr());
            if (acceptWord("AS")) s.variant = 1;
            else expectPunct(",");
            s.args.push_back(lvalue());
            return s;
        }
        if (word == "MENU" && (atWord("KILL", 1) || atWord("KEY", 1))) {
            take();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = "MENU " + take().text;
            exprList(s);
            return s;
        }
        if (word == "LINE" && atWord("INPUT", 1)) {
            take();
            take();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = "LINE INPUT";
            inputPrompt(s);
            exprList(s);
            return s;
        }
        if ((word == "FILES" || word == "DIR") && !atPunct("=", 1)) {  // FILES [p$] [TO name$]
            take();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = word;
            if (!at(Tok::End) && !atWord("TO")) s.args.push_back(expr());
            if (acceptWord("TO")) {
                s.variant = 1;
                s.args.push_back(expr());
            }
            return s;
        }
        if ((word == "POLYLINE" || word == "POLYFILL") && !atPunct("=", 1)) {  // n,x(),y() [OFFSET dx,dy]
            take();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = word;
            s.args.push_back(expr());
            expectPunct(",");
            s.args.push_back(expr());
            expectPunct(",");
            s.args.push_back(expr());
            if (acceptWord("OFFSET")) {
                s.args.push_back(expr());
                expectPunct(",");
                s.args.push_back(expr());
            }
            return s;
        }
        if (kGfxCommands.count(word) != 0 && !(word == "LINE" && atWord("INPUT", 1)) && !atPunct("=", 1)) {
            take();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = word;
            if (word == "DRAW") {  // DRAW x,y / DRAW TO x,y / DRAW x1,y1 TO x2,y2 [TO ...]
                if (at(Tok::String)) fail("DRAW with a turtle string is not supported yet");
                if (acceptWord("TO")) s.variant = 1;
                do {
                    s.args.push_back(expr());
                    expectPunct(",");
                    s.args.push_back(expr());
                } while (acceptWord("TO"));
                return s;
            }
            gfxArgs(s);
            return s;
        }
        if (word == "MAT" && peek(1).kind == Tok::Ident) {  // chapter 13
            static const std::set<std::string_view> words = {"BASE", "CLR",  "SET",  "ONE",   "READ", "PRINT", "INPUT",
                                                             "CPY",  "COPY", "XCPY", "TRANS", "ADD",  "SUB",   "MUL",
                                                             "NORM", "DET",  "QDET", "RANG",  "INV"};
            if (words.count(peek(1).text) != 0) return matStatement(p);
        }
        if (kCommands.count(word) != 0 && !atPunct("=", 1)) {
            take();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = word;
            if (word == "OPTION") {
                expectWord("BASE");
                s.name = "OPTION BASE";
            }
            if (word == "INPUT") inputPrompt(s);
            exprList(s);
            if (word == "WRITE" && acceptPunct(";")) s.variant = 1;  // WRITE ...; : no line feed
            return s;
        }
        // assignment or procedure call
        const std::size_t save = i_;
        take();
        if (acceptPunct("(")) {
            int depth = 1;
            while (depth > 0 && !at(Tok::End)) {
                if (atPunct("(")) ++depth;
                else if (atPunct(")")) --depth;
                take();
            }
        }
        const bool isAssign = atPunct("=");
        i_ = save;
        if (isAssign) return assignment(p);
        Stmt s = stmt(StmtKind::Call, p);
        s.name = take().text;
        callArgs(s);
        return s;
    }

    // arguments that may be left out: "DEFFILL ,2,4", "COLOR 1,,2"; "#n" for a window number
    void gfxArgs(Stmt& s) {
        if (at(Tok::End)) return;
        for (;;) {
            if (atPunct(",") || at(Tok::End)) {
                s.args.push_back(make(ExprKind::Missing, pos(), ""));
            } else {
                acceptPunct("#");
                s.args.push_back(expr());
            }
            if (!acceptPunct(",")) break;
            if (at(Tok::End)) s.args.push_back(make(ExprKind::Missing, pos(), ""));
        }
    }

    // MAT w ...: args = target [, operands after '='] [, further arguments]; variant 0 without '=',
    // else 1 + 2 * (number of operands after '='). PRINT/INPUT: optional channel, then a list.
    Stmt matStatement(ast::Pos p) {
        take();
        std::string w = take().text;
        if (w == "COPY") w = "CPY";
        Stmt s = stmt(StmtKind::Command, p);
        s.name = "MAT " + w;
        if (w == "PRINT" || w == "INPUT") {
            if (acceptPunct("#")) {
                s.channel = expr();
                expectPunct(",");
            }
            exprList(s);
            return s;
        }
        if (w == "BASE") {
            s.args.push_back(expr());
            return s;
        }
        s.args.push_back(lvalue());
        if (acceptPunct("=")) {
            int n = 1;
            if (w == "SET") {
                s.args.push_back(expr());
            } else {
                s.args.push_back(lvalue());
                const char* op = w == "ADD" ? "+" : w == "SUB" ? "-" : w == "MUL" ? "*" : nullptr;
                while (op != nullptr && acceptPunct(op)) {
                    s.args.push_back(lvalue());
                    ++n;
                }
            }
            s.variant = 1 + 2 * n;
        }
        while (acceptPunct(",")) s.args.push_back(expr());
        return s;
    }

    bool topLevelCommaAfter(std::size_t skip) const {
        Parser copy = *this;
        copy.i_ += skip;
        return copy.topLevelComma();
    }

    ExprPtr makeName(std::string name) { return make(ExprKind::Var, pos(), std::move(name)); }

    std::string labelName() {
        if (at(Tok::Ident) || at(Tok::Number)) return take().text;
        fail("label expected");
    }

    Stmt assignment(ast::Pos p) {
        Stmt s = stmt(StmtKind::Assign, p);
        if (atWord("MID$")) {  // MID$(a$,x[,y])=b$
            auto target = make(ExprKind::Call, pos(), take().text);
            expectPunct("(");
            argList(*target);
            s.args.push_back(std::move(target));
        } else {
            s.args.push_back(lvalue());
        }
        expectPunct("=");
        s.args.push_back(expr());
        return s;
    }

    Stmt print(ast::Pos p) {
        Stmt s = stmt(StmtKind::Print, p);
        s.name = take().text;
        if (acceptPunct("#")) {
            s.channel = expr();
            if (!acceptPunct(",") && !at(Tok::End)) fail("',' expected");
        }
        if (acceptWord("USING")) {
            s.usingFmt = expr();
            if (!acceptPunct(",") && !acceptPunct(";") && !at(Tok::End)) fail("',' expected");
        }
        while (!at(Tok::End)) {
            ast::PrintItem item;
            if (!(atPunct(";") || atPunct(",") || atPunct("'"))) item.expr = expr();
            if (atPunct(";") || atPunct(",") || atPunct("'")) item.sep = take().text[0];
            else if (!at(Tok::End)) fail("';' or ',' expected");
            s.print.push_back(std::move(item));
        }
        return s;
    }

    // DATA items from the raw line: comma separated, strings may be quoted, no comments.
    std::vector<ast::DataItem> dataItems() const {
        const std::string& text = line_.text;
        std::size_t k = static_cast<std::size_t>(toks_[0].col - 1) + 4;  // after "DATA"
        std::vector<ast::DataItem> items;
        std::size_t rest = k;
        while (rest < text.size() && (text[rest] == ' ' || text[rest] == '\t')) ++rest;
        if (rest >= text.size()) return items;
        for (;;) {
            while (k < text.size() && (text[k] == ' ' || text[k] == '\t')) ++k;
            ast::DataItem item;
            if (k < text.size() && text[k] == '"') {
                item.quoted = true;
                ++k;
                while (k < text.size() && text[k] != '"') item.text += text[k++];
                if (k < text.size()) ++k;
                while (k < text.size() && text[k] != ',') ++k;
            } else {
                while (k < text.size() && text[k] != ',') item.text += text[k++];
                while (!item.text.empty() && (item.text.back() == ' ' || item.text.back() == '\t'))
                    item.text.pop_back();
            }
            items.push_back(std::move(item));
            if (k >= text.size()) break;
            ++k;  // ','
        }
        return items;
    }
};

}  // namespace

ast::Program parseProgram(std::string_view source, Diagnostics& diags) {
    ast::Program program;
    const std::vector<SourceLine> lines = lexSource(source, diags);
    // the names of all procedures first: a bare call may come before the PROCEDURE line
    std::set<std::string> procs;
    for (const SourceLine& line : lines) {
        const std::vector<Token>& t = line.tokens;
        std::size_t k = 0;
        if (k < t.size() && t[k].kind == Tok::Punct && t[k].text == ">") ++k;
        if (k + 1 < t.size() && t[k].kind == Tok::Ident && t[k].text == "PROCEDURE") {
            if (Parser::atDigitName(t, k + 1)) procs.insert(t[k + 1].text + t[k + 2].text);
            else if (t[k + 1].kind == Tok::Ident) procs.insert(t[k + 1].text);
        }
    }
    for (const SourceLine& line : lines) {
        if (line.tokens.empty()) continue;
        Parser parser(line, diags, &procs);
        Stmt s;
        if (parser.parse(s)) program.stmts.push_back(std::move(s));
    }
    return program;
}

}  // namespace basic
