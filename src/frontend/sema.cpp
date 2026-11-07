#include "sema.hpp"

#include <cctype>
#include <cstring>
#include <map>
#include <set>
#include <string>

#include "builtins.hpp"
#include "rt_ref.h"

namespace basic {

namespace {

using ast::ExprKind;
using ast::StmtKind;
using hir::Op;
using hir::SKind;
using hir::Type;

struct SemaError {
    int line, col;
    std::string message;
};

hir::ExprPtr makeExpr(Op op, Type type) {
    auto e = std::make_unique<hir::Expr>();
    e->op = op;
    e->type = type;
    return e;
}

hir::ExprPtr constInt(std::int32_t v) {
    auto e = makeExpr(Op::ConstInt, Type::Int);
    e->ival = v;
    return e;
}

hir::ExprPtr constFloat(f64 v) {
    auto e = makeExpr(Op::ConstFloat, Type::Float);
    e->fval = v;
    return e;
}

// Builtins the runtime does not provide yet (input, files ...).
const std::set<std::string, std::less<>> kNotYet = { "LPOS", "FRE",
};

// Commands handled by the interpreter with their argument forms.
const std::set<std::string, std::less<>> kSimpleCommands = {
    "END", "STOP", "QUIT", "SYSTEM", "EDIT", "DEFNUM", "MODE", "RANDOMIZE", "ERROR", "CLS", "PAUSE", "DELAY",
    "OPTION BASE", "NEW", "CLEAR", "TRON", "TROFF", "LOCATE", "HTAB", "VTAB", "VSYNC",
};

class Sema {
public:
    Sema(const ast::Program& a, Diagnostics& d) : a_(a), diags_(d) { resetDefs(); }

    hir::Program run() {
        collect();
        resetDefs();
        i_ = 0;
        p_.main = block({}, "");
        while (i_ < a_.stmts.size()) {  // a terminator without its opening statement
            unexpected(a_.stmts[i_]);
            ++i_;
            hir::Block more = block({}, "");
            for (auto& st : more) p_.main.push_back(std::move(st));
        }
        checkGotos();
        return std::move(p_);
    }

private:
    const ast::Program& a_;
    Diagnostics& diags_;
    hir::Program p_;
    std::size_t i_ = 0;
    Type defs_[26];
    std::map<std::string, int> vars_;
    std::map<std::string, int> routines_;
    std::map<std::string, int> labels_;
    int routine_ = -1;  // routine being analysed, -1 = main program
    int loops_ = 0;
    int depth_ = 0;     // nesting of blocks; 1 = top level
    struct GotoRef {
        int label, routine, line;
    };
    std::vector<GotoRef> gotos_;

    // ---- diagnostics ------------------------------------------------------------------------
    [[noreturn]] static void fail(const ast::Pos& p, std::string message) { throw SemaError{p.line, p.col, std::move(message)}; }
    void report(const ast::Pos& p, std::string message) { diags_.push_back({p.line, p.col, std::move(message)}); }

    // ---- types and names --------------------------------------------------------------------
    void resetDefs() {
        for (Type& t : defs_) t = Type::Float;
    }

    Type typeOf(const std::string& name) const {
        switch (name.back()) {
            case '!': return Type::Bool;
            case '|': return Type::Byte;
            case '&': return Type::Word;
            case '%': return Type::Int;
            case '#': return Type::Float;
            case '$': return Type::Str;
            default: break;
        }
        const char c = name[0];
        return c >= 'A' && c <= 'Z' ? defs_[c - 'A'] : Type::Float;
    }

    static std::string baseName(const std::string& name) {
        return std::strchr("!|&%#$", name.back()) != nullptr ? name.substr(0, name.size() - 1) : name;
    }

    int varId(const std::string& name, bool array) {
        const Type t = typeOf(name);
        const std::string key = baseName(name) + hir::typePostfix(t) + (array ? "()" : "");
        const auto it = vars_.find(key);
        if (it != vars_.end()) return it->second;
        hir::Var v;
        v.name = baseName(name);
        v.type = t;
        v.array = array;
        p_.vars.push_back(v);
        const int id = static_cast<int>(p_.vars.size()) - 1;
        vars_[key] = id;
        return id;
    }

    // DEFINT "i-k,m" and friends
    void applyDef(const ast::Stmt& s) {
        Type t = Type::Float;
        if (s.name == "DEFINT") t = Type::Int;
        else if (s.name == "DEFWRD") t = Type::Word;
        else if (s.name == "DEFBYT") t = Type::Byte;
        else if (s.name == "DEFBIT") t = Type::Bool;
        else if (s.name == "DEFSTR") t = Type::Str;
        if (s.args.size() != 1 || s.args[0]->kind != ExprKind::String) fail(s.pos, s.name + " expects a string constant");
        const std::string& spec = s.args[0]->text;
        for (std::size_t k = 0; k < spec.size(); ++k) {
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(spec[k])));
            if (c < 'A' || c > 'Z') continue;
            char last = c;
            if (k + 2 < spec.size() && spec[k + 1] == '-') {
                last = static_cast<char>(std::toupper(static_cast<unsigned char>(spec[k + 2])));
                k += 2;
            }
            for (char x = c; x <= last && x <= 'Z'; ++x) defs_[x - 'A'] = t;
        }
    }

    static bool isDef(const ast::Stmt& s) {
        return s.kind == StmtKind::Command && (s.name == "DEFINT" || s.name == "DEFFLT" || s.name == "DEFSTR" ||
                                                s.name == "DEFBIT" || s.name == "DEFBYT" || s.name == "DEFWRD" ||
                                                s.name == "DEFSNG" || s.name == "DEFDBL");
    }

    // ---- pass 1: routines, labels, DATA -----------------------------------------------------
    void collect() {
        for (const ast::Stmt& s : a_.stmts) {
            try {
                if (isDef(s)) applyDef(s);
                if (s.kind == StmtKind::Procedure || s.kind == StmtKind::Function || s.kind == StmtKind::DefFn) {
                    if (routines_.count(s.name) != 0) fail(s.pos, "'" + s.name + "' is defined twice");
                    hir::Routine r;
                    r.name = s.name;
                    r.kind = s.kind == StmtKind::Procedure  ? hir::Routine::Procedure
                             : s.kind == StmtKind::Function ? hir::Routine::Function
                                                            : hir::Routine::DefFn;
                    r.result = s.name.back() == '$' ? Type::Str : typeOf(s.name);
                    if (r.kind != hir::Routine::Procedure && s.name.back() != '$' && r.result == Type::Str)
                        r.result = Type::Float;
                    r.line = s.pos.line;
                    for (const ast::Param& prm : s.params) {
                        if (prm.array && !prm.byRef) fail(s.pos, "array parameters must be VAR");
                        r.params.push_back({varId(prm.name, prm.array), prm.byRef});
                    }
                    p_.routines.push_back(std::move(r));
                    routines_[s.name] = static_cast<int>(p_.routines.size()) - 1;
                }
                if (s.kind == StmtKind::Label) {
                    if (labels_.count(s.name) != 0) fail(s.pos, "label '" + s.name + "' is defined twice");
                    hir::Label l;
                    l.name = s.name;
                    l.data = static_cast<int>(p_.data.size());
                    p_.labels.push_back(l);
                    labels_[s.name] = static_cast<int>(p_.labels.size()) - 1;
                }
                if (s.kind == StmtKind::Data)
                    for (const ast::DataItem& d : s.data) p_.data.push_back({d.text, d.quoted, s.pos.line});
            } catch (const SemaError& e) {
                diags_.push_back({e.line, e.col, e.message});
            }
        }
    }

