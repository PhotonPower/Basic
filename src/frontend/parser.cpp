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
const std::set<std::string, std::less<>> kCommands = {
    "INC",    "DEC",       "ADD",    "SUB",    "MUL",    "DIV",    "SWAP",   "CLR",     "ERASE",
    "DEFNUM", "MODE",      "RANDOMIZE", "ERROR", "QUIT", "SYSTEM", "STOP",   "END",     "EDIT",
    "CLEAR",  "LSET",      "RSET",   "DEFINT", "DEFFLT", "DEFSTR", "DEFBIT", "DEFBYT",  "DEFWRD",
    "DEFSNG", "DEFDBL",    "PAUSE",  "DELAY",  "CLS",    "NEW",    "ARRAYFILL", "INSERT", "DELETE",
    "OPTION", "TRON",      "TROFF",  "WRITE",  "LOCATE", "HTAB",   "VTAB",   "INPUT",   "POKE",
    "DPOKE",  "LPOKE",     "BMOVE",  "OPEN",   "CLOSE",  "KILL",   "NAME",   "RENAME",  "SOUND",
};

class Parser {
public:
    Parser(const SourceLine& line, Diagnostics& diags) : line_(line), toks_(line.tokens), diags_(diags) {}

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
        if (acceptPunct("@") || acceptWord("FN")) {
            auto e = make(ExprKind::Call, p, expectIdent("function name"));
            e->user = true;
            if (acceptPunct("(")) argList(*e);
            return e;
        }
        if (t.kind == Tok::Ident) {
            std::string name = take().text;
            if (findBuiltin(name) != nullptr) {
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

    Stmt statement() {
        const ast::Pos p = pos();
        // label: "name:" alone on the line (names may start with a digit)
        if ((at(Tok::Ident) || at(Tok::Number)) && atPunct(":", 1) && peek(2).kind == Tok::End) {
            Stmt s = stmt(StmtKind::Label, p);
            s.name = take().text;
            take();
            return s;
        }
        if (acceptPunct("@")) {
            Stmt s = stmt(StmtKind::Call, p);
            s.name = expectIdent("procedure name");
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
            s.name = expectIdent("procedure name");
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
            s.name = expectIdent("procedure name");
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
            if (word == "ON" && !acceptWord("ERROR")) {
                Stmt s = stmt(StmtKind::Command, p);  // ON x GOSUB a,b / ON BREAK ...
                s.name = "ON";
                if (acceptWord("BREAK")) {
                    s.name = "ON BREAK";
                    if (acceptWord("GOSUB")) s.args.push_back(makeName(expectIdent("procedure name")));
                    else if (acceptWord("CONT")) s.variant = 1;
                    return s;
                }
                s.args.push_back(expr());
                expectWord("GOSUB");
                do s.args.push_back(makeName(expectIdent("procedure name")));
                while (acceptPunct(","));
                return s;
            }
            Stmt s = stmt(StmtKind::OnError, p);
            if (acceptWord("GOSUB")) s.name = expectIdent("procedure name");
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
        if (word == "LINE" && atWord("INPUT", 1)) {
            take();
            take();
            Stmt s = stmt(StmtKind::Command, p);
            s.name = "LINE INPUT";
            inputPrompt(s);
            exprList(s);
            return s;
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
    for (const SourceLine& line : lines) {
        if (line.tokens.empty()) continue;
        Parser parser(line, diags);
        Stmt s;
        if (parser.parse(s)) program.stmts.push_back(std::move(s));
    }
    return program;
}

}  // namespace basic
