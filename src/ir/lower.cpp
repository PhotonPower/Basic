#include "lower.hpp"

#include <functional>
#include <map>
#include <string>

#include "rt_ref.h"

namespace basic {

namespace {

using hir::Op;
using hir::SKind;
using hir::Type;
using lir::Instr;
using lir::Kind;
using lir::Reg;
using lir::VT;

struct Unsupported {
    int line;
    std::string what;
};

class Lowering {
public:
    Lowering(const hir::Program& p, Diagnostics& d) : p_(p), diags_(d) {}

    lir::Program run() {
        strings_ = usesStrings();
        lir::Program out;
        out.hir = &p_;
        lir::Function main;
        main.name = "main";
        function(main, p_.main, nullptr);
        out.functions.push_back(std::move(main));
        for (std::size_t r = 0; r < p_.routines.size(); ++r) {
            lir::Function f;
            f.routine = static_cast<int>(r);
            f.name = p_.routines[r].name;
            function(f, p_.routines[r].body, &p_.routines[r]);
            out.functions.push_back(std::move(f));
        }
        return out;
    }

private:
    const hir::Program& p_;
    Diagnostics& diags_;
    lir::Function* f_ = nullptr;
    const hir::Routine* routine_ = nullptr;
    int line_ = 0;
    std::vector<int> exitLabels_;   // EXIT IF targets
    std::vector<int> saved_;        // variables saved in this routine (restore in reverse order)
    int exitLabel_ = -1;            // end of the routine
    Reg result_;                    // function result (numbers; strings go through rt_str_set_ret)
    bool strings_ = false;          // the program uses string values: release temporaries (D18)
    Reg base_;                      // number of string temporaries at the entry of the routine

    [[noreturn]] void unsupported(const std::string& what) { throw Unsupported{line_, what}; }

    static VT vt(Type t) { return t == Type::Float ? VT::F64 : t == Type::Str ? VT::Str : VT::I32; }

    Reg reg(VT t) {
        Reg r;
        r.id = f_->regs++;
        r.type = t;
        f_->regTypes.push_back(t);
        return r;
    }

    int label() { return f_->labels++; }

    Instr& emit(Kind k) {
        f_->code.push_back(Instr());
        f_->code.back().kind = k;
        return f_->code.back();
    }

    Reg constI(std::int32_t v) {
        Reg r = reg(VT::I32);
        Instr& i = emit(Kind::ConstI);
        i.dst = r;
        i.imm = v;
        return r;
    }

    Reg constF(std::uint64_t v) {
        Reg r = reg(VT::F64);
        Instr& i = emit(Kind::ConstF);
        i.dst = r;
        i.fimm = v;
        return r;
    }

    Reg rtcall(const std::string& sym, std::vector<Reg> args, VT result, bool hasResult = true) {
        Instr i;
        i.kind = Kind::RtCall;
        i.op = sym;
        i.args = std::move(args);
        if (hasResult) i.dst = reg(result);
        f_->code.push_back(i);
        return i.dst;
    }

    void rtcallVoid(const std::string& sym, std::vector<Reg> args) { rtcall(sym, std::move(args), VT::I32, false); }

    Reg op(const char* name, Reg a, Reg b) {
        Reg r = reg(VT::I32);
        Instr& i = emit(Kind::Op);
        i.dst = r;
        i.a = a;
        i.b = b;
        i.op = name;
        return r;
    }

    Reg cmp(const char* cond, Reg a, Reg b) {
        Reg r = reg(VT::I32);
        Instr& i = emit(Kind::Cmp);
        i.dst = r;
        i.a = a;
        i.b = b;
        i.op = cond;
        return r;
    }

    void jump(int l) { emit(Kind::Jump).label = l; }
    void branchZ(Reg a, int l) {
        Instr& i = emit(Kind::BranchZ);
        i.a = a;
        i.label = l;
    }
    void branchNZ(Reg a, int l) {
        Instr& i = emit(Kind::BranchNZ);
        i.a = a;
        i.label = l;
    }
    void place(int l) { emit(Kind::Label).label = l; }

    const hir::Var& var(int v) const { return p_.vars[static_cast<std::size_t>(v)]; }

    Reg load(int v) {
        if (var(v).array) unsupported("arrays");
        Reg r = reg(vt(var(v).type));
        Instr& i = emit(Kind::Load);
        i.dst = r;
        i.var = v;
        return r;
    }