    // ---- pass 2: blocks ---------------------------------------------------------------------
    // Statements up to (not including) one whose kind is in `stop`, or to the end of the program.
    hir::Block block(std::initializer_list<StmtKind> stop, const char* opener) {
        hir::Block b;
        (void)opener;
        struct Depth {
            int& d;
            explicit Depth(int& x) : d(x) { ++d; }
            ~Depth() { --d; }
        } depth(depth_);
        while (i_ < a_.stmts.size()) {
            const ast::Stmt& s = a_.stmts[i_];
            for (StmtKind k : stop)
                if (s.kind == k) return b;
            if (isTerminator(s.kind)) return b;  // reported by the caller
            ++i_;
            try {
                statement(s, b);
            } catch (const SemaError& e) {
                diags_.push_back({e.line, e.col, e.message});
            }
        }
        return b;
    }

    static bool isTerminator(StmtKind k) {
        switch (k) {
            case StmtKind::ElseIf: case StmtKind::Else: case StmtKind::EndIf: case StmtKind::Next:
            case StmtKind::Wend: case StmtKind::Until: case StmtKind::Loop: case StmtKind::Case:
            case StmtKind::Default: case StmtKind::Cont: case StmtKind::EndSelect: case StmtKind::EndFunc:
                return true;
            default: return false;
        }
    }

    void unexpected(const ast::Stmt& s) {
        static const std::map<StmtKind, const char*> names = {
            {StmtKind::ElseIf, "ELSE IF without IF"}, {StmtKind::Else, "ELSE without IF"},
            {StmtKind::EndIf, "ENDIF without IF"},    {StmtKind::Next, "NEXT without FOR"},
            {StmtKind::Wend, "WEND without WHILE"},   {StmtKind::Until, "UNTIL without REPEAT"},
            {StmtKind::Loop, "LOOP without DO"},      {StmtKind::Case, "CASE without SELECT"},
            {StmtKind::Default, "DEFAULT without SELECT"}, {StmtKind::Cont, "CONT outside SELECT"},
            {StmtKind::EndSelect, "ENDSELECT without SELECT"}, {StmtKind::EndFunc, "ENDFUNC without FUNCTION"},
            {StmtKind::Return, "RETURN without PROCEDURE"},
        };
        const auto it = names.find(s.kind);
        report(s.pos, it != names.end() ? it->second : "unexpected statement");
    }

    // Consumes the expected terminator or reports that it is missing.
    bool expectEnd(StmtKind k, const ast::Stmt& opener, const char* what) {
        if (i_ < a_.stmts.size() && a_.stmts[i_].kind == k) {
            ++i_;
            return true;
        }
        report(opener.pos, std::string(what) + " missing");
        return false;
    }

    std::unique_ptr<hir::Stmt> make(SKind kind, const ast::Stmt& s) {
        auto h = std::make_unique<hir::Stmt>();
        h->kind = kind;
        h->line = s.pos.line;
        return h;
    }

