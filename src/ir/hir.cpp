#include "hir.hpp"

#include "rt_ref.h"

namespace basic::hir {

const char* typeName(Type t) {
    switch (t) {
        case Type::Bool: return "bool";
        case Type::Byte: return "byte";
        case Type::Word: return "word";
        case Type::Int: return "int";
        case Type::Float: return "float";
        case Type::Str: return "str";
    }
    return "?";
}

char typePostfix(Type t) {
    switch (t) {
        case Type::Bool: return '!';
        case Type::Byte: return '|';
        case Type::Word: return '&';
        case Type::Int: return '%';
        case Type::Float: return '#';
        case Type::Str: return '$';
    }
    return '?';
}

std::string Var::display() const {
    return name + typePostfix(type) + (array ? "()" : "");
}

namespace {

const char* opName(Op op) {
    switch (op) {
        case Op::Conv: return "conv";
        case Op::ToInt: return "toint";
        case Op::Neg: return "neg";
        case Op::Add: return "add";
        case Op::Sub: return "sub";
        case Op::Mul: return "mul";
        case Op::Div: return "div";
        case Op::Pow: return "pow";
        case Op::IDiv: return "idiv";
        case Op::IMod: return "imod";
        case Op::Not: return "not";
        case Op::And: return "and";
        case Op::Or: return "or";
        case Op::Xor: return "xor";
        case Op::Imp: return "imp";
        case Op::Eqv: return "eqv";
        case Op::Eq: return "eq";
        case Op::Ne: return "ne";
        case Op::Lt: return "lt";
        case Op::Gt: return "gt";
        case Op::Le: return "le";
        case Op::Ge: return "ge";
        case Op::ApproxEq: return "approx";
        case Op::Concat: return "concat";
        default: return "?";
    }
}

class Dumper {
public:
    explicit Dumper(const Program& p) : p_(p) {}

    std::string expr(const Expr& e) const {
        switch (e.op) {
            case Op::ConstInt: return std::to_string(e.ival);
            case Op::ConstFloat: {
                char buf[RT_FMT_FLT_MAX];
                rt_fmt_flt(buf, e.fval, 13);
                return std::string(buf) + "#";
            }
            case Op::ConstStr: return "\"" + e.sval + "\"";
            case Op::Load: return p_.vars[static_cast<std::size_t>(e.var)].display();
            case Op::LoadElem: {
                std::string r = p_.vars[static_cast<std::size_t>(e.var)].name + typePostfix(e.type) + "(";
                for (std::size_t i = 0; i < e.args.size(); ++i) r += (i ? "," : "") + expr(*e.args[i]);
                return r + ")";
            }
            case Op::Builtin:
            case Op::CallFunc: {
                std::string r = e.op == Op::Builtin ? e.sval : "@" + p_.routines[static_cast<std::size_t>(e.func)].name;
                r += "(";
                for (std::size_t i = 0; i < e.args.size(); ++i) r += (i ? "," : "") + expr(*e.args[i]);
                return r + "):" + typeName(e.type);
            }
            case Op::Conv: return "(conv." + std::string(typeName(e.type)) + " " + expr(*e.args[0]) + ")";
            default: {
                std::string r = "(" + std::string(opName(e.op));
                for (const auto& a : e.args) r += " " + expr(*a);
                return r + ")";
            }
        }
    }

    void block(const Block& b, int indent, std::string& out) const {
        for (const auto& s : b) stmt(*s, indent, out);
    }