    void store(int v, Reg x) {
        if (var(v).type == Type::Str) {  // assignment of strings: the runtime takes over or copies
            rtcallVoid("rt_str_assign", {load(v), x});
            return;
        }
        Instr& i = emit(Kind::Store);
        i.var = v;
        i.a = x;
    }

    // Does the program compute with strings (beyond constants printed directly)?
    bool usesStrings() const {
        for (const hir::Var& v : p_.vars)
            if (v.type == Type::Str) return true;
        for (const hir::Routine& r : p_.routines)
            if (r.result == Type::Str && r.kind != hir::Routine::Procedure) return true;
        bool found = false;
        std::function<void(const hir::Expr&)> ex = [&](const hir::Expr& e) {
            if (e.type == Type::Str) found = true;
            for (const auto& a : e.args) ex(*a);
        };
        std::function<void(const hir::Block&)> blk = [&](const hir::Block& b) {
            for (const auto& st : b) {
                for (const hir::PrintItem& it : st->items)
                    if (it.expr && it.expr->op != Op::ConstStr) ex(*it.expr);
                for (const hir::ExprPtr* e : {&st->value, &st->cond, &st->cond2, &st->start, &st->end, &st->usingFmt})
                    if (*e) ex(**e);
                for (const auto& a : st->args) ex(*a);
                for (const auto& br : st->branches) {
                    ex(*br.cond);
                    blk(br.body);
                }
                for (const auto& c : st->cases) blk(c.body);
                blk(st->body);
                blk(st->elseBody);
            }
        };
        blk(p_.main);
        for (const hir::Routine& r : p_.routines) {
            blk(r.body);
            if (r.expr) ex(*r.expr);
        }
        return found;
    }

    void releaseTemps() {
        if (strings_) rtcallVoid("rt_str_release", {base_});
    }

    // ---- routines -------------------------------------------------------------------------------
    void function(lir::Function& f, const hir::Block& body, const hir::Routine* r) {
        f_ = &f;
        routine_ = r;
        saved_.clear();
        exitLabels_.clear();
        exitLabel_ = label();
        try {
            if (strings_) base_ = rtcall("rt_str_mark", {}, VT::I32);
            if (r != nullptr) {
                f.result = vt(r->result);
                f.returnsValue = r->kind != hir::Routine::Procedure && f.result != VT::Str;
                if (f.returnsValue) result_ = reg(f.result);
                for (const hir::Param& prm : r->params) {
                    if (prm.byRef) unsupported("VAR parameters");
                    f.params.push_back(reg(vt(var(prm.var).type)));
                }
                for (std::size_t k = 0; k < r->params.size(); ++k) {
                    save(r->params[k].var);
                    store(r->params[k].var, f.params[k]);
                }
                if (r->kind == hir::Routine::DefFn) {
                    setResult(expr(*r->expr));
                } else {
                    block(body);
                    if (r->kind == hir::Routine::Function) rtcallVoid("rt_no_return", {});
                }
            } else {
                block(body);
            }
        } catch (const Unsupported& u) {
            diags_.push_back({u.line, 0, u.what + " not supported by the MC68000 backend yet"});
        }
        place(exitLabel_);
        releaseTemps();
        for (auto it = saved_.rbegin(); it != saved_.rend(); ++it) emit(Kind::Restore).var = *it;
        Instr& ret = emit(Kind::Return);
        if (f.returnsValue) ret.a = result_;
    }

    // function result: numbers in the result register, strings through rt_str_set_ret (D18)
    void setResult(Reg v) {
        if (v.type == VT::Str) rtcallVoid("rt_str_set_ret", {v});
        else copyTo(result_, v);
    }

    void save(int v) {
        for (int s : saved_)
            if (s == v) return;
        if (var(v).array) unsupported("LOCAL arrays");
        saved_.push_back(v);
        emit(Kind::Save).var = v;
    }

    void copyTo(Reg dst, Reg src) {
        // dst = src (same type): an addition of 0 would cost more; use the Op "mov"
        Instr& i = emit(Kind::Op);
        i.dst = dst;
        i.a = src;
        i.op = "mov";
    }

    // ---- statements -----------------------------------------------------------------------------
    void block(const hir::Block& b) {
        for (const auto& s : b) statement(*s);
    }

