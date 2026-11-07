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
        scanErrors();
        lir::Program out;
        out.onError = onError_;
        out.hir = &p_;
        out.indirect.assign(p_.vars.size(), false);
        for (const hir::Routine& r : p_.routines)
            for (const hir::Param& prm : r.params)
                if (prm.byRef) out.indirect[static_cast<std::size_t>(prm.var)] = true;
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
    bool onError_ = false;          // the program uses ON ERROR: statement records (docs/runtime.md)
    std::vector<bool> resumeTarget_;  // per HIR label: target of a RESUME label
    std::vector<std::pair<int, int>> stmtLabels_;  // (start, after) of the enclosing statements
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
        if (var(v).array) unsupported("whole arrays here");
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

    // address of the storage of a variable (scalar or the slot of an array)
    Reg addrOf(int v) {
        Reg r = reg(VT::Addr);
        Instr& i = emit(Kind::Addr);
        i.dst = r;
        i.var = v;
        return r;
    }

    // element size in an array (bytes)
    static int elemSize(Type t) {
        switch (t) {
            case Type::Float:
            case Type::Str: return 8;
            case Type::Int: return 4;
            case Type::Word: return 2;
            default: return 1;
        }
    }

    // address of an array element (checks in rt_arr_elem)
    Reg elemAddr(const hir::Expr& e) {
        if (e.args.size() > 3) unsupported("arrays with more than 3 dimensions");
        std::vector<Reg> ix;
        for (const auto& a : e.args) ix.push_back(toI(expr(*a)));
        std::vector<Reg> args{addrOf(e.var), constI(static_cast<std::int32_t>(ix.size()))};
        args.insert(args.end(), ix.begin(), ix.end());
        return rtcall("rt_arr_elem", args, VT::Addr);
    }

    Reg loadElem(const hir::Expr& e) { return loadAtAddr(elemAddr(e), var(e.var).type); }

    Reg loadAtAddr(Reg a, Type t) {
        if (t == Type::Str) return strAt(a);
        Reg r = reg(vt(t));
        Instr& i = emit(Kind::LoadAt);
        i.dst = r;
        i.a = a;
        i.mem = t;
        return r;
    }

    Reg strAt(Reg a) {
        Reg r = reg(VT::Str);
        copyTo(r, a);
        return r;
    }

    Reg loadTarget(const hir::Expr& t) { return t.op == Op::Load ? load(t.var) : loadElem(t); }

    // store into a variable or an element; `addr` may be given when the element address is known
    void storeTarget(const hir::Expr& t, Reg x, Reg addr = Reg()) {
        if (t.op == Op::Load) return store(t.var, x);
        if (addr.id < 0) addr = elemAddr(t);
        const Type ty = var(t.var).type;
        if (ty == Type::Str) {
            Reg d = reg(VT::Str);
            copyTo(d, addr);
            rtcallVoid("rt_str_assign", {d, x});
            return;
        }
        Instr& i = emit(Kind::StoreAt);
        i.a = addr;
        i.b = x;
        i.mem = ty;
    }

    void scanErrors() {
        resumeTarget_.assign(p_.labels.size(), false);
        std::function<void(const hir::Block&)> blk = [&](const hir::Block& b) {
            for (const auto& st : b) {
                if (st->kind == SKind::OnError) onError_ = true;
                if (st->kind == SKind::Resume && st->mode == 2) resumeTarget_[static_cast<std::size_t>(st->label)] = true;
                for (const auto& br : st->branches) blk(br.body);
                for (const auto& c : st->cases) blk(c.body);
                blk(st->body);
                blk(st->elseBody);
            }
        };
        blk(p_.main);
        for (const hir::Routine& r : p_.routines) blk(r.body);
    }

    // statement record again (with ON ERROR): the condition or the step of a loop belongs to the
    // loop statement, not to the last statement of its body
    void reenter() {
        if (!onError_ || stmtLabels_.empty()) return;
        Instr& i = emit(Kind::Line);
        i.imm = line_;
        i.label = stmtLabels_.back().first;
        i.label2 = stmtLabels_.back().second;
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
                f.deffn = r->kind == hir::Routine::DefFn;
                f.result = vt(r->result);
                f.returnsValue = r->kind != hir::Routine::Procedure && f.result != VT::Str;
                if (f.returnsValue) result_ = reg(f.result);
                for (const hir::Param& prm : r->params) f.params.push_back(reg(prm.byRef ? VT::Addr : vt(var(prm.var).type)));
                for (std::size_t k = 0; k < r->params.size(); ++k) {
                    const int v = r->params[k].var;
                    if (r->params[k].byRef) {  // VAR: the variable refers to the caller's storage (probe10)
                        for (int sv : saved_)
                            if (sv == v) unsupported("the same VAR parameter twice");
                        saved_.push_back(v);
                        emit(Kind::Save).var = v;
                        Instr& b = emit(Kind::Bind);
                        b.var = v;
                        b.a = f.params[k];
                        continue;
                    }
                    if (var(v).array) unsupported("array parameters without VAR");
                    save(v);
                    store(v, f.params[k]);
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

    // arguments of a call: values, or for VAR the address of the variable (Load of a scalar or a
    // whole array; an element is passed by value as the original does with VAR a(i)? not allowed)
    std::vector<Reg> callArgs(const hir::Routine& r, const std::vector<hir::ExprPtr>& args) {
        std::vector<Reg> out;
        for (std::size_t k = 0; k < args.size(); ++k) {
            const hir::Expr& a = *args[k];
            if (r.params[k].byRef) {
                if (a.op != Op::Load) unsupported("VAR with an array element");
                out.push_back(addrOf(a.var));
            } else {
                out.push_back(expr(a));
            }
        }
        return out;
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
        if (!onError_) {
            emit(Kind::Line).imm = s.line;
            releaseTemps();
            statementBody(s);
            return;
        }
        const int start = label(), after = label();
        place(start);
        Instr& i = emit(Kind::Line);
        i.imm = s.line;
        i.label = start;
        i.label2 = after;
        releaseTemps();
        stmtLabels_.emplace_back(start, after);
        statementBody(s);
        stmtLabels_.pop_back();
        place(after);
    }

    void statementBody(const hir::Stmt& s) {
        switch (s.kind) {
            case SKind::Assign: {
                Reg x = expr(*s.value);
                storeTarget(*s.target, x);
                return;
            }
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
                reenter();
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
                reenter();
                releaseTemps();
                branchZ(truth(*s.cond), top);
                place(end);
                return;
            }
            case SKind::Do: {
                const int top = label(), end = label();
                place(top);
                reenter();
                releaseTemps();
                if (s.pre == 1) branchZ(truth(*s.cond), end);
                if (s.pre == 2) branchNZ(truth(*s.cond), end);
                exitLabels_.push_back(end);
                block(s.body);
                exitLabels_.pop_back();
                reenter();
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
                i.args = callArgs(r, s.args);
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
            case SKind::Label:
                place(gotoLabel(s.label));
                if (resumeTarget_[static_cast<std::size_t>(s.label)]) f_->resumeLabels.emplace_back(s.label, gotoLabel(s.label));
                return;
            case SKind::Read:
                for (const auto& t : s.targets) {
                    const Type ty = var(t->var).type;
                    if (ty == Type::Str) storeTarget(*t, rtcall("rt_data_str", {}, VT::Str));
                    else storeTarget(*t, convF(rtcall("rt_data_num", {}, VT::F64), ty));
                }
                return;
            case SKind::Restore:
                rtcallVoid("rt_data_restore", {constI(s.label < 0 ? 0 : p_.labels[static_cast<std::size_t>(s.label)].data)});
                return;
            case SKind::OnError: {
                Reg a = reg(VT::Addr);
                if (s.proc >= 0) {
                    Instr& i = emit(Kind::RoutineAddr);
                    i.dst = a;
                    i.routine = s.proc;
                } else {
                    Instr& i = emit(Kind::ConstI);
                    i.dst = a;
                }
                rtcallVoid("rt_on_error", {a});
                return;
            }
            case SKind::Resume:
                rtcallVoid("rt_resume", {constI(s.mode), constI(s.mode == 2 ? s.label : 0)});
                return;
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

    // PRINT USING: every item is a value (as in the interpreter), numbers as floats
    void printUsing(const hir::Stmt& s) {
        rtcallVoid("rt_using_begin", {expr(*s.usingFmt)});
        char last = 0;
        for (const hir::PrintItem& it : s.items) {
            if (it.expr) {
                Reg v = expr(*it.expr);
                if (v.type == VT::Str) rtcallVoid("rt_using_str", {v});
                else rtcallVoid("rt_using_num", {v.type == VT::F64 ? v : rtcall("fp_from_i32", {v}, VT::F64)});
            }
            last = it.sep;
        }
        rtcallVoid("rt_using_end", {});
        if (last != ';' && last != ',') rtcallVoid("rt_print_nl", {});
    }

    void print(const hir::Stmt& s) {
        if (s.usingFmt) return printUsing(s);
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
        reenter();
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
            const hir::Expr& t = *s.targets[0];
            const int v = t.var;
            const Type ty = var(v).type;
            Reg addr;
            if (t.op == Op::LoadElem) addr = elemAddr(t);
            auto store = [&](int, Reg r) { storeTarget(t, r, addr); };
            Reg x = t.op == Op::Load ? load(v) : loadAtAddr(addr, ty);
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
        if (n == "DIM") {
            for (const auto& t : s.targets) {
                if (t->args.size() > 3) unsupported("arrays with more than 3 dimensions");
                std::vector<Reg> dims;
                for (const auto& d : t->args) dims.push_back(toI(expr(*d)));
                const int code = elemSize(var(t->var).type) << 8 | static_cast<int>(dims.size());
                std::vector<Reg> args{addrOf(t->var), constI(code)};
                args.insert(args.end(), dims.begin(), dims.end());
                rtcallVoid("rt_arr_dim", args);
            }
            return;
        }
        if (n == "ERASE") {
            for (const auto& t : s.targets)
                rtcallVoid("rt_arr_erase", {addrOf(t->var), constI(var(t->var).type == Type::Str ? 1 : 0)});
            return;
        }
        if (n == "CLR") {
            for (const auto& t : s.targets) {
                const Type ty = var(t->var).type;
                if (ty == Type::Str) {
                    Reg e = reg(VT::Str);
                    emit(Kind::ConstS).dst = e;
                    store(t->var, e);
                } else {
                    store(t->var, ty == Type::Float ? constF(0) : constI(0));
                }
            }
            return;
        }
        if (n == "OPTION BASE") {
            rtcallVoid("rt_arr_set_base", {toI(expr(*s.args[0]))});
            return;
        }
        if (n == "SWAP") {
            const hir::Expr& a = *s.targets[0];
            const hir::Expr& b = *s.targets[1];
            const Type ty = var(a.var).type;
            if (var(a.var).array && a.op == Op::Load) {  // whole arrays: exchange the slots
                Reg pa = addrOf(a.var);
                rtcallVoid("rt_swap8", {pa, addrOf(b.var)});
                return;
            }
            if (ty == Type::Str) {
                Reg pa = a.op == Op::Load ? load(a.var) : strAt(elemAddr(a));
                Reg pb = b.op == Op::Load ? load(b.var) : strAt(elemAddr(b));
                rtcallVoid("rt_str_swap", {pa, pb});
                return;
            }
            Reg aa, ab;
            if (a.op == Op::LoadElem) aa = elemAddr(a);
            if (b.op == Op::LoadElem) ab = elemAddr(b);
            Reg va = a.op == Op::Load ? load(a.var) : loadAtAddr(aa, ty);
            Reg vb = b.op == Op::Load ? load(b.var) : loadAtAddr(ab, ty);
            storeTarget(a, vb, aa);
            storeTarget(b, va, ab);
            return;
        }
        if (n == "END" || n == "STOP" || n == "QUIT" || n == "SYSTEM" || n == "EDIT" || n == "NEW") {
            rtcallVoid("rt_end", {});
            return;
        }
        if (n == "MID$=") {
            const hir::Expr& t = *s.targets[0];
            Reg target = t.op == Op::Load ? load(t.var) : strAt(elemAddr(t));
            Reg from = toI(expr(*s.args[0]));
            Reg len = s.args.size() > 1 ? toI(expr(*s.args[1])) : constI(32767);
            Reg b = expr(*s.value);
            rtcallVoid("rt_str_midset", {target, b, from, len});
            return;
        }
        if (n == "LSET" || n == "RSET") {
            const hir::Expr& t = *s.targets[0];
            Reg target = t.op == Op::Load ? load(t.var) : strAt(elemAddr(t));
            Reg b = expr(*s.value);
            rtcallVoid(n == "LSET" ? "rt_str_lset" : "rt_str_rset", {target, b});
            return;
        }
        if (n == "MODE") {
            rtcallVoid("rt_set_mode", {toI(expr(*s.args[0]))});
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
            case Op::LoadElem: return loadElem(e);
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
                i.args = callArgs(r, e.args);
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

    Reg conv(const hir::Expr& e) { return convF(expr(*e.args[0]), e.type); }

    // conversion with the semantics of an assignment (truncation, range errors 2/3/4)
    Reg convF(Reg v, Type to) {
        const bool fromF = v.type == VT::F64;
        switch (to) {
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
        if (Reg r; intBuiltin(e, r)) return r;
        return stringBuiltin(e);
    }

    // integer and bit functions: arguments as 32-bit integers (error 2 out of range)
    bool intBuiltin(const hir::Expr& e, Reg& out) {
        const std::string& n = e.sval;
        auto ia = [&](std::size_t k) { return toI(arg(e, k)); };
        static const std::map<std::string, std::string> two = {
            {"MUL", "rt_imul"}, {"DIV", "rt_idiv"}, {"MOD", "rt_imod"},   {"BTST", "rt_btst"},
            {"BCLR", "rt_bclr"}, {"BSET", "rt_bset"}, {"BCHG", "rt_bchg"}};
        static const std::map<std::string, std::string> one = {
            {"SWAP", "rt_swap"}, {"WORD", "rt_word"}, {"CARD", "rt_card"}, {"BYTE", "rt_byte"}};
        static const std::map<std::string, std::string> ops = {
            {"ADD", "add"}, {"SUB", "sub"}, {"AND", "and"}, {"OR", "or"}, {"XOR", "xor"}};
        if (e.type == Type::Str) return false;
        if (const auto it = ops.find(n); it != ops.end() && e.args.size() == 2) {
            Reg a = ia(0);
            out = op(it->second.c_str(), a, ia(1));
            return true;
        }
        if (const auto it = two.find(n); it != two.end() && e.args.size() == 2) {
            Reg a = ia(0);
            out = rtcall(it->second, {a, ia(1)}, VT::I32);
            return true;
        }
        if (const auto it = one.find(n); it != one.end()) {
            out = rtcall(it->second, {ia(0)}, VT::I32);
            return true;
        }
        if (n.size() >= 3 && (n.rfind("SHL", 0) == 0 || n.rfind("SHR", 0) == 0 || n.rfind("ROL", 0) == 0 || n.rfind("ROR", 0) == 0)) {
            const int width = n.size() == 3 ? 32 : n[3] == '&' ? 16 : 8;
            const int kind = (n[0] == 'R' ? 2 : 0) + (n[2] == 'R' ? 1 : 0);
            Reg x = ia(0);
            Reg c = ia(1);
            out = rtcall("rt_shift", {x, c, constI(width), constI(kind)}, VT::I32);
            return true;
        }
        return false;
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
        if (n == "ERR") return rtcall("rt_err_get", {}, VT::I32);
        if (n == "ERR$") return rtcall("rt_err_text", {toI(arg(e, 0))}, VT::Str);
        if (n == "VAL") return rtcall("rt_str_val", {arg(e, 0)}, VT::F64);
        if (n == "VAL?") return rtcall("rt_str_valq", {arg(e, 0)}, VT::I32);
        if (n == "DIM?") return rtcall("rt_arr_count", {addrOf(e.args[0]->var)}, VT::I32);
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
        if (n == "STR$" && argc == 2) {
            Reg x = arg(e, 0);
            return rtcall("rt_str_n", {x, toI(arg(e, 1))}, VT::Str);
        }
        if (n == "STR$") {
            Reg x = arg(e, 0);
            Reg y = toI(arg(e, 1));
            return rtcall("rt_str_fmt", {x, y, toI(arg(e, 2))}, VT::Str);
        }
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