    void stmt(const Stmt& s, int indent, std::string& out) const {
        const std::string pad(static_cast<std::size_t>(indent) * 2, ' ');
        auto line = [&](const std::string& text) { out += pad + text + "\n"; };
        switch (s.kind) {
            case SKind::Assign: line(expr(*s.target) + " = " + expr(*s.value)); break;
            case SKind::Print: {
                std::string r = "print";
                if (s.usingFmt) r += " using " + expr(*s.usingFmt);
                for (const PrintItem& it : s.items) {
                    r += " [";
                    if (it.kind == PrintItem::Tab) r += "tab ";
                    if (it.kind == PrintItem::Spc) r += "spc ";
                    if (it.expr) r += expr(*it.expr);
                    if (it.sep) r += std::string(" ") + it.sep;
                    r += "]";
                }
                line(r);
                break;
            }
            case SKind::If:
                for (std::size_t i = 0; i < s.branches.size(); ++i) {
                    line((i == 0 ? "if " : "elseif ") + expr(*s.branches[i].cond));
                    block(s.branches[i].body, indent + 1, out);
                }
                if (!s.elseBody.empty()) {
                    line("else");
                    block(s.elseBody, indent + 1, out);
                }
                line("endif");
                break;
            case SKind::For:
                line("for " + expr(*s.target) + " = " + expr(*s.start) + " to " + expr(*s.end) + " step " + expr(*s.step));
                block(s.body, indent + 1, out);
                line("next");
                break;
            case SKind::While:
                line("while " + expr(*s.cond));
                block(s.body, indent + 1, out);
                line("wend");
                break;
            case SKind::Repeat:
                line("repeat");
                block(s.body, indent + 1, out);
                line("until " + (s.cond ? expr(*s.cond) : std::string("?")));
                break;
            case SKind::Do:
                line("do" + std::string(s.pre == 1 ? " while " : s.pre == 2 ? " until " : "") + (s.cond ? expr(*s.cond) : ""));
                block(s.body, indent + 1, out);
                line("loop" + std::string(s.post == 1 ? " while " : s.post == 2 ? " until " : "") + (s.cond2 ? expr(*s.cond2) : ""));
                break;
            case SKind::ExitIf: line("exitif " + expr(*s.cond)); break;
            case SKind::Select:
                line("select " + expr(*s.value));
                for (const Case& c : s.cases) {
                    std::string r = "case";
                    for (const CaseItem& it : c.items)
                        r += " {" + (it.lo ? expr(*it.lo) : "") + (it.range ? " to " : "") + (it.hi ? expr(*it.hi) : "") + "}";
                    line(r);
                    block(c.body, indent + 1, out);
                    if (c.cont) line("  cont");
                }
                if (!s.elseBody.empty()) {
                    line("default");
                    block(s.elseBody, indent + 1, out);
                }
                line("endselect");
                break;
            case SKind::Call: {
                std::string r = "call " + p_.routines[static_cast<std::size_t>(s.proc)].name;
                for (const auto& a : s.args) r += " " + expr(*a);
                line(r);
                break;
            }
            case SKind::Return: line("return " + expr(*s.value)); break;
            case SKind::Local: {
                std::string r = "local";
                for (int v : s.vars) r += " " + p_.vars[static_cast<std::size_t>(v)].display();
                line(r);
                break;
            }
            case SKind::Goto: line("goto " + p_.labels[static_cast<std::size_t>(s.label)].name); break;
            case SKind::Label: line(p_.labels[static_cast<std::size_t>(s.label)].name + ":"); break;
            case SKind::Read: {
                std::string r = "read";
                for (const auto& t : s.targets) r += " " + expr(*t);
                line(r);
                break;
            }
            case SKind::Restore: line("restore" + (s.label >= 0 ? " " + p_.labels[static_cast<std::size_t>(s.label)].name : "")); break;
            case SKind::OnError: line("onerror" + (s.proc >= 0 ? " " + p_.routines[static_cast<std::size_t>(s.proc)].name : "")); break;
            case SKind::Resume: line("resume" + std::string(s.mode == 1 ? " next" : s.mode == 2 ? " " + p_.labels[static_cast<std::size_t>(s.label)].name : "")); break;
            case SKind::Cmd: {
                std::string r = "cmd " + s.name;
                for (const auto& t : s.targets) r += " " + expr(*t);
                for (const auto& a : s.args) r += " " + expr(*a);
                if (s.value) r += " = " + expr(*s.value);
                for (int v : s.vars) r += " " + p_.routines[static_cast<std::size_t>(v)].name;
                line(r);
                break;
            }
        }
    }

private:
    const Program& p_;
};

}  // namespace

std::string dump(const Program& p, const Expr& e) {
    return Dumper(p).expr(e);
}

std::string dump(const Program& p) {
    Dumper d(p);
    std::string out = "main\n";
    d.block(p.main, 1, out);
    for (const Routine& r : p.routines) {
        std::string head = r.kind == Routine::Procedure ? "procedure " : r.kind == Routine::Function ? "function " : "deffn ";
        head += r.name + "(";
        for (std::size_t i = 0; i < r.params.size(); ++i)
            head += (i ? "," : "") + std::string(r.params[i].byRef ? "VAR " : "") + p.vars[static_cast<std::size_t>(r.params[i].var)].display();
        head += ")";
        if (r.kind != Routine::Procedure) head += std::string(":") + typeName(r.result);
        out += head + "\n";
        if (r.kind == Routine::DefFn) {
            if (r.expr) out += "  = " + d.expr(*r.expr) + "\n";
        } else {
            d.block(r.body, 1, out);
        }
    }
    if (!p.data.empty()) {
        out += "data";
        for (const DataItem& x : p.data) out += x.quoted ? " \"" + x.text + "\"" : " <" + x.text + ">";
        out += "\n";
    }
    return out;
}

}  // namespace basic::hir