    Reg truth(const hir::Expr& e) {
        Reg v = expr(e);
        if (v.type == VT::I32) return v;
        return rtcall("fp_cmp", {v, constF(0)}, VT::I32);  // non-zero = true
    }

    void statement(const hir::Stmt& s) {
        line_ = s.line;
        emit(Kind::Line).imm = s.line;
        releaseTemps();
        switch (s.kind) {
            case SKind::Assign:
                if (s.target->op != Op::Load) unsupported("arrays");
                store(s.target->var, expr(*s.value));
                return;
            case SKind::Print: return print(s);
            case SKind::If: {
                const int end = label();
                for (const hir::Branch& br : s.branches) {
                    const int next = label();
                    branchZ(truth(*br.cond), next);
                    block(br.body);
                    jump(end);
                    place(next);
                }
                block(s.elseBody);
                place(end);
                return;
            }
            case SKind::For: return forLoop(s);
            case SKind::While: {
                const int top = label(), end = label();
                place(top);
                releaseTemps();
                branchZ(truth(*s.cond), end);
                exitLabels_.push_back(end);
                block(s.body);
                exitLabels_.pop_back();
                jump(top);
                place(end);
                return;
            }
            case SKind::Repeat: {
                const int top = label(), end = label();
                place(top);
                exitLabels_.push_back(end);
                block(s.body);
                exitLabels_.pop_back();
                releaseTemps();
                branchZ(truth(*s.cond), top);
                place(end);
                return;
            }
            case SKind::Do: {
                const int top = label(), end = label();
                place(top);
                releaseTemps();
                if (s.pre == 1) branchZ(truth(*s.cond), end);
                if (s.pre == 2) branchNZ(truth(*s.cond), end);
                exitLabels_.push_back(end);
                block(s.body);
                exitLabels_.pop_back();
                releaseTemps();
                if (s.post == 1) branchZ(truth(*s.cond2), end);
                if (s.post == 2) branchNZ(truth(*s.cond2), end);
                jump(top);
                place(end);
                return;
            }
            case SKind::ExitIf:
                branchNZ(truth(*s.cond), exitLabels_.back());
                return;
            case SKind::Select: return select(s);
            case SKind::Call: {
                const hir::Routine& r = p_.routines[static_cast<std::size_t>(s.proc)];
                Instr i;
                i.kind = Kind::Call;
                i.routine = s.proc;
                for (std::size_t k = 0; k < s.args.size(); ++k) {
                    if (r.params[k].byRef) unsupported("VAR parameters");
                    i.args.push_back(expr(*s.args[k]));
                }
                f_->code.push_back(i);
                return;
            }
            case SKind::Return:
                setResult(expr(*s.value));
                jump(exitLabel_);
                return;
            case SKind::Local:
                for (int v : s.vars) {
                    save(v);
                    emit(Kind::Clear).var = v;
                }
                return;
            case SKind::Goto: jump(gotoLabel(s.label)); return;
            case SKind::Label: place(gotoLabel(s.label)); return;
            case SKind::Read:
            case SKind::Restore: unsupported("DATA/READ");
            case SKind::OnError:
            case SKind::Resume: unsupported("ON ERROR");
            case SKind::Cmd: return command(s);
        }
    }

    std::map<std::pair<lir::Function*, int>, int> gotoLabels_;
    int gotoLabel(int hirLabel) {
        const auto key = std::make_pair(f_, hirLabel);
        const auto it = gotoLabels_.find(key);
        if (it != gotoLabels_.end()) return it->second;
        return gotoLabels_[key] = label();
    }

    void print(const hir::Stmt& s) {
        if (s.usingFmt) unsupported("PRINT USING");
        char last = 0;
        for (const hir::PrintItem& it : s.items) {
            if (it.expr) {
                if (it.kind == hir::PrintItem::Value && it.expr->op == Op::ConstStr) {
                    emit(Kind::PrintStr).text = it.expr->sval;
                } else if (it.kind == hir::PrintItem::Value) {
                    Reg v = expr(*it.expr);
                    rtcallVoid(v.type == VT::F64 ? "rt_print_flt" : v.type == VT::Str ? "rt_print_sdesc" : "rt_print_int", {v});
                } else {
                    Reg n = rtcall("fp_to_i32", {expr(*it.expr)}, VT::I32);
                    rtcallVoid(it.kind == hir::PrintItem::Tab ? "rt_print_tab" : "rt_print_spc", {n});
                }
            }
            last = it.sep;
            if (it.sep == ',') rtcallVoid("rt_print_comma", {});
            else if (it.sep == '\'') rtcallVoid("rt_print_apos", {});
        }
        if (last != ';' && last != ',') rtcallVoid("rt_print_nl", {});
    }

