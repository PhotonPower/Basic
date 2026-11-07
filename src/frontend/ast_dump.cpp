#include "ast.hpp"

namespace basic::ast {

std::string dump(const Expr& e) {
    std::string r;
    switch (e.kind) {
        case ExprKind::Number: return e.text;
        case ExprKind::String: return "\"" + e.text + "\"";
        case ExprKind::Var:
            r = e.text;
            if (e.wholeArray) return r + "()";
            break;
        case ExprKind::Call: r = (e.user ? "@" : "") + e.text; break;
        case ExprKind::Unary: return "(" + e.text + " " + dump(*e.args[0]) + ")";
        case ExprKind::Binary: return "(" + dump(*e.args[0]) + " " + e.text + " " + dump(*e.args[1]) + ")";
    }
    if (e.parens || !e.args.empty()) {
        r += "(";
        for (std::size_t i = 0; i < e.args.size(); ++i) r += (i ? "," : "") + dump(*e.args[i]);
        r += ")";
    }
    return r;
}

namespace {

const char* kindName(StmtKind k) {
    switch (k) {
        case StmtKind::Assign: return "assign";
        case StmtKind::Print: return "print";
        case StmtKind::If: return "if";
        case StmtKind::ElseIf: return "elseif";
        case StmtKind::Else: return "else";
        case StmtKind::EndIf: return "endif";
        case StmtKind::For: return "for";
        case StmtKind::Next: return "next";
        case StmtKind::While: return "while";
        case StmtKind::Wend: return "wend";
        case StmtKind::Repeat: return "repeat";
        case StmtKind::Until: return "until";
        case StmtKind::Do: return "do";
        case StmtKind::Loop: return "loop";
        case StmtKind::ExitIf: return "exitif";
        case StmtKind::Select: return "select";
        case StmtKind::Case: return "case";
        case StmtKind::Default: return "default";
        case StmtKind::Cont: return "cont";
        case StmtKind::EndSelect: return "endselect";
        case StmtKind::Procedure: return "procedure";
        case StmtKind::Function: return "function";
        case StmtKind::EndFunc: return "endfunc";
        case StmtKind::Return: return "return";
        case StmtKind::Local: return "local";
        case StmtKind::Call: return "call";
        case StmtKind::Goto: return "goto";
        case StmtKind::Label: return "label";
        case StmtKind::Dim: return "dim";
        case StmtKind::Data: return "data";
        case StmtKind::Read: return "read";
        case StmtKind::Restore: return "restore";
        case StmtKind::DefFn: return "deffn";
        case StmtKind::OnError: return "onerror";
        case StmtKind::Resume: return "resume";
        case StmtKind::Void: return "void";
        case StmtKind::Command: return "command";
    }
    return "?";
}

}  // namespace

std::string dump(const Stmt& s) {
    std::string r = std::to_string(s.pos.line) + ": " + kindName(s.kind);
    if (s.variant != 0) r += "." + std::to_string(s.variant);
    if (!s.name.empty()) r += " " + s.name;
    if (!s.params.empty()) {
        r += " (";
        for (std::size_t i = 0; i < s.params.size(); ++i) {
            const Param& p = s.params[i];
            r += (i ? "," : "") + std::string(p.byRef ? "VAR " : "") + p.name + (p.array ? "()" : "");
        }
        r += ")";
    }
    if (s.channel) r += " #" + dump(*s.channel);
    if (s.usingFmt) r += " using " + dump(*s.usingFmt);
    for (const auto& a : s.args) r += " " + dump(*a);
    for (const PrintItem& it : s.print) {
        r += " [" + (it.expr ? dump(*it.expr) : std::string());
        if (it.sep != 0) r += std::string(" ") + it.sep;
        r += "]";
    }
    for (const CaseItem& c : s.cases) {
        r += " {";
        if (c.lo) r += dump(*c.lo);
        if (c.range) r += " TO ";
        if (c.hi) r += dump(*c.hi);
        r += "}";
    }
    for (const DataItem& d : s.data) r += d.quoted ? " \"" + d.text + "\"" : " <" + d.text + ">";
    return r;
}

std::string dump(const Program& p) {
    std::string r;
    for (const Stmt& s : p.stmts) r += dump(s) + "\n";
    return r;
}

}  // namespace basic::ast
