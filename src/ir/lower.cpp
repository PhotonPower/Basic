#include "lower.hpp"

#include <cmath>
#include <cstring>
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

    // record of a header line (WHILE, IF, ELSE IF, DO WHILE/UNTIL, FOR) with ON ERROR: RESUME repeats
    // the line at `start`, RESUME NEXT goes on at `after`, the first line of the body (D19, probe11)
    void headerRecord(int start, int after) {
        if (!onError_) return;
        Instr& i = emit(Kind::Line);
        i.imm = line_;
        i.label = start;
        i.label2 = after;
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
                    const int next = label(), head = label(), body = label();
                    place(head);
                    headerRecord(head, body);
                    branchZ(truth(*br.cond), next);
                    place(body);
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
                const int top = label(), end = label(), body = label();
                place(top);
                headerRecord(top, body);
                releaseTemps();
                branchZ(truth(*s.cond), end);
                place(body);
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
                const int top = label(), end = label(), body = label();
                place(top);
                if (s.pre != 0) headerRecord(top, body);
                else reenter();
                releaseTemps();
                if (s.pre == 1) branchZ(truth(*s.cond), end);
                if (s.pre == 2) branchNZ(truth(*s.cond), end);
                place(body);
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
        if (s.channel) {  // PRINT #n: the output goes to the file for this statement (rt_file)
            rtcallVoid("rt_file_select", {toI(expr(*s.channel))});
            printItems(s);
            rtcallVoid("rt_file_deselect", {});
            return;
        }
        printItems(s);
    }

    void printItems(const hir::Stmt& s) {
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
        if (ty == Type::Int && !onError_ && isIntLeaf(*s.end) && isIntLeaf(*s.step) && s.step->op != Op::Conv)
            return intForLoop(s, v);
        // with ON ERROR: an error in the FOR line and RESUME NEXT run the body once, NEXT increases
        // the variable (by 1 if the step is unknown) and leaves the loop (D19, probe11 T4)
        Reg failed, step;
        int failLabel = -1;
        if (onError_) {
            failed = constI(0);
            step = reg(VT::F64);
            copyTo(step, constF(F64_ONE));
            failLabel = label();
            headerRecord(stmtLabels_.back().first, failLabel);
        }
        store(v, expr(*s.start));
        Reg end = expr(*s.end);
        if (onError_) copyTo(step, expr(*s.step));
        else step = expr(*s.step);
        Reg dir = rtcall("fp_cmp", {step, constF(0)}, VT::I32);
        Reg down = cmp("lt", dir, constI(0));
        Reg bad = op("or", op("add", down, down), constI(1));
        const int top = label(), exit = label();
        Reg cur = load(v);
        if (cur.type == VT::I32) cur = rtcall("fp_from_i32", {cur}, VT::F64);
        branchZ(cmp("ne", rtcall("fp_cmp", {cur, end}, VT::I32), bad), exit);
        if (onError_) {
            jump(top);
            place(failLabel);
            copyTo(failed, constI(-1));
        }
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
        if (onError_) branchNZ(failed, exit);
        branchNZ(cmp("ne", rtcall("fp_cmp", {next, end}, VT::I32), bad), top);
        place(exit);
    }

    // FOR i% with an integer end and a constant integer step: the value plus the step exactly in 32
    // bits; an overflow means the exact value lies beyond the end (the loop ends, the variable keeps
    // the wrapped value, as rt_for_wrap_int). The same results as the float loop above.
    void intForLoop(const hir::Stmt& s, int v) {
        double d = 0;
        if (s.step->op == Op::ConstFloat) std::memcpy(&d, &s.step->fval, sizeof d);
        else d = s.step->ival;
        const bool down = d < 0;
        store(v, expr(*s.start));
        Reg end = intLeaf(*s.end);
        Reg step = intLeaf(*s.step);
        const int top = label(), exit = label();
        branchZ(cmp(down ? "ge" : "le", load(v), end), exit);
        place(top);
        exitLabels_.push_back(exit);
        block(s.body);
        exitLabels_.pop_back();
        reenter();
        Reg now = load(v);
        Reg next = op("add", now, step);
        store(v, next);
        branchNZ(cmp(down ? "gt" : "lt", next, now), exit);  // overflow: beyond the end
        branchNZ(cmp(down ? "ge" : "le", next, end), top);
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

    // ---- MAT (D23): views of runtime/m68k/rt_mat, then the operation ------------------------------
    void matView(const hir::Expr& t, int k, bool column = false) {
        Reg a = addrOf(t.var);
        rtcallVoid("rt_mat_view", {a, constI(k), constI(column ? 1 : 0)});
    }

    // offsets a(i[,j]) of view k: DET/RANG (cpy false) or CPY/XCPY rules
    void matAt(const hir::Expr& t, int k, bool cpy, bool transpose) {
        if (t.args.empty()) return;
        Reg i = toI(expr(*t.args[0]));
        Reg j = t.args.size() > 1 ? toI(expr(*t.args[1])) : constI(0);
        const int flags = k | static_cast<int>(t.args.size()) << 8 | (cpy ? 1 << 10 : 0) | (transpose ? 1 << 11 : 0);
        rtcallVoid("rt_mat_at", {constI(flags), i, j});
    }

    void matScalarResult(const hir::Expr& target, Reg x) { storeTarget(target, convF(x, var(target.var).type)); }

    void mat(const hir::Stmt& s) {
        const std::string w = s.name.substr(4);
        const auto& T = s.targets;
        auto arg = [&](std::size_t k) { return expr(*s.args[k]); };
        if (w == "BASE") {
            rtcallVoid("rt_mat_set_base", {toI(arg(0))});
        } else if (w == "CLR" || w == "SET") {
            Reg a = addrOf(T[0]->var);
            rtcallVoid("rt_mat_fill", {a, w == "SET" ? arg(0) : constF(0)});
        } else if (w == "ONE" || w == "READ") {
            matView(*T[0], 0);
            rtcallVoid(w == "ONE" ? "rt_mat_one" : "rt_mat_read", {});
        } else if (w == "PRINT") {
            if (s.channel) rtcallVoid("rt_file_select", {toI(expr(*s.channel))});
            matView(*T[0], 0);
            if (s.args.size() == 2) {
                Reg g = toI(arg(0));
                rtcallVoid("rt_mat_print", {constI(1), g, toI(arg(1))});
            } else {
                rtcallVoid("rt_mat_print", {constI(0), constI(0), constI(0)});
            }
            if (s.channel) rtcallVoid("rt_file_deselect", {});
        } else if (w == "CPY" || w == "XCPY") {
            const bool tr = w == "XCPY";
            matView(*T[0], 0, !tr);
            matView(*T[1], 1, !tr);
            matAt(*T[0], 0, true, tr);
            matAt(*T[1], 1, true, tr);
            Reg h = s.args.size() == 2 ? toI(arg(0)) : constI(-1);
            Reg wd = s.args.size() == 2 ? toI(arg(1)) : constI(-1);
            rtcallVoid("rt_mat_cpy", {h, wd, constI(tr ? 1 : 0)});
        } else if (w == "TRANS" || w == "INV") {
            matView(*T[0], 0);
            matView(*T[T.size() > 1 ? 1 : 0], 1);
            rtcallVoid(w == "TRANS" ? "rt_mat_trans" : "rt_mat_inv", {});
        } else if (w == "ADD" || w == "SUB") {
            const int sub = w == "SUB" ? 1 : 0;
            matView(*T[0], 0);
            if (s.mode == 2) {
                rtcallVoid("rt_mat_scalar", {arg(0), constI(sub)});
            } else {
                matView(*T[s.mode == 0 ? 1 : 0], 1);
                matView(*T[s.mode == 0 ? 2 : 1], 2);
                rtcallVoid("rt_mat_addsub", {constI(sub)});
            }
        } else if (w == "MUL") {
            if (s.mode == 3) {
                matView(*T[0], 0);
                rtcallVoid("rt_mat_scalar", {arg(0), constI(2)});
            } else if (s.mode == 0) {
                for (int k = 0; k < 3; ++k) matView(*T[static_cast<std::size_t>(k)], k);
                rtcallVoid("rt_mat_mul", {});
            } else {
                const int n = s.mode == 1 ? 2 : 3;
                for (int k = 0; k < n; ++k) matView(*T[static_cast<std::size_t>(k + 1)], k);
                matScalarResult(*T[0], rtcall(s.mode == 1 ? "rt_mat_dot" : "rt_mat_triple", {}, VT::F64));
            }
        } else if (w == "NORM") {
            matView(*T[0], 0);
            rtcallVoid("rt_mat_norm", {toI(arg(0))});
        } else if (w == "DET" || w == "QDET" || w == "RANG") {
            matView(*T[1], 0);
            matAt(*T[1], 0, false, false);
            if (!s.args.empty()) rtcallVoid("rt_mat_limit", {constI(0), toI(arg(0))});
            Reg x = w == "RANG" ? rtcall("rt_mat_rank", {}, VT::F64) : rtcall("rt_mat_det", {constI(w == "QDET" ? 1 : 0)}, VT::F64);
            matScalarResult(*T[0], x);
        } else {
            unsupported(s.name);
        }
    }

    void command(const hir::Stmt& s) {
        const std::string& n = s.name;
        if (n.rfind("MAT ", 0) == 0) return mat(s);
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
        if (n == "OPEN") {
            Reg mode = expr(*s.args[0]);
            Reg ch = toI(expr(*s.args[1]));
            Reg name = expr(*s.args[2]);
            Reg len = s.args.size() > 3 ? toI(expr(*s.args[3])) : constI(0);
            rtcallVoid("rt_file_open", {mode, ch, name, len});
            return;
        }
        if (n == "CLOSE") {
            if (s.args.empty()) rtcallVoid("rt_file_close_all", {});
            else rtcallVoid("rt_file_close", {toI(expr(*s.args[0]))});
            return;
        }
        if (n == "KILL") {
            rtcallVoid("rt_file_kill", {expr(*s.args[0])});
            return;
        }
        if (n == "NAME") {
            Reg from = expr(*s.args[0]);
            rtcallVoid("rt_file_name", {from, expr(*s.args[1])});
            return;
        }
        if (n == "FIELD") {  // binds the descriptors of the string variables to the record (probe16)
            rtcallVoid("rt_file_field_begin", {toI(expr(*s.args[0]))});
            for (std::size_t k = 0; k < s.targets.size(); ++k) {
                Reg len = toI(expr(*s.args[k + 1]));
                rtcallVoid("rt_file_field_add", {load(s.targets[k]->var), len});
            }
            rtcallVoid("rt_file_field_end", {});
            return;
        }
        if (n == "GET#" || n == "PUT#") {
            Reg ch = toI(expr(*s.args[0]));
            const bool numbered = s.args.size() > 1;
            Reg rec = numbered ? toI(expr(*s.args[1])) : constI(0);
            rtcallVoid(n == "GET#" ? "rt_file_get" : "rt_file_put", {ch, rec, constI(numbered ? 1 : 0)});
            return;
        }
        if (n == "RECORD" || n == "SEEK" || n == "RELSEEK") {
            Reg ch = toI(expr(*s.args[0]));
            Reg v = toI(expr(*s.args[1]));
            rtcallVoid(n == "RECORD" ? "rt_file_record" : n == "SEEK" ? "rt_file_seek" : "rt_file_relseek", {ch, v});
            return;
        }
        if (n == "POKE" || n == "DPOKE" || n == "LPOKE") {
            Reg a = toI(expr(*s.args[0]));
            rtcallVoid(n == "POKE" ? "rt_poke" : n == "DPOKE" ? "rt_dpoke" : "rt_lpoke", {a, toI(expr(*s.args[1]))});
            return;
        }
        if (n.size() > 2 && n.substr(n.size() - 2) == "{}") {
            static const std::map<std::string, const char*, std::less<>> set = {
                {"BYTE{}", "rt_poke"},        {"CARD{}", "rt_dpoke"},         {"INT{}", "rt_dpoke"},
                {"LONG{}", "rt_lpoke"},       {"FLOAT{}", "rt_float_set"},    {"SINGLE{}", "rt_single_set"},
                {"DOUBLE{}", "rt_double_set"}, {"CHAR{}", "rt_char_set"}};
            Reg a = toI(expr(*s.args[0]));
            Reg v = expr(*s.args[1]);
            if (n != "CHAR{}" && n != "FLOAT{}" && n != "SINGLE{}" && n != "DOUBLE{}") v = toI(v);
            rtcallVoid(set.at(n), {a, v});
            return;
        }
        if (n == "BMOVE") {
            Reg from = toI(expr(*s.args[0]));
            Reg to = toI(expr(*s.args[1]));
            rtcallVoid("rt_bmove", {from, to, toI(expr(*s.args[2]))});
            return;
        }
        if (n == "OUT") {
            Reg ch = toI(expr(*s.args[0]));
            for (std::size_t k = 1; k < s.args.size(); ++k) rtcallVoid("rt_file_out", {ch, toI(expr(*s.args[k]))});
            return;
        }
        if (n == "INPUT" || n == "LINE INPUT") {  // from the keyboard (rt_kbd)
            rtcallVoid("rt_kbd_begin", {expr(*s.args[0]), constI(s.mode)});
            for (const auto& t : s.targets) {
                const Type ty = var(t->var).type;
                if (n == "LINE INPUT") storeTarget(*t, rtcall("rt_kbd_line", {}, VT::Str));
                else if (ty == Type::Str) storeTarget(*t, rtcall("rt_kbd_field", {}, VT::Str));
                else storeTarget(*t, convF(rtcall("rt_kbd_num", {}, VT::F64), ty));
            }
            rtcallVoid("rt_kbd_end", {});
            return;
        }
        if (n == "INPUT#" || n == "LINE INPUT#") {
            Reg ch = toI(expr(*s.args[0]));
            for (const auto& t : s.targets) {
                const Type ty = var(t->var).type;
                if (n == "LINE INPUT#") storeTarget(*t, rtcall("rt_file_line", {ch}, VT::Str));
                else if (ty == Type::Str) storeTarget(*t, rtcall("rt_file_item", {ch}, VT::Str));
                else storeTarget(*t, convF(rtcall("rt_file_num", {ch}, VT::F64), ty));
            }
            return;
        }
        if (n == "WRITE" || n == "WRITE#") {  // values separated by commas, strings in quotes
            const bool file = n == "WRITE#";
            if (file) rtcallVoid("rt_file_select", {toI(expr(*s.args[0]))});
            for (std::size_t k = file ? 1 : 0; k < s.args.size(); ++k) {
                if (k != (file ? 1u : 0u)) emit(Kind::PrintStr).text = ",";
                Reg v = expr(*s.args[k]);
                rtcallVoid(v.type == VT::Str ? "rt_write_str" : "rt_print_flt", {v});
            }
            if (s.mode != 1) rtcallVoid("rt_print_nl", {});  // WRITE ...; : no line feed
            if (file) rtcallVoid("rt_file_deselect", {});
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
            case Op::ToInt:
                if (intArith(*e.args[0])) return intArithCall(*e.args[0], 2);
                if (intChain(*e.args[0])) return intChainConv(*e.args[0], Type::Int);
                if (isIntLeaf(*e.args[0])) return intLeaf(*e.args[0]);
                return rtcall("fp_to_i32", {expr(*e.args[0])}, VT::I32);
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
        if (isIntLeaf(*e.args[0]) && isIntLeaf(*e.args[1])) {  // two integers: compare directly
            Reg a = intLeaf(*e.args[0]);
            return cmp(cond, a, intLeaf(*e.args[1]));
        }
        Reg a = expr(*e.args[0]);
        Reg b = expr(*e.args[1]);
        return cmp(cond, rtcall(a.type == VT::Str ? "rt_str_cmp" : "fp_cmp", {a, b}, VT::I32), constI(0));
    }

    // ---- integer fast paths (exact, D16) --------------------------------------------------------
    // An operand that is an integer value: an integer expression converted to float, or an integral
    // float constant in the 32-bit range.
    static bool isIntLeaf(const hir::Expr& e) {
        if (e.op == Op::Conv && e.type == Type::Float && hir::isInteger(e.args[0]->type)) return true;
        if (e.op == Op::ConstInt) return true;
        if (e.op == Op::ConstFloat) {
            double d = 0;
            std::memcpy(&d, &e.fval, sizeof d);
            return d == std::floor(d) && d >= -2147483648.0 && d <= 2147483647.0;
        }
        return false;
    }

    Reg intLeaf(const hir::Expr& e) {
        if (e.op == Op::Conv) return expr(*e.args[0]);
        if (e.op == Op::ConstInt) return constI(e.ival);
        double d = 0;
        std::memcpy(&d, &e.fval, sizeof d);
        return constI(static_cast<std::int32_t>(d));
    }

    // a+b, a-b, a*b of two integer operands whose result goes into an integer: in float the exact
    // result fits 32 bits or the conversion fails; the same as a 32-bit operation with overflow
    // check (error of the target width). Not for chains (an intermediate result could overflow).
    bool intArith(const hir::Expr& x) const {
        return (x.op == Op::Add || x.op == Op::Sub || x.op == Op::Mul) && isIntLeaf(*x.args[0]) && isIntLeaf(*x.args[1]);
    }

    // A chain of + - * over integer operands (intChain): in 32 bits with a jump to the float path on
    // the first overflow. Without an overflow every intermediate value has |v| < 2^31, so the float
    // path computes the same values exactly (D16 addendum). The operands are evaluated again on the
    // float path, so they must be free of side effects: variables, constants, array elements with
    // such indices.
    static bool pureLeaf(const hir::Expr& e) {
        switch (e.op) {
            case Op::ConstInt:
            case Op::ConstFloat:
            case Op::Load: return true;
            case Op::LoadElem:
                for (const auto& a : e.args)
                    if (!pureLeaf(*a) && !(a->op == Op::Conv && pureLeaf(*a->args[0]))) return false;
                return true;
            case Op::Conv: return pureLeaf(*e.args[0]);
            default: return false;
        }
    }

    static bool chainOperand(const hir::Expr& e) {
        if (e.op == Op::Add || e.op == Op::Sub || e.op == Op::Mul) return chainOperand(*e.args[0]) && chainOperand(*e.args[1]);
        return isIntLeaf(e) && pureLeaf(e);
    }

    static bool intChain(const hir::Expr& x) {
        return (x.op == Op::Add || x.op == Op::Sub || x.op == Op::Mul) && chainOperand(*x.args[0]) && chainOperand(*x.args[1]);
    }

    Reg chainEval(const hir::Expr& e, int overflow) {
        if (e.op != Op::Add && e.op != Op::Sub && e.op != Op::Mul) return intLeaf(e);
        Reg a = chainEval(*e.args[0], overflow);
        Reg b = chainEval(*e.args[1], overflow);
        Reg r = reg(VT::I32);
        Instr& i = emit(Kind::OpOvf);
        i.dst = r;
        i.a = a;
        i.b = b;
        i.op = e.op == Op::Add ? "add" : e.op == Op::Sub ? "sub" : "mul";
        i.label = overflow;
        return r;
    }

    void mov(Reg dst, Reg a) {
        Instr& i = emit(Kind::Op);
        i.dst = dst;
        i.a = a;
        i.op = "mov";
    }

    // the value of the chain x converted to `to` (Int, Word, Byte) like an assignment
    Reg intChainConv(const hir::Expr& x, Type to) {
        const int overflow = label(), done = label();
        Reg r = reg(VT::I32);
        Reg v = chainEval(x, overflow);
        if (to == Type::Word) v = rtcall("rt_conv_word", {v}, VT::I32);
        else if (to == Type::Byte) v = rtcall("rt_conv_byte", {v}, VT::I32);
        mov(r, v);
        jump(done);
        place(overflow);
        mov(r, convF(expr(x), to));
        place(done);
        return r;
    }

    Reg intArithCall(const hir::Expr& x, int error) {
        Reg a = intLeaf(*x.args[0]);
        Reg b = intLeaf(*x.args[1]);
        const char* fn = x.op == Op::Add ? "rt_iadd_chk" : x.op == Op::Sub ? "rt_isub_chk" : "rt_imul_chk";
        return rtcall(fn, {a, b, constI(error)}, VT::I32);
    }

    Reg conv(const hir::Expr& e) {
        const hir::Expr& x = *e.args[0];
        if ((e.type == Type::Int || e.type == Type::Word || e.type == Type::Byte) && intArith(x)) {
            Reg r = intArithCall(x, e.type == Type::Int ? 2 : e.type == Type::Word ? 4 : 3);
            if (e.type == Type::Word) return rtcall("rt_conv_word", {r}, VT::I32);
            if (e.type == Type::Byte) return rtcall("rt_conv_byte", {r}, VT::I32);
            return r;
        }
        if ((e.type == Type::Int || e.type == Type::Word || e.type == Type::Byte) && intChain(x)) return intChainConv(x, e.type);
        return convF(expr(x), e.type);
    }

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
        static const std::map<std::string, std::string> math = {
            {"EXP", "rt_exp"}, {"LOG", "rt_log"}, {"LOG10", "rt_log10"}, {"SIN", "rt_sin"}, {"COS", "rt_cos"},
            {"TAN", "rt_tan"}, {"ATN", "rt_atn"}, {"ASIN", "rt_asin"}, {"ACOS", "rt_acos"}, {"DEG", "rt_deg"},
            {"RAD", "rt_rad"}};
        if (const auto it = math.find(n); it != math.end()) return rtcall(it->second, {arg(e, 0)}, VT::F64);
        if (n == "SINQ" || n == "COSQ") {  // SIN(RAD(x)); x must be in the integer range (probe13 T21)
            Reg x = arg(e, 0);
            rtcall("fp_to_i32", {x}, VT::I32);
            return rtcall(n == "SINQ" ? "rt_sin" : "rt_cos", {rtcall("rt_rad", {x}, VT::F64)}, VT::F64);
        }
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
        if (n == "EOF") return rtcall("rt_file_eof", {toI(arg(e, 0))}, VT::I32);
        if (n == "LOF") return rtcall("rt_file_lof", {toI(arg(e, 0))}, VT::I32);
        if (n == "EXIST") return rtcall("rt_file_exist", {arg(e, 0)}, VT::I32);
        if (n == "LOC") return rtcall("rt_file_loc", {toI(arg(e, 0))}, VT::I32);
        if (n == "VARPTR") {  // the address of the storage; strings: of their characters
            const hir::Expr& t = *e.args[0];
            Reg a = t.op == Op::Load ? addrOf(t.var) : elemAddr(t);
            if (var(t.var).type == Type::Str) return rtcall("rt_mem_chars", {a}, VT::I32);
            Reg r = reg(VT::I32);
            copyTo(r, a);
            return r;
        }
        if (n.size() > 2 && n.substr(n.size() - 2) == "{}") {  // BYTE{a} ... CHAR{a}
            static const std::map<std::string, std::pair<const char*, VT>, std::less<>> get = {
                {"BYTE{}", {"rt_peek", VT::I32}},         {"CARD{}", {"rt_dpeek", VT::I32}},
                {"INT{}", {"rt_wpeek", VT::I32}},         {"LONG{}", {"rt_lpeek", VT::I32}},
                {"FLOAT{}", {"rt_float_get", VT::F64}},   {"SINGLE{}", {"rt_single_get", VT::F64}},
                {"DOUBLE{}", {"rt_double_get", VT::F64}}, {"CHAR{}", {"rt_char_get", VT::Str}}};
            const auto& g = get.at(n);
            return rtcall(g.first, {toI(arg(e, 0))}, g.second);
        }
        if (n == "PEEK" || n == "DPEEK" || n == "LPEEK")
            return rtcall(n == "PEEK" ? "rt_peek" : n == "DPEEK" ? "rt_dpeek" : "rt_lpeek", {toI(arg(e, 0))}, VT::I32);
        if (n == "MALLOC") {
            Reg size = toI(arg(e, 0));
            return rtcall("rt_malloc", {size, e.args.size() > 1 ? toI(arg(e, 1)) : constI(0)}, VT::I32);
        }
        if (n == "MFREE") {
            Reg a = toI(arg(e, 0));
            return rtcall("rt_mfree", {a, toI(arg(e, 1))}, VT::I32);
        }
        if (n == "MKI$" || n == "MKL$") return rtcall(n == "MKI$" ? "rt_mki" : "rt_mkl", {toI(arg(e, 0))}, VT::Str);
        if (n == "MKS$" || n == "MKD$") return rtcall(n == "MKS$" ? "rt_mks" : "rt_mkd", {arg(e, 0)}, VT::Str);
        if (n == "CVI" || n == "CVL") return rtcall(n == "CVI" ? "rt_cvi" : "rt_cvl", {arg(e, 0)}, VT::I32);
        if (n == "CVS" || n == "CVD") return rtcall(n == "CVS" ? "rt_cvs" : "rt_cvd", {arg(e, 0)}, VT::F64);
        if (n == "INP") return rtcall("rt_file_inp", {toI(arg(e, 0))}, VT::I32);
        if (n == "INKEY$") return rtcall("rt_kbd_inkey", {}, VT::Str);
        if (n == "INPUT$" && e.args.size() == 1) return rtcall("rt_kbd_chars", {toI(arg(e, 0))}, VT::Str);
        if (n == "INPUT$") {
            Reg cnt = toI(arg(e, 0));
            return rtcall("rt_file_chars", {cnt, toI(arg(e, 1))}, VT::Str);
        }
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