    void forLoop(const hir::Stmt& s) {
        // GFA semantics (docs/language.md): end and step once; check before the first pass;
        // increase (integers wrap like INC), then check the new value. The loop goes on while
        // fp_cmp(value, end) differs from `bad` (1 for a step >= 0, -1 for a negative step).
        if (s.target->op != Op::Load) unsupported("arrays");
        const int v = s.target->var;
        const Type ty = var(v).type;
        store(v, expr(*s.start));
        Reg end = expr(*s.end);
        Reg step = expr(*s.step);
        Reg dir = rtcall("fp_cmp", {step, constF(0)}, VT::I32);
        Reg down = cmp("lt", dir, constI(0));
        Reg bad = op("or", op("add", down, down), constI(1));
        const int top = label(), exit = label();
        Reg cur = load(v);
        if (cur.type == VT::I32) cur = rtcall("fp_from_i32", {cur}, VT::F64);
        branchZ(cmp("ne", rtcall("fp_cmp", {cur, end}, VT::I32), bad), exit);
        place(top);
        exitLabels_.push_back(exit);
        block(s.body);
        exitLabels_.pop_back();
        Reg now = load(v);
        if (now.type == VT::I32) now = rtcall("fp_from_i32", {now}, VT::F64);
        Reg next = rtcall("fp_add", {now, step}, VT::F64);
        if (ty == Type::Float) store(v, next);
        else store(v, rtcall(ty == Type::Word ? "rt_for_wrap_word" : ty == Type::Byte ? "rt_for_wrap_byte" : ty == Type::Bool ? "rt_for_wrap_bool" : "rt_for_wrap_int", {next}, VT::I32));
        branchNZ(cmp("ne", rtcall("fp_cmp", {next, end}, VT::I32), bad), top);
        place(exit);
    }

    void select(const hir::Stmt& s) {
        Reg v = expr(*s.value);
        const int end = label();
        std::vector<int> bodies;
        for (std::size_t k = 0; k < s.cases.size(); ++k) bodies.push_back(label());
        const int dflt = label();
        for (std::size_t k = 0; k < s.cases.size(); ++k) {
            for (const hir::CaseItem& it : s.cases[k].items) {
                if (!it.range) {
                    branchNZ(cmp("eq", v, expr(*it.lo)), bodies[k]);
                    continue;
                }
                const int skip = label();
                if (it.lo) branchZ(cmp("ge", v, expr(*it.lo)), skip);
                if (it.hi) branchZ(cmp("le", v, expr(*it.hi)), skip);
                jump(bodies[k]);
                place(skip);
            }
        }
        jump(dflt);
        for (std::size_t k = 0; k < s.cases.size(); ++k) {
            place(bodies[k]);
            block(s.cases[k].body);
            if (!s.cases[k].cont) jump(end);  // CONT: on into the next block (or DEFAULT)
        }
        place(dflt);
        block(s.elseBody);
        place(end);
    }