    void statement(const ast::Stmt& s, hir::Block& out) {
        switch (s.kind) {
            case StmtKind::Assign: return assign(s, out);
            case StmtKind::Print: return print(s, out);
            case StmtKind::If: return ifStmt(s, out);
            case StmtKind::For: return forStmt(s, out);
            case StmtKind::While: {
                auto h = make(SKind::While, s);
                h->cond = condition(*s.args[0]);
                h->body = loopBody({StmtKind::Wend});
                expectEnd(StmtKind::Wend, s, "WEND");
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Repeat: {
                auto h = make(SKind::Repeat, s);
                h->body = loopBody({StmtKind::Until});
                if (expectEnd(StmtKind::Until, s, "UNTIL")) h->cond = condition(*a_.stmts[i_ - 1].args[0]);
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Do: {
                auto h = make(SKind::Do, s);
                h->pre = s.variant;
                if (s.variant != 0) h->cond = condition(*s.args[0]);
                h->body = loopBody({StmtKind::Loop});
                if (expectEnd(StmtKind::Loop, s, "LOOP")) {
                    const ast::Stmt& l = a_.stmts[i_ - 1];
                    h->post = l.variant;
                    if (l.variant != 0) h->cond2 = condition(*l.args[0]);
                }
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::ExitIf: {
                if (loops_ == 0) fail(s.pos, "EXIT IF outside a loop");
                auto h = make(SKind::ExitIf, s);
                h->cond = condition(*s.args[0]);
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Select: return selectStmt(s, out);
            case StmtKind::Procedure:
            case StmtKind::Function: return routine(s);
            case StmtKind::DefFn: return defFn(s);
            case StmtKind::Return: {
                if (routine_ >= 0 && p_.routines[static_cast<std::size_t>(routine_)].kind == hir::Routine::Procedure)
                    fail(s.pos, "RETURN must be the last statement of the procedure, not inside a block");
                if (routine_ < 0) {
                    unexpected(s);
                    return;
                }
                auto h = make(SKind::Return, s);
                if (s.args.empty()) fail(s.pos, "RETURN in a FUNCTION needs a value");
                h->value = convertTo(expr(*s.args[0]), p_.routines[static_cast<std::size_t>(routine_)].result, s.pos);
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Local: {
                if (routine_ < 0) fail(s.pos, "LOCAL outside a procedure");
                auto h = make(SKind::Local, s);
                for (const auto& v : s.args) {
                    if (!v->args.empty()) fail(v->pos, "LOCAL takes variables");
                    h->vars.push_back(varId(v->text, v->wholeArray));
                }
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Call: return call(s, out);
            case StmtKind::Goto: {
                auto h = make(SKind::Goto, s);
                h->label = labelId(s);
                gotos_.push_back({h->label, routine_, s.pos.line});
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Label: {
                auto h = make(SKind::Label, s);
                h->label = labels_.at(s.name);
                p_.labels[static_cast<std::size_t>(h->label)].routine = routine_;
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Dim: {
                auto h = make(SKind::Cmd, s);
                h->name = "DIM";
                for (const auto& arr : s.args) {
                    auto t = makeExpr(Op::LoadElem, typeOf(arr->text));
                    t->var = varId(arr->text, true);
                    for (const auto& dim : arr->args) t->args.push_back(toInt(expr(*dim), arr->pos));
                    h->targets.push_back(std::move(t));
                }
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Data: return;  // collected in pass 1
            case StmtKind::Read: {
                auto h = make(SKind::Read, s);
                for (const auto& t : s.args) h->targets.push_back(lvalue(*t));
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Restore: {
                auto h = make(SKind::Restore, s);
                h->label = s.name.empty() ? -1 : labelId(s);
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::OnError: {
                auto h = make(SKind::OnError, s);
                h->proc = s.name.empty() ? -1 : procedureId(s.name, s.pos);
                if (h->proc >= 0 && !p_.routines[static_cast<std::size_t>(h->proc)].params.empty())
                    fail(s.pos, "ON ERROR GOSUB needs a procedure without parameters");
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Resume: {
                auto h = make(SKind::Resume, s);
                h->mode = s.variant;
                if (s.variant == 2) h->label = labelId(s);
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Void: {
                auto h = make(SKind::Cmd, s);
                h->name = "VOID";
                h->args.push_back(expr(*s.args[0]));
                out.push_back(std::move(h));
                return;
            }
            case StmtKind::Command: return command(s, out);
            default: unexpected(s); return;
        }
    }

    hir::Block loopBody(std::initializer_list<StmtKind> stop) {
        ++loops_;
        hir::Block b = block(stop, "");
        --loops_;
        return b;
    }

    void ifStmt(const ast::Stmt& s, hir::Block& out) {
        auto h = make(SKind::If, s);
        hir::Branch first;
        first.cond = condition(*s.args[0]);
        first.body = block({StmtKind::ElseIf, StmtKind::Else, StmtKind::EndIf}, "IF");
        h->branches.push_back(std::move(first));
        while (i_ < a_.stmts.size() && a_.stmts[i_].kind == StmtKind::ElseIf) {
            const ast::Stmt& e = a_.stmts[i_++];
            hir::Branch br;
            try {
                br.cond = condition(*e.args[0]);
            } catch (const SemaError& err) {
                diags_.push_back({err.line, err.col, err.message});
                br.cond = constInt(0);
            }
            br.body = block({StmtKind::ElseIf, StmtKind::Else, StmtKind::EndIf}, "IF");
            h->branches.push_back(std::move(br));
        }
        if (i_ < a_.stmts.size() && a_.stmts[i_].kind == StmtKind::Else) {
            ++i_;
            h->elseBody = block({StmtKind::EndIf}, "IF");
        }
        expectEnd(StmtKind::EndIf, s, "ENDIF");
        out.push_back(std::move(h));
    }

    void forStmt(const ast::Stmt& s, hir::Block& out) {
        auto h = make(SKind::For, s);
        if (!s.args[0]->args.empty() || s.args[0]->wholeArray) fail(s.pos, "FOR needs a simple variable");
        h->target = lvalue(*s.args[0]);
        if (!hir::isNumeric(h->target->type)) fail(s.pos, "FOR needs a numeric variable");
        h->start = convertTo(expr(*s.args[1]), h->target->type, s.args[1]->pos);
        h->end = convertTo(expr(*s.args[2]), Type::Float, s.args[2]->pos);
        h->step = s.args.size() > 3 ? convertTo(expr(*s.args[3]), Type::Float, s.args[3]->pos)
                                    : constFloat(s.variant == 1 ? 0xBFF0000000000000u : F64_ONE);
        h->downto = s.variant == 1;
        h->body = loopBody({StmtKind::Next});
        if (expectEnd(StmtKind::Next, s, "NEXT")) {
            const ast::Stmt& n = a_.stmts[i_ - 1];
            if (!n.args.empty() && varId(n.args[0]->text, false) != h->target->var)
                report(n.pos, "NEXT " + n.args[0]->text + " does not match FOR " + s.args[0]->text);
        }
        out.push_back(std::move(h));
    }

    void selectStmt(const ast::Stmt& s, hir::Block& out) {
        auto h = make(SKind::Select, s);
        h->value = selector(*s.args[0]);
        bool haveDefault = false;
        while (i_ < a_.stmts.size()) {
            const ast::Stmt& c = a_.stmts[i_];
            if (c.kind == StmtKind::Case && !haveDefault) {
                ++i_;
                hir::Case hc;
                for (const ast::CaseItem& item : c.cases) {
                    hir::CaseItem hi;
                    hi.range = item.range;
                    try {  // an error in one value must not end the whole SELECT
                        if (item.lo) hi.lo = caseValue(*item.lo);
                        if (item.hi) hi.hi = caseValue(*item.hi);
                    } catch (const SemaError& err) {
                        diags_.push_back({err.line, err.col, err.message});
                        hi.lo = constInt(0);
                        hi.hi.reset();
                        hi.range = false;
                    }
                    hc.items.push_back(std::move(hi));
                }
                hc.body = block({StmtKind::Case, StmtKind::Default, StmtKind::EndSelect, StmtKind::Cont}, "SELECT");
                if (i_ < a_.stmts.size() && a_.stmts[i_].kind == StmtKind::Cont) {
                    ++i_;
                    hc.cont = true;
                }
                h->cases.push_back(std::move(hc));
            } else if (c.kind == StmtKind::Default && !haveDefault) {
                ++i_;
                haveDefault = true;
                h->elseBody = block({StmtKind::EndSelect}, "SELECT");
            } else {
                break;
            }
        }
        if (!h->cases.empty() && h->cases.back().cont && !haveDefault) {
            report(s.pos, "CONT must be followed by CASE or DEFAULT");
        }
        expectEnd(StmtKind::EndSelect, s, "ENDSELECT");
        out.push_back(std::move(h));
    }

    // SELECT value: numbers are truncated (probe4 T9), strings packed like CASE constants.
    hir::ExprPtr selector(const ast::Expr& e) {
        hir::ExprPtr v = expr(e);
        if (v->type == Type::Str) {
            auto b = makeExpr(Op::Builtin, Type::Int);
            b->sval = "#PACK";
            b->args.push_back(std::move(v));
            return b;
        }
        return toInt(std::move(v), e.pos);
    }

    // CASE constant: up to 4 characters packed big-endian (probe4 T1-T5), or a number.
    hir::ExprPtr caseValue(const ast::Expr& e) {
        if (e.kind == ExprKind::String) {
            if (e.text.size() > 4) fail(e.pos, "CASE strings have at most 4 characters");
            std::uint32_t v = 0;
            for (char c : e.text) v = (v << 8) | static_cast<unsigned char>(c);
            return constInt(static_cast<std::int32_t>(v));
        }
        return toInt(expr(e), e.pos);
    }

    void routine(const ast::Stmt& s) {
        if (routine_ >= 0 || depth_ != 1) fail(s.pos, "PROCEDURE/FUNCTION inside a block");
        const int id = routines_.at(s.name);
        routine_ = id;
        hir::Routine& r = p_.routines[static_cast<std::size_t>(id)];
        if (s.kind == StmtKind::Procedure) {
            r.body = block({StmtKind::Return}, "PROCEDURE");
            if (i_ < a_.stmts.size() && a_.stmts[i_].kind == StmtKind::Return) {
                if (!a_.stmts[i_].args.empty()) report(a_.stmts[i_].pos, "RETURN in a PROCEDURE takes no value");
                ++i_;
            } else {
                report(s.pos, "RETURN missing at the end of PROCEDURE " + s.name);
            }
        } else {
            r.body = block({StmtKind::EndFunc}, "FUNCTION");
            expectEnd(StmtKind::EndFunc, s, "ENDFUNC");
        }
        routine_ = -1;
    }

    void defFn(const ast::Stmt& s) {
        hir::Routine& r = p_.routines[static_cast<std::size_t>(routines_.at(s.name))];
        r.expr = convertTo(expr(*s.args[0]), r.result, s.pos);
    }

    int labelId(const ast::Stmt& s) {
        const auto it = labels_.find(s.name);
        if (it == labels_.end()) fail(s.pos, "label '" + s.name + "' not found");
        return it->second;
    }

    void checkGotos() {
        for (const GotoRef& g : gotos_)
            if (p_.labels[static_cast<std::size_t>(g.label)].routine != g.routine)
                diags_.push_back({g.line, 0, "GOTO into or out of a procedure"});
    }

    int procedureId(const std::string& name, const ast::Pos& pos) {
        const auto it = routines_.find(name);
        if (it == routines_.end() || p_.routines[static_cast<std::size_t>(it->second)].kind != hir::Routine::Procedure)
            fail(pos, "procedure '" + name + "' not found");
        return it->second;
    }

    void call(const ast::Stmt& s, hir::Block& out) {
        auto h = make(SKind::Call, s);
        h->proc = procedureId(s.name, s.pos);
        h->args = arguments(p_.routines[static_cast<std::size_t>(h->proc)], s.args, s.pos);
        out.push_back(std::move(h));
    }

    std::vector<hir::ExprPtr> arguments(const hir::Routine& r, const std::vector<ast::ExprPtr>& args, const ast::Pos& pos) {
        if (args.size() != r.params.size())
            fail(pos, r.name + " expects " + std::to_string(r.params.size()) + " argument(s)");
        std::vector<hir::ExprPtr> out;
        for (std::size_t k = 0; k < args.size(); ++k) {
            const hir::Param& prm = r.params[k];
            const hir::Var& pv = p_.vars[static_cast<std::size_t>(prm.var)];
            if (prm.byRef) {
                const ast::Expr& a = *args[k];
                if (a.kind != ExprKind::Var || !a.args.empty()) fail(a.pos, "var-type mismatch: VAR parameter needs a variable");
                if (a.wholeArray != pv.array) fail(a.pos, pv.array ? "var-type mismatch: VAR parameter needs an array" : "var-type mismatch: VAR parameter needs a variable");
                auto v = makeExpr(Op::Load, typeOf(a.text));
                v->var = varId(a.text, a.wholeArray);
                if (v->type != pv.type) fail(a.pos, "var-type mismatch: VAR parameter has a different type");
                out.push_back(std::move(v));
            } else {
                out.push_back(convertTo(expr(*args[k]), pv.type, args[k]->pos));
            }
        }
        return out;
    }

    // ---- assignment and PRINT ---------------------------------------------------------------
    hir::ExprPtr lvalue(const ast::Expr& e) {
        if (e.kind != ExprKind::Var) fail(e.pos, "variable expected");
        if (e.wholeArray) fail(e.pos, "array element expected");
        auto t = makeExpr(e.args.empty() ? Op::Load : Op::LoadElem, typeOf(e.text));
        t->var = varId(e.text, !e.args.empty());
        for (const auto& ix : e.args) t->args.push_back(toInt(expr(*ix), ix->pos));
        return t;
    }

    void assign(const ast::Stmt& s, hir::Block& out) {
        if (s.args[0]->kind == ExprKind::Call) {  // MID$(a$,x[,y])=b$
            const ast::Expr& m = *s.args[0];
            if (m.args.size() < 2 || m.args.size() > 3) fail(m.pos, "MID$ expects 2 or 3 arguments");
            auto h = make(SKind::Cmd, s);
            h->name = "MID$=";
            h->targets.push_back(lvalue(*m.args[0]));
            if (h->targets[0]->type != Type::Str) fail(m.pos, "MID$ needs a string variable");
            for (std::size_t k = 1; k < m.args.size(); ++k) h->args.push_back(convertTo(expr(*m.args[k]), Type::Float, m.pos));
            h->value = convertTo(expr(*s.args[1]), Type::Str, s.pos);
            out.push_back(std::move(h));
            return;
        }
        auto h = make(SKind::Assign, s);
        h->target = lvalue(*s.args[0]);
        h->value = convertTo(expr(*s.args[1]), h->target->type, s.args[1]->pos);
        out.push_back(std::move(h));
    }

    void print(const ast::Stmt& s, hir::Block& out) {
        if (s.name == "LPRINT") fail(s.pos, "LPRINT is not supported");
        auto h = make(SKind::Print, s);
        if (s.channel) h->channel = toFloat(expr(*s.channel), s.pos);
        if (s.usingFmt) h->usingFmt = convertTo(expr(*s.usingFmt), Type::Str, s.usingFmt->pos);
        for (const ast::PrintItem& it : s.print) {
            hir::PrintItem hi;
            hi.sep = it.sep;
            if (it.expr) {
                const ast::Expr& e = *it.expr;
                if (e.kind == ExprKind::Call && !e.user && e.text == "AT") {  // PRINT AT(column,line) (probe20)
                    if (h->usingFmt || h->channel) fail(e.pos, "AT is not allowed with USING or a channel");
                    if (e.args.size() != 2) fail(e.pos, "AT expects 2 arguments");
                    hi.kind = hir::PrintItem::At;
                    hi.expr = convertTo(expr(*e.args[0]), Type::Float, e.pos);
                    hi.expr2 = convertTo(expr(*e.args[1]), Type::Float, e.pos);
                } else if (e.kind == ExprKind::Call && !e.user && (e.text == "TAB" || e.text == "SPC")) {
                    if (h->usingFmt) fail(e.pos, e.text + " is not allowed with USING");
                    if (e.args.size() != 1) fail(e.pos, e.text + " expects 1 argument");
                    hi.kind = e.text == "TAB" ? hir::PrintItem::Tab : hir::PrintItem::Spc;
                    hi.expr = convertTo(expr(*e.args[0]), Type::Float, e.pos);
                } else {
                    hi.expr = expr(e);
                }
            }
            h->items.push_back(std::move(hi));
        }
        out.push_back(std::move(h));
    }

    // ---- graphics and windows (chapters 8, 9; D25) ------------------------------------------------
    // HIR: cmd <name> with the arguments as Float (a string for TEXT/TITLEW/TITLES); a left out one
    // is 0 and its bit in mode is clear (bit k = argument k given). DRAW: mode 1 for "DRAW TO".
    bool gfxCommand(const ast::Stmt& s, hir::Stmt& h) {
        static const std::map<std::string, std::pair<int, int>, std::less<>> counts = {
            {"OPENS", {1, 7}},    {"CLOSES", {1, 1}},   {"OPENW", {1, 9}},    {"CLOSEW", {1, 1}},   {"CLEARW", {1, 1}},
            {"TITLEW", {2, 3}},   {"TITLES", {2, 2}},   {"FRONTS", {1, 1}},   {"BACKS", {1, 1}},    {"FRONTW", {1, 1}},
            {"BACKW", {1, 1}},    {"COLOR", {1, 3}},    {"SETCOLOR", {2, 4}}, {"GRAPHMODE", {1, 1}}, {"DEFFILL", {1, 3}},
            {"BOUNDARY", {1, 1}}, {"DEFLINE", {1, 1}},  {"PLOT", {2, 2}},     {"LINE", {4, 4}},     {"BOX", {4, 4}},
            {"PBOX", {4, 4}},     {"CIRCLE", {3, 3}},   {"PCIRCLE", {3, 3}},  {"ELLIPSE", {4, 4}},   {"PELLIPSE", {4, 4}},
            {"TEXT", {3, 3}},     {"DRAW", {2, 64}},
        };
        const auto it = counts.find(s.name);
        if (it == counts.end()) return false;
        const auto n = static_cast<int>(s.args.size());
        if (n < it->second.first || n > it->second.second) fail(s.pos, s.name + ": wrong number of arguments");
        if (s.name == "SETCOLOR" && n == 3) fail(s.pos, "SETCOLOR expects reg,rgb or reg,r,g,b");
        if (s.name == "DRAW") {
            h.mode = s.variant;
            for (const auto& a : s.args) h.args.push_back(convertTo(expr(*a), Type::Float, a->pos));
            return true;
        }
        unsigned present = 0;
        for (int k = 0; k < n; ++k) {
            const ast::Expr& a = *s.args[static_cast<std::size_t>(k)];
            if (a.kind == ExprKind::Missing) {
                if (s.name != "COLOR" && s.name != "DEFFILL") fail(a.pos, s.name + ": argument expected");
                auto z = makeExpr(Op::ConstFloat, Type::Float);
                z->fval = 0;
                h.args.push_back(std::move(z));
                continue;
            }
            present |= 1u << k;
            const bool text = (s.name == "TEXT" && k == 2) || ((s.name == "TITLEW" || s.name == "TITLES") && k >= 1);
            if (text) {
                hir::ExprPtr v = expr(a);
                if (v->type != Type::Str) {  // a number as PRINT writes it
                    auto c = makeExpr(Op::Builtin, Type::Str);
                    c->sval = "STR$";
                    c->args.push_back(convertTo(std::move(v), Type::Float, a.pos));
                    v = std::move(c);
                }
                h.args.push_back(std::move(v));
            } else {
                h.args.push_back(convertTo(expr(a), Type::Float, a.pos));
            }
        }
        h.mode = static_cast<int>(present);
        return true;
    }

    // ---- MAT (chapter 13, D23) ----------------------------------------------------------------
    // a() or a(i[,j]) (offsets): a float array as a Load of the array variable, offsets as args (Int)
    hir::ExprPtr matArray(const ast::Expr& a, bool offsets) {
        if (a.kind != ExprKind::Var || (!a.wholeArray && (!offsets || a.args.empty() || a.args.size() > 2)))
            fail(a.pos, offsets ? "MAT expects an array a() or a(i,j)" : "MAT expects an array a()");
        if (typeOf(a.text) != Type::Float) fail(a.pos, "MAT works on arrays of floats only");
        auto v = makeExpr(Op::Load, Type::Float);
        v->var = varId(a.text, true);
        for (const auto& x : a.args) v->args.push_back(convertTo(expr(*x), Type::Int, x->pos));
        return v;
    }

    static bool isArrayArg(const ast::Expr& a) { return a.kind == ExprKind::Var && a.wholeArray; }

    // HIR layout (mode = form): see docs/ir.md, MAT
    void matCommand(const ast::Stmt& s, hir::Stmt& h) {
        const std::string w = s.name.substr(4);
        const auto& a = s.args;
        const bool eq = s.variant != 0;
        const std::size_t rhs = eq ? static_cast<std::size_t>((s.variant - 1) / 2) : 0;  // operands after '='
        const std::size_t extra = a.size() - (w == "PRINT" || w == "INPUT" || w == "BASE" ? 0 : 1 + rhs);
        auto bad = [&]() { fail(s.pos, s.name + ": unexpected form"); };
        auto num = [&](std::size_t k, Type t) { h.args.push_back(convertTo(expr(*a[k]), t, a[k]->pos)); };
        if (w == "BASE") {
            if (a.size() != 1) bad();
            num(0, Type::Int);
        } else if (w == "CLR" || w == "ONE" || w == "READ") {
            if (eq || extra != 0) bad();
            h.targets.push_back(matArray(*a[0], false));
        } else if (w == "SET") {
            if (rhs != 1 || extra != 0) bad();
            h.targets.push_back(matArray(*a[0], false));
            num(1, Type::Float);
        } else if (w == "PRINT") {
            if (a.size() != 1 && a.size() != 3) bad();
            if (s.channel) h.channel = convertTo(expr(*s.channel), Type::Float, s.pos);
            h.targets.push_back(matArray(*a[0], false));
            for (std::size_t k = 1; k < a.size(); ++k) num(k, Type::Float);
        } else if (w == "INPUT") {  // MAT INPUT #c,a(): numbers like INPUT #, row by row (probe18)
            if (!s.channel) fail(s.pos, "MAT INPUT needs a channel: MAT INPUT #c,a()");
            if (a.size() != 1) bad();
            h.channel = convertTo(expr(*s.channel), Type::Float, s.pos);
            h.targets.push_back(matArray(*a[0], false));
        } else if (w == "CPY" || w == "XCPY") {
            if (rhs != 1 || (extra != 0 && extra != 2)) bad();
            h.targets.push_back(matArray(*a[0], true));
            h.targets.push_back(matArray(*a[1], true));
            for (std::size_t k = 2; k < a.size(); ++k) num(k, Type::Int);
        } else if (w == "TRANS" || w == "INV") {
            if (extra != 0 || rhs > 1 || (w == "INV" && rhs != 1)) bad();
            h.targets.push_back(matArray(*a[0], false));
            if (rhs == 1) h.targets.push_back(matArray(*a[1], false));
        } else if (w == "ADD" || w == "SUB") {
            h.targets.push_back(matArray(*a[0], false));
            if (rhs == 2 && extra == 0) {
                h.mode = 0;
                h.targets.push_back(matArray(*a[1], false));
                h.targets.push_back(matArray(*a[2], false));
            } else if (!eq && extra == 1 && isArrayArg(*a[1])) {
                h.mode = 1;
                h.targets.push_back(matArray(*a[1], false));
            } else if (!eq && extra == 1) {
                h.mode = 2;
                num(1, Type::Float);
            } else {
                bad();
            }
        } else if (w == "MUL") {
            if (eq && extra == 0 && isArrayArg(*a[0]) && rhs == 2) {
                h.mode = 0;
                for (std::size_t k = 0; k < 3; ++k) h.targets.push_back(matArray(*a[k], false));
            } else if (eq && extra == 0 && !isArrayArg(*a[0]) && (rhs == 2 || rhs == 3)) {
                h.mode = rhs == 2 ? 1 : 2;
                h.targets.push_back(numericTarget(*a[0]));
                for (std::size_t k = 1; k <= rhs; ++k) h.targets.push_back(matArray(*a[k], false));
            } else if (!eq && extra == 1 && !isArrayArg(*a[1])) {
                h.mode = 3;
                h.targets.push_back(matArray(*a[0], false));
                num(1, Type::Float);
            } else {
                bad();
            }
        } else if (w == "NORM") {
            if (eq || extra != 1) bad();
            h.targets.push_back(matArray(*a[0], false));
            num(1, Type::Int);
        } else if (w == "DET" || w == "QDET" || w == "RANG") {
            if (rhs != 1 || extra > 1) bad();
            h.targets.push_back(numericTarget(*a[0]));
            h.targets.push_back(matArray(*a[1], true));
            if (extra == 1) num(2, Type::Int);
        }
    }

    // ---- commands ---------------------------------------------------------------------------
    void command(const ast::Stmt& s, hir::Block& out) {
        if (isDef(s)) {
            applyDef(s);
            return;
        }
        auto h = make(SKind::Cmd, s);
        h->name = s.name;
        const std::string& n = s.name;
        auto argc = [&](std::size_t lo, std::size_t hi) {
            if (s.args.size() < lo || s.args.size() > hi) fail(s.pos, n + ": wrong number of arguments");
        };
        if (n.rfind("MAT ", 0) == 0) {
            matCommand(s, *h);
        } else if (gfxCommand(s, *h)) {
        } else if (n == "INC" || n == "DEC") {
            argc(1, 1);
            h->targets.push_back(numericTarget(*s.args[0]));
        } else if (n == "ADD" || n == "SUB" || n == "MUL" || n == "DIV") {
            argc(2, 2);
            h->targets.push_back(numericTarget(*s.args[0]));
            h->args.push_back(convertTo(expr(*s.args[1]), Type::Float, s.args[1]->pos));
        } else if (n == "SWAP" && s.args.size() == 2 &&
                   (s.args[0]->kind == ExprKind::Call && s.args[0]->text == "ARRPTR" && !s.args[0]->args[0]->wholeArray) !=
                       (s.args[1]->kind == ExprKind::Call && s.args[1]->text == "ARRPTR" && !s.args[1]->args[0]->wholeArray)) {
            // SWAP *p,a() (or a(),*p): exchange a() with the array whose descriptor p points to (probe19 T4)
            const bool first = s.args[0]->kind == ExprKind::Call;
            const ast::Expr& ptr = *s.args[first ? 0 : 1]->args[0];
            const ast::Expr& arr = *s.args[first ? 1 : 0];
            if (arr.kind != ExprKind::Var || !arr.wholeArray) fail(arr.pos, "SWAP *p needs an array a() as the other argument");
            h->mode = 1;
            h->args.push_back(convertTo(expr(ptr), Type::Float, ptr.pos));
            auto v = makeExpr(Op::Load, typeOf(arr.text));
            v->var = varId(arr.text, true);
            h->targets.push_back(std::move(v));
        } else if (n == "ABSOLUTE") {
            fail(s.pos, "ABSOLUTE is not supported yet (needs the memory image of D6)");
        } else if (n == "SWAP") {
            argc(2, 2);
            for (const auto& a : s.args) {
                if (a->kind != ExprKind::Var) fail(a->pos, "SWAP needs variables");
                if (a->wholeArray) {
                    auto v = makeExpr(Op::Load, typeOf(a->text));
                    v->var = varId(a->text, true);
                    h->targets.push_back(std::move(v));
                } else {
                    h->targets.push_back(lvalue(*a));
                }
            }
            const hir::Expr& x = *h->targets[0];
            const hir::Expr& y = *h->targets[1];
            if (x.type != y.type || p_.vars[static_cast<std::size_t>(x.var)].array != p_.vars[static_cast<std::size_t>(y.var)].array ||
                (x.op == Op::Load) != (y.op == Op::Load))
                fail(s.pos, "SWAP needs two variables of the same type");
        } else if (n == "CLR" || n == "ERASE") {
            argc(1, 64);
            for (const auto& a : s.args) {
                if (a->kind != ExprKind::Var || !a->args.empty() || (n == "ERASE") != a->wholeArray)
                    fail(a->pos, n == "ERASE" ? "ERASE needs arrays: a()" : "CLR needs variables");
                auto v = makeExpr(Op::Load, typeOf(a->text));
                v->var = varId(a->text, a->wholeArray);
                h->targets.push_back(std::move(v));
            }
        } else if (n == "LSET" || n == "RSET") {
            argc(1, 1);
            const ast::Expr& e = *s.args[0];
            if (e.kind != ExprKind::Binary || e.text != "=") fail(s.pos, n + " expects a$=b$");
            h->targets.push_back(lvalue(*e.args[0]));
            if (h->targets[0]->type != Type::Str) fail(s.pos, n + " needs a string variable");
            h->value = convertTo(expr(*e.args[1]), Type::Str, e.pos);
        } else if (n == "ALERT") {  // ALERT symbol,text$,default,buttons$,choice (D26)
            argc(5, 5);
            h->args.push_back(convertTo(expr(*s.args[0]), Type::Float, s.args[0]->pos));
            h->args.push_back(convertTo(expr(*s.args[1]), Type::Str, s.args[1]->pos));
            h->args.push_back(convertTo(expr(*s.args[2]), Type::Float, s.args[2]->pos));
            h->args.push_back(convertTo(expr(*s.args[3]), Type::Str, s.args[3]->pos));
            h->targets.push_back(numericTarget(*s.args[4]));
        } else if (n == "FILESELECT") {  // FILESELECT title$,ok$,path$,name$ (as the syntax line; D26)
            argc(4, 4);
            for (std::size_t k = 0; k < 3; ++k) h->args.push_back(convertTo(expr(*s.args[k]), Type::Str, s.args[k]->pos));
            h->targets.push_back(lvalue(*s.args[3]));
            if (h->targets[0]->type != Type::Str) fail(s.args[3]->pos, "FILESELECT needs a string variable");
        } else if (n == "FORM INPUT") {
            h->args.push_back(convertTo(expr(*s.args[0]), Type::Float, s.args[0]->pos));
            h->targets.push_back(lvalue(*s.args[1]));
            if (h->targets[0]->type != Type::Str) fail(s.args[1]->pos, "FORM INPUT needs a string variable");
            h->mode = s.variant;
        } else if (n == "MENU") {  // MENU m$() / MENU x,flags / MENU x,text$
            argc(1, 2);
            if (s.args.size() == 1) {
                const ast::Expr& a = *s.args[0];
                if (a.kind != ExprKind::Var || !a.wholeArray || typeOf(a.text) != Type::Str) fail(a.pos, "MENU expects a string array: m$()");
                auto v = makeExpr(Op::Load, Type::Str);
                v->var = varId(a.text, true);
                h->targets.push_back(std::move(v));
                h->mode = 0;
            } else {
                h->args.push_back(convertTo(expr(*s.args[0]), Type::Float, s.args[0]->pos));
                hir::ExprPtr v = expr(*s.args[1]);
                h->mode = v->type == Type::Str ? 2 : 1;
                h->args.push_back(v->type == Type::Str ? std::move(v) : convertTo(std::move(v), Type::Float, s.args[1]->pos));
            }
        } else if (n == "MENU KILL" || n == "MENU KEY" || n == "SLEEP") {
            for (const auto& a : s.args) h->args.push_back(convertTo(expr(*a), Type::Float, a->pos));
        } else if (n == "ON MENU") {
            for (const auto& a : s.args) h->args.push_back(convertTo(expr(*a), Type::Float, a->pos));
        } else if (n.rfind("ON MENU", 0) == 0 || n == "ON MESSAGE GOSUB") {  // ... GOSUB proc
            const int id = procedureId(s.args[0]->text, s.args[0]->pos);
            if (!p_.routines[static_cast<std::size_t>(id)].params.empty()) fail(s.args[0]->pos, n + " needs a procedure without parameters");
            h->vars.push_back(id);
        } else if (n == "ON") {
            h->name = "ON GOSUB";
            h->args.push_back(convertTo(expr(*s.args[0]), Type::Float, s.args[0]->pos));
            for (std::size_t k = 1; k < s.args.size(); ++k) {
                const int id = procedureId(s.args[k]->text, s.args[k]->pos);
                if (!p_.routines[static_cast<std::size_t>(id)].params.empty())
                    fail(s.args[k]->pos, "ON GOSUB needs procedures without parameters");
                h->vars.push_back(id);
            }
        } else if (n == "OPEN") {  // OPEN mode$,#n,name$[,record length] (probe14, probe16)
            argc(3, 4);
            if (!isChannel(*s.args[1])) fail(s.args[1]->pos, "OPEN expects a file channel: #n");
            h->args.push_back(convertTo(expr(*s.args[0]), Type::Str, s.args[0]->pos));
            h->args.push_back(channel(*s.args[1]));
            h->args.push_back(convertTo(expr(*s.args[2]), Type::Str, s.args[2]->pos));
            if (s.args.size() == 4) h->args.push_back(convertTo(expr(*s.args[3]), Type::Float, s.args[3]->pos));
        } else if (n == "CLOSE") {
            argc(0, 1);
            if (!s.args.empty()) {
                if (!isChannel(*s.args[0])) fail(s.args[0]->pos, "CLOSE expects a file channel: #n");
                h->args.push_back(channel(*s.args[0]));
            }
        } else if (n == "FIELD") {  // FIELD #n,len AS a$[,len AS b$...]: args #n, len, len...; targets
            if (s.args.size() < 3 || !isChannel(*s.args[0]) || s.args.size() % 2 == 0)
                fail(s.pos, "FIELD expects #n,length AS a$[,...] (AT is not supported yet)");
            h->args.push_back(channel(*s.args[0]));
            for (std::size_t k = 1; k + 1 < s.args.size(); k += 2) {
                h->args.push_back(convertTo(expr(*s.args[k]), Type::Float, s.args[k]->pos));
                const ast::Expr& v = *s.args[k + 1];
                if (v.kind != ExprKind::Var || !v.args.empty()) fail(v.pos, "FIELD needs string variables");
                h->targets.push_back(lvalue(v));
                if (h->targets.back()->type != Type::Str) fail(v.pos, "FIELD needs string variables");
            }
        } else if ((n == "GET" || n == "PUT") && !s.args.empty() && isChannel(*s.args[0])) {  // GET #n[,record]
            argc(1, 2);
            h->name = n + "#";
            h->args.push_back(channel(*s.args[0]));
            if (s.args.size() == 2) h->args.push_back(convertTo(expr(*s.args[1]), Type::Float, s.args[1]->pos));
        } else if (n == "RECORD" || n == "SEEK" || n == "RELSEEK") {  // #n,number
            argc(2, 2);
            if (!isChannel(*s.args[0])) fail(s.args[0]->pos, n + " expects a file channel: #n");
            h->args.push_back(channel(*s.args[0]));
            h->args.push_back(convertTo(expr(*s.args[1]), Type::Float, s.args[1]->pos));
        } else if (n == "OUT" && !s.args.empty() && isChannel(*s.args[0])) {  // OUT #n,byte[,byte...]
            argc(2, 256);
            h->args.push_back(channel(*s.args[0]));
            for (std::size_t k = 1; k < s.args.size(); ++k) h->args.push_back(convertTo(expr(*s.args[k]), Type::Float, s.args[k]->pos));
        } else if (n == "POKE" || n == "DPOKE" || n == "LPOKE" || n == "BMOVE") {  // address, value / from, to, count
            argc(n == "BMOVE" ? 3 : 2, n == "BMOVE" ? 3 : 2);
            for (const auto& x : s.args) h->args.push_back(convertTo(expr(*x), Type::Float, x->pos));
        } else if (n.size() > 2 && n.substr(n.size() - 2) == "{}") {  // BYTE{a}=v: address, value
            argc(2, 2);
            h->args.push_back(convertTo(expr(*s.args[0]), Type::Float, s.args[0]->pos));
            h->args.push_back(convertTo(expr(*s.args[1]), n == "CHAR{}" ? Type::Str : Type::Float, s.args[1]->pos));
        } else if (n == "NAME" || n == "RENAME") {  // NAME old$ AS new$
            argc(2, 2);
            h->name = "NAME";
            h->args.push_back(convertTo(expr(*s.args[0]), Type::Str, s.args[0]->pos));
            h->args.push_back(convertTo(expr(*s.args[1]), Type::Str, s.args[1]->pos));
        } else if (n == "KILL") {
            argc(1, 1);
            h->args.push_back(convertTo(expr(*s.args[0]), Type::Str, s.args[0]->pos));
        } else if (n == "INPUT" || n == "LINE INPUT") {
            std::size_t first = 1;
            if (!s.args.empty() && isChannel(*s.args[0])) {  // from a file
                argc(2, 64);
                h->name = n + "#";
                h->args.push_back(channel(*s.args[0]));
            } else {  // from the keyboard: prompt (mode 1: "text"; 2: "text",), then the variables
                h->mode = s.variant;
                auto prompt = makeExpr(Op::ConstStr, Type::Str);
                if (s.variant != 0) prompt->sval = s.args[0]->text;
                else first = 0;
                h->args.push_back(std::move(prompt));
                if (s.args.size() <= first) fail(s.pos, n + " needs variables");
            }
            for (std::size_t k = first; k < s.args.size(); ++k) {
                if (s.args[k]->kind != ExprKind::Var) fail(s.args[k]->pos, n + " needs variables");
                h->targets.push_back(lvalue(*s.args[k]));
                if (n == "LINE INPUT" && h->targets.back()->type != Type::Str) fail(s.args[k]->pos, "LINE INPUT needs string variables");
            }
        } else if (n == "WRITE") {  // WRITE [#n,]values[;]: strings in quotes, separated by commas
            h->mode = s.variant;  // 1: no line feed
            std::size_t k = 0;
            if (!s.args.empty() && isChannel(*s.args[0])) {
                h->name = "WRITE#";
                h->args.push_back(channel(*s.args[0]));
                k = 1;
            }
            for (; k < s.args.size(); ++k) {
                hir::ExprPtr v = expr(*s.args[k]);
                if (v->type != Type::Str) v = toFloat(std::move(v), s.args[k]->pos);
                h->args.push_back(std::move(v));
            }
        } else if (n == "ON BREAK") {
            return;  // no break key in compiled programs
        } else if (kSimpleCommands.count(n) != 0) {
            if (n == "LOCATE") argc(2, 2);  // LOCATE column,line (probe20 T3; the reference has it reversed)
            else if (n == "HTAB" || n == "VTAB") argc(1, 1);
            else argc(0, 1);
            if (n == "OPTION BASE" && (s.args.size() != 1 || s.args[0]->kind != ExprKind::Number))
                fail(s.pos, "OPTION BASE expects 0 or 1");
            for (const auto& a : s.args) h->args.push_back(convertTo(expr(*a), Type::Float, a->pos));
        } else {
            fail(s.pos, n + " is not supported yet");
        }
        out.push_back(std::move(h));
    }

    static bool isChannel(const ast::Expr& e) { return e.kind == ExprKind::Unary && e.text == "#"; }
    hir::ExprPtr channel(const ast::Expr& e) { return toFloat(expr(*e.args[0]), e.pos); }

    hir::ExprPtr numericTarget(const ast::Expr& e) {
        hir::ExprPtr t = lvalue(e);
        if (!hir::isNumeric(t->type)) fail(e.pos, "numeric variable expected");
        return t;
    }

    // ---- expressions ------------------------------------------------------------------------
    // Conversion with the semantics of an assignment to a variable of type `t`.
    hir::ExprPtr convertTo(hir::ExprPtr e, Type t, const ast::Pos& pos) {
        if (e->type == t) return e;
        if ((t == Type::Str) != (e->type == Type::Str)) fail(pos, "type mismatch");
        if (t == Type::Float && e->op == Op::ConstInt) return constFloat(fp_from_i32(e->ival));
        if (t == Type::Int && hir::isInteger(e->type)) return e;  // Bool/Byte/Word values are Int values
        auto c = makeExpr(Op::Conv, t);
        c->args.push_back(std::move(e));
        return c;
    }

    hir::ExprPtr toFloat(hir::ExprPtr e, const ast::Pos& pos) {
        if (e->type == Type::Str) fail(pos, "type mismatch");
        return convertTo(std::move(e), Type::Float, pos);
    }

    // Operand of AND, DIV, NOT, SELECT ...: truncated to a 32-bit integer.
    hir::ExprPtr toInt(hir::ExprPtr e, const ast::Pos& pos) {
        if (e->type == Type::Str) fail(pos, "type mismatch");
        if (hir::isInteger(e->type)) return e;  // Bool/Byte/Word values are Int values
        auto c = makeExpr(Op::ToInt, Type::Int);
        c->args.push_back(std::move(e));
        return c;
    }

    hir::ExprPtr condition(const ast::Expr& e) {
        hir::ExprPtr c = expr(e);
        if (c->type == Type::Str) fail(e.pos, "condition must be numeric");
        return c;
    }

    hir::ExprPtr expr(const ast::Expr& e) {
        switch (e.kind) {
            case ExprKind::Number: return number(e);
            case ExprKind::String: {
                auto s = makeExpr(Op::ConstStr, Type::Str);
                s->sval = e.text;
                return s;
            }
            case ExprKind::Var: {
                if (e.wholeArray) fail(e.pos, "array element expected");
                // variables and procedures have separate names (new.colors(farbe) with PROCEDURE farbe)
                auto v = makeExpr(e.args.empty() ? Op::Load : Op::LoadElem, typeOf(e.text));
                v->var = varId(e.text, !e.args.empty());
                for (const auto& ix : e.args) v->args.push_back(toInt(expr(*ix), ix->pos));
                return v;
            }
            case ExprKind::Unary: return unary(e);
            case ExprKind::Binary: return binary(e);
            case ExprKind::Call: return e.user ? userCall(e) : builtin(e);
            case ExprKind::Missing: fail(e.pos, "argument expected");
        }
        fail(e.pos, "invalid expression");
    }

    hir::ExprPtr number(const ast::Expr& e) {
        const std::string& t = e.text;
        std::int32_t used = 0;
        const f64 v = rt_val(t.c_str(), static_cast<std::int32_t>(t.size()), &used);
        if (used != static_cast<std::int32_t>(t.size())) fail(e.pos, "invalid number " + t);
        const bool radix = t[0] == '&' || t[0] == '$' || t[0] == '%';
        bool plain = true;
        for (char c : t)
            if (c < '0' || c > '9') plain = false;
        if (radix || (plain && t.size() <= 10)) {
            // integer literal if it fits into 32 bits
            const fp_parts_view pv = view(v);
            if (radix || pv.integral) return constInt(pv.value);
        }
        return constFloat(v);
    }

    struct fp_parts_view {
        bool integral;
        std::int32_t value;
    };

    static fp_parts_view view(f64 v) {
        // integral and within the Int range? (no rt_error: checked with fp_cmp first)
        const f64 lo = 0xC1E0000000000000u, hi = 0x41DFFFFFFFC00000u;  // -2^31, 2^31-1
        if (fp_cmp(v, lo) < 0 || fp_cmp(v, hi) > 0 || fp_cmp(fp_trunc(v), v) != 0) return {false, 0};
        return {true, fp_to_i32(v)};
    }

    hir::ExprPtr unary(const ast::Expr& e) {
        hir::ExprPtr a = expr(*e.args[0]);
        if (e.text == "+") {
            if (a->type == Type::Str) fail(e.pos, "type mismatch");
            return a;
        }
        if (e.text == "-") {
            if (a->op == Op::ConstFloat) {
                a->fval = fp_neg(a->fval);
                return a;
            }
            auto n = makeExpr(Op::Neg, Type::Float);
            n->args.push_back(toFloat(std::move(a), e.pos));
            return n;
        }
        if (e.text == "#") fail(e.pos, "files are not supported yet");
        auto n = makeExpr(Op::Not, Type::Int);
        n->args.push_back(toInt(std::move(a), e.pos));
        return n;
    }

    hir::ExprPtr binary(const ast::Expr& e) {
        hir::ExprPtr l = expr(*e.args[0]);
        hir::ExprPtr r = expr(*e.args[1]);
        const std::string& op = e.text;
        const bool str = l->type == Type::Str || r->type == Type::Str;
        if (str && l->type != r->type) fail(e.pos, "type mismatch");
        static const std::map<std::string, Op> cmp = {{"=", Op::Eq}, {"<>", Op::Ne}, {"<", Op::Lt},
                                                      {">", Op::Gt}, {"<=", Op::Le}, {">=", Op::Ge}};
        if (const auto it = cmp.find(op); it != cmp.end()) {
            auto c = makeExpr(it->second, Type::Int);
            if (!str) {
                l = toFloat(std::move(l), e.pos);
                r = toFloat(std::move(r), e.pos);
            }
            c->args.push_back(std::move(l));
            c->args.push_back(std::move(r));
            return c;
        }
        if (op == "+" && str) {
            auto c = makeExpr(Op::Concat, Type::Str);
            c->args.push_back(std::move(l));
            c->args.push_back(std::move(r));
            return c;
        }
        if (str) fail(e.pos, "'" + op + "' is not defined for strings");
        static const std::map<std::string, Op> flt = {{"+", Op::Add}, {"-", Op::Sub}, {"*", Op::Mul},
                                                      {"/", Op::Div}, {"^", Op::Pow}, {"==", Op::ApproxEq}};
        if (const auto it = flt.find(op); it != flt.end()) {
            auto c = makeExpr(it->second, op == "==" ? Type::Int : Type::Float);
            c->args.push_back(toFloat(std::move(l), e.pos));
            c->args.push_back(toFloat(std::move(r), e.pos));
            return c;
        }
        static const std::map<std::string, Op> ints = {{"MOD", Op::IMod}, {"DIV", Op::IDiv}, {"\\", Op::IDiv},
                                                       {"AND", Op::And},  {"OR", Op::Or},    {"XOR", Op::Xor},
                                                       {"IMP", Op::Imp},  {"EQV", Op::Eqv}};
        const auto it = ints.find(op);
        if (it == ints.end()) fail(e.pos, "unknown operator " + op);
        auto c = makeExpr(it->second, Type::Int);
        c->args.push_back(toInt(std::move(l), e.pos));
        c->args.push_back(toInt(std::move(r), e.pos));
        return c;
    }

    hir::ExprPtr userCall(const ast::Expr& e) {
        const auto it = routines_.find(e.text);
        if (it == routines_.end() || p_.routines[static_cast<std::size_t>(it->second)].kind == hir::Routine::Procedure)
            fail(e.pos, "function '" + e.text + "' not found");
        const hir::Routine& r = p_.routines[static_cast<std::size_t>(it->second)];
        auto c = makeExpr(Op::CallFunc, r.result);
        c->func = it->second;
        c->args = arguments(r, e.args, e.pos);
        return c;
    }

    hir::ExprPtr builtin(const ast::Expr& e) {
        const Builtin* b = findBuiltin(e.text);
        if (b->printOnly) fail(e.pos, e.text + " is only allowed in PRINT");
        if (kNotYet.count(e.text) != 0) fail(e.pos, e.text + " is not supported yet");

        const int n = static_cast<int>(e.args.size());
        if (n < b->minArgs || (b->maxArgs >= 0 && n > b->maxArgs))
            fail(e.pos, e.text + ": wrong number of arguments");
        auto c = makeExpr(Op::Builtin, Type::Int);
        c->sval = e.text;
        bool anyStr = false, allStr = true;
        for (int k = 0; k < n; ++k) {
            const ast::Expr& a = *e.args[static_cast<std::size_t>(k)];
            const char kind = b->args.empty() ? 'x' : b->args[std::min<std::size_t>(static_cast<std::size_t>(k), b->args.size() - 1)];
            if (kind == 'a') {
                if (a.kind != ExprKind::Var || !a.wholeArray) fail(a.pos, e.text + " expects an array: a()");
                auto v = makeExpr(Op::Load, typeOf(a.text));
                v->var = varId(a.text, true);
                c->args.push_back(std::move(v));
                continue;
            }
            if (kind == 'r') {  // ARRPTR, *x: a variable or a whole array (its descriptor)
                if (a.kind != ExprKind::Var) fail(a.pos, e.text + " expects a variable or an array a()");
                if (a.wholeArray) {
                    auto v = makeExpr(Op::Load, typeOf(a.text));
                    v->var = varId(a.text, true);
                    c->args.push_back(std::move(v));
                } else {
                    c->args.push_back(lvalue(a));
                }
                continue;
            }
            if (kind == 'v') {  // a variable or an array element (VARPTR, V:)
                if (a.kind != ExprKind::Var || a.wholeArray) fail(a.pos, e.text + " expects a variable or an array element");
                c->args.push_back(lvalue(a));
                continue;
            }
            if (kind == 'c') {  // file channel #n
                if (a.kind != ExprKind::Unary || a.text != "#") fail(a.pos, e.text + " expects a file channel: #n");
                c->args.push_back(toFloat(expr(*a.args[0]), a.pos));
                continue;
            }
            hir::ExprPtr v = expr(a);
            if (kind == 'n') v = toFloat(std::move(v), a.pos);
            else if (kind == 's' && v->type != Type::Str) fail(a.pos, e.text + " expects a string here");
            else if (kind == 'x' && v->type != Type::Str) v = toFloat(std::move(v), a.pos);
            anyStr = anyStr || v->type == Type::Str;
            allStr = allStr && v->type == Type::Str;
            c->args.push_back(std::move(v));
        }
        switch (b->result) {
            case 'f': c->type = Type::Float; break;
            case 's': c->type = Type::Str; break;
            case '?':
                if (anyStr && !allStr) fail(e.pos, e.text + ": mixed strings and numbers");
                c->type = allStr && n > 0 ? Type::Str : (e.text == "MIN" || e.text == "MAX" ? Type::Float : Type::Int);
                break;
            default: c->type = Type::Int; break;
        }
        return c;
    }
};

}  // namespace

hir::Program analyze(const ast::Program& program, Diagnostics& diags) {
    Sema sema(program, diags);
    return sema.run();
}

}  // namespace basic