    void command(const hir::Stmt& s) {
        const std::string& n = s.name;
        if (n == "INC" || n == "DEC" || n == "ADD" || n == "SUB" || n == "MUL" || n == "DIV") {
            if (s.targets[0]->op != Op::Load) unsupported("arrays");
            const int v = s.targets[0]->var;
            const Type ty = var(v).type;
            Reg x = load(v);
            if (ty == Type::Float) {
                Reg y = n == "INC" || n == "DEC" ? constF(F64_ONE) : expr(*s.args[0]);
                const char* fn = n == "INC" || n == "ADD" ? "fp_add" : n == "DEC" || n == "SUB" ? "fp_sub" : n == "MUL" ? "fp_mul" : "fp_div";
                store(v, rtcall(fn, {x, y}, VT::F64));
                return;
            }
            Reg y = n == "INC" || n == "DEC" ? constI(1) : rtcall("fp_to_i32", {expr(*s.args[0])}, VT::I32);
            Reg r;
            if (n == "INC" || n == "ADD") r = op("add", x, y);
            else if (n == "DEC" || n == "SUB") r = op("sub", x, y);
            else if (n == "MUL") r = rtcall("rt_imul", {x, y}, VT::I32);
            else r = rtcall("rt_idiv", {x, y}, VT::I32);
            if (ty == Type::Word) r = rtcall("rt_wrap_word", {r}, VT::I32);
            else if (ty == Type::Byte) r = rtcall("rt_wrap_byte", {r}, VT::I32);
            else if (ty == Type::Bool) r = cmp("ne", r, constI(0));
            store(v, r);
            return;
        }
        if (n == "END" || n == "STOP" || n == "QUIT" || n == "SYSTEM" || n == "EDIT" || n == "NEW") {
            rtcallVoid("rt_end", {});
            return;
        }
        if (n == "DEFNUM") {
            rtcallVoid("rt_set_defnum", {rtcall("fp_to_i32", {expr(*s.args[0])}, VT::I32)});
            return;
        }
        if (n == "RANDOMIZE") {
            Reg seed = s.args.empty() ? constI(0) : rtcall("fp_to_i32", {expr(*s.args[0])}, VT::I32);
            rtcallVoid("rt_randomize", {seed});
            return;
        }
        if (n == "ERROR") {
            rtcallVoid("rt_error", {rtcall("fp_to_i32", {expr(*s.args[0])}, VT::I32)});
            return;
        }
        if (n == "VOID") {
            expr(*s.args[0]);
            return;
        }
        if (n == "CLS" || n == "PAUSE" || n == "DELAY" || n == "TRON" || n == "TROFF") {
            for (const auto& a : s.args) expr(*a);
            return;
        }
        unsupported(n);
    }

    // ---- expressions ----------------------------------------------------------------------------
    Reg expr(const hir::Expr& e) {
        switch (e.op) {
            case Op::ConstInt: return constI(e.ival);
            case Op::ConstFloat: return constF(e.fval);
            case Op::ConstStr: {
                Reg r = reg(VT::Str);
                Instr& i = emit(Kind::ConstS);
                i.dst = r;
                i.text = e.sval;
                return r;
            }
            case Op::Load: return load(e.var);
            case Op::LoadElem: unsupported("arrays");
            case Op::Conv: return conv(e);
            case Op::ToInt: return rtcall("fp_to_i32", {expr(*e.args[0])}, VT::I32);
            case Op::Neg: return rtcall("fp_neg", {expr(*e.args[0])}, VT::F64);
            case Op::Add: return fbin("fp_add", e);
            case Op::Sub: return fbin("fp_sub", e);
            case Op::Mul: return fbin("fp_mul", e);
            case Op::Div: return fbin("fp_div", e);
            case Op::Pow: return fbin("rt_power", e);
            case Op::IDiv: return ibinCall("rt_idiv", e);
            case Op::IMod: return ibinCall("rt_imod", e);
            case Op::Not: return op("not", expr(*e.args[0]), Reg());
            case Op::And: return ibin("and", e);
            case Op::Or: return ibin("or", e);
            case Op::Xor: return ibin("xor", e);
            case Op::Imp: {
                Reg a = expr(*e.args[0]);
                Reg b = expr(*e.args[1]);
                return op("or", op("not", a, Reg()), b);
            }
            case Op::Eqv: {
                Reg a = expr(*e.args[0]);
                Reg b = expr(*e.args[1]);
                return op("not", op("xor", a, b), Reg());
            }
            case Op::Eq: return fcmp("eq", e);
            case Op::Ne: return fcmp("ne", e);
            case Op::Lt: return fcmp("lt", e);
            case Op::Gt: return fcmp("gt", e);
            case Op::Le: return fcmp("le", e);
            case Op::Ge: return fcmp("ge", e);
            case Op::ApproxEq: {
                Reg a = expr(*e.args[0]);
                Reg b = expr(*e.args[1]);
                return cmp("ne", rtcall("fp_approx_eq", {a, b}, VT::I32), constI(0));
            }
            case Op::Concat: {
                Reg a = expr(*e.args[0]);
                Reg b = expr(*e.args[1]);
                return rtcall("rt_str_concat", {a, b}, VT::Str);
            }
            case Op::Builtin: return builtin(e);
            case Op::CallFunc: {
                const hir::Routine& r = p_.routines[static_cast<std::size_t>(e.func)];
                Instr i;
                i.kind = Kind::Call;
                i.routine = e.func;
                for (std::size_t k = 0; k < e.args.size(); ++k) {
                    if (r.params[k].byRef) unsupported("VAR parameters");
                    i.args.push_back(expr(*e.args[k]));
                }
                i.dst = reg(vt(r.result));
                f_->code.push_back(i);
                return i.dst;
            }
        }
        unsupported("expression");
    }

    Reg fbin(const char* fn, const hir::Expr& e) {
        Reg a = expr(*e.args[0]);
        Reg b = expr(*e.args[1]);
        return rtcall(fn, {a, b}, VT::F64);
    }

    Reg ibin(const char* name, const hir::Expr& e) {
        Reg a = expr(*e.args[0]);
        Reg b = expr(*e.args[1]);
        return op(name, a, b);
    }

    Reg ibinCall(const char* fn, const hir::Expr& e) {
        Reg a = expr(*e.args[0]);
        Reg b = expr(*e.args[1]);
        return rtcall(fn, {a, b}, VT::I32);
    }

    Reg fcmp(const char* cond, const hir::Expr& e) {
        Reg a = expr(*e.args[0]);
        Reg b = expr(*e.args[1]);
        return cmp(cond, rtcall(a.type == VT::Str ? "rt_str_cmp" : "fp_cmp", {a, b}, VT::I32), constI(0));
    }

    Reg conv(const hir::Expr& e) {
        const hir::Expr& x = *e.args[0];
        Reg v = expr(x);
        const bool fromF = v.type == VT::F64;
        switch (e.type) {
            case Type::Float: return fromF ? v : rtcall("fp_from_i32", {v}, VT::F64);
            case Type::Int: return fromF ? rtcall("fp_to_i32", {v}, VT::I32) : v;
            case Type::Word: return rtcall(fromF ? "fp_to_i16" : "rt_conv_word", {v}, VT::I32);
            case Type::Byte: return rtcall(fromF ? "fp_to_u8" : "rt_conv_byte", {v}, VT::I32);
            case Type::Bool:
                if (fromF) v = rtcall("fp_cmp", {v, constF(0)}, VT::I32);
                return cmp("ne", v, constI(0));
            case Type::Str: return v;
        }
        unsupported("conversion");
    }

    Reg arg(const hir::Expr& e, std::size_t k) { return expr(*e.args[k]); }

    Reg builtin(const hir::Expr& e) {
        const std::string& n = e.sval;
        static const std::map<std::string, std::string> f2f = {
            {"INT", "fp_int"}, {"TRUNC", "fp_trunc"}, {"FIX", "fp_trunc"}, {"FRAC", "fp_frac"}, {"SQR", "fp_sqrt"}, {"ABS", "fp_abs"}};
        if (const auto it = f2f.find(n); it != f2f.end()) return rtcall(it->second, {arg(e, 0)}, VT::F64);
        if (n == "CFLOAT") return arg(e, 0);
        if (n == "SGN") return rtcall("fp_cmp", {arg(e, 0), constF(0)}, VT::I32);
        if (n == "CINT") return rtcall("fp_to_i32", {rtcall("fp_cint", {arg(e, 0)}, VT::F64)}, VT::I32);
        if (n == "ROUND") {
            Reg x = arg(e, 0);
            Reg k = e.args.size() > 1 ? rtcall("fp_to_i32", {arg(e, 1)}, VT::I32) : constI(0);
            return rtcall("fp_round", {x, k}, VT::F64);
        }
        if (n == "ODD" || n == "EVEN") {
            Reg i = rtcall("fp_to_i32", {arg(e, 0)}, VT::I32);
            Reg bit = op("and", i, constI(1));
            return cmp(n == "ODD" ? "ne" : "eq", bit, constI(0));
        }
        if (n == "TRUE") return constI(-1);
        if (n == "FALSE") return constI(0);
        if (n == "PI") return constF(0x400921FB54442D18u);
        if (n == "RND") {
            if (!e.args.empty()) arg(e, 0);
            return rtcall("rt_rnd", {}, VT::F64);
        }
        if (n == "RANDOM") {
            Reg x = arg(e, 0);
            Reg r = rtcall("rt_rnd", {}, VT::F64);
            return rtcall("fp_to_i32", {rtcall("fp_int", {rtcall("fp_mul", {r, x}, VT::F64)}, VT::F64)}, VT::I32);
        }
        if (n == "RAND") return rtcall("rt_rand", {rtcall("fp_to_i32", {arg(e, 0)}, VT::I32)}, VT::I32);
        return stringBuiltin(e);
    }

    Reg toI(Reg v) { return v.type == VT::F64 ? rtcall("fp_to_i32", {v}, VT::I32) : v; }

    // MIN/MAX: the first value, replaced by every later one that is smaller/greater
    Reg minMax(const hir::Expr& e, bool isMax) {
        Reg best = arg(e, 0);
        Reg result = reg(best.type);
        copyTo(result, best);
        for (std::size_t k = 1; k < e.args.size(); ++k) {
            Reg v = arg(e, k);
            Reg c = rtcall(v.type == VT::Str ? "rt_str_cmp" : "fp_cmp", {v, result}, VT::I32);
            const int skip = label();
            branchZ(cmp(isMax ? "gt" : "lt", c, constI(0)), skip);
            copyTo(result, v);
            place(skip);
        }
        return result;
    }

    Reg stringBuiltin(const hir::Expr& e) {
        const std::string& n = e.sval;
        const std::size_t argc = e.args.size();
        if (n == "MIN" || n == "MAX") return minMax(e, n == "MAX");
        if (n == "LEN") return rtcall("rt_str_len", {arg(e, 0)}, VT::I32);
        if (n == "ASC") return rtcall("rt_str_asc", {arg(e, 0)}, VT::I32);
        if (n == "#PACK") return rtcall("rt_str_pack", {arg(e, 0)}, VT::I32);
        if (n == "CHR$") return rtcall("rt_str_chr", {toI(arg(e, 0))}, VT::Str);
        if (n == "SPACE$") return rtcall("rt_str_space", {toI(arg(e, 0))}, VT::Str);
        if (n == "LEFT$" || n == "RIGHT$") {
            Reg s = arg(e, 0);
            Reg k = argc > 1 ? toI(arg(e, 1)) : constI(1);
            return rtcall(n == "LEFT$" ? "rt_str_left" : "rt_str_right", {s, k}, VT::Str);
        }
        if (n == "MID$") {
            Reg s = arg(e, 0);
            Reg from = toI(arg(e, 1));
            Reg len = argc > 2 ? toI(arg(e, 2)) : constI(32767);
            return rtcall("rt_str_mid", {s, from, len}, VT::Str);
        }
        if (n == "STRING$") {
            Reg k = toI(arg(e, 0));
            Reg u = arg(e, 1);
            if (u.type != VT::Str) u = rtcall("rt_str_chr", {toI(u)}, VT::Str);
            return rtcall("rt_str_string", {k, u}, VT::Str);
        }
        if (n == "INSTR") {
            if (e.args[0]->type == Type::Str) {
                Reg a = arg(e, 0);
                Reg b = arg(e, 1);
                Reg from = argc > 2 ? toI(arg(e, 2)) : constI(1);
                return rtcall("rt_str_instr", {a, b, from}, VT::I32);
            }
            Reg from = toI(arg(e, 0));
            Reg a = arg(e, 1);
            Reg b = arg(e, 2);
            return rtcall("rt_str_instr", {a, b, from}, VT::I32);
        }
        if (n == "UPPER$") return rtcall("rt_str_upper", {arg(e, 0)}, VT::Str);
        if (n == "TRIM$") return rtcall("rt_str_trim", {arg(e, 0)}, VT::Str);
        if (n == "STR$" && argc == 1) return rtcall("rt_str_str", {arg(e, 0)}, VT::Str);
        if (n == "HEX$" || n == "BIN$" || n == "OCT$") {
            Reg v = toI(arg(e, 0));
            Reg base = constI(n == "HEX$" ? 16 : n == "BIN$" ? 2 : 8);
            Reg w = argc > 1 ? toI(arg(e, 1)) : constI(0);
            return rtcall("rt_str_radix", {v, base, w}, VT::Str);
        }
        if ((n == "PRED" || n == "SUCC") && e.type == Type::Str) {
            Reg c = rtcall("rt_str_asc", {arg(e, 0)}, VT::I32);
            return rtcall("rt_str_chr", {op(n == "PRED" ? "sub" : "add", c, constI(1))}, VT::Str);
        }
        if (n == "PRED" || n == "SUCC") return op(n == "PRED" ? "sub" : "add", toI(arg(e, 0)), constI(1));
        unsupported(n);
    }
};

}  // namespace

lir::Program lower(const hir::Program& program, Diagnostics& diags) {
    Lowering l(program, diags);
    return l.run();
}

}  // namespace basic
