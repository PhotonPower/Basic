#include "m68k_backend.hpp"

#include "ir/lower.hpp"
#include "link/embedded.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace basic {

namespace {

using lir::Instr;
using lir::Kind;
using lir::Reg;
using lir::VT;

std::string hex32(std::uint32_t v) {
    static const char* d = "0123456789ABCDEF";
    std::string s = "$";
    for (int i = 7; i >= 0; --i) s += d[(v >> (i * 4)) & 15];
    return s;
}

class Gen {
public:
    explicit Gen(const lir::Program& p) : p_(p) {}

    link::Program run() {
        for (std::size_t v = 0; v < p_.hir->vars.size(); ++v) {
            const hir::Var& var = p_.hir->vars[v];
            out_.bss += varLabel(static_cast<int>(v)) + ":\n        DS.L    2                       ; " + var.display() + "\n";
            if (indirect(static_cast<int>(v)))  // VAR parameter: the pointer to the current storage
                out_.data += ptrLabel(static_cast<int>(v)) + ":\n        DC.L    " + varLabel(static_cast<int>(v)) + "\n";
        }
        for (const lir::Function& f : p_.functions) function(f);
        out_.provides.push_back("main");
        if (uses_.count("rt_data_str") + uses_.count("rt_data_num") + uses_.count("rt_data_restore") != 0) dataTable();
        if (p_.onError) errorTables();
        out_.uses.assign(uses_.begin(), uses_.end());
        return out_;
    }

private:
    const lir::Program& p_;
    link::Program out_;
    std::set<std::string> uses_;
    int strings_ = 0;
    const lir::Function* f_ = nullptr;

    void line(const std::string& s) { out_.code += "        " + s + "\n"; }

    static std::string varLabel(int v) { return "v" + std::to_string(v); }
    static std::string ptrLabel(int v) { return "p" + std::to_string(v); }
    bool indirect(int v) const { return v >= 0 && static_cast<std::size_t>(v) < p_.indirect.size() && p_.indirect[static_cast<std::size_t>(v)]; }

    // operand for the storage of a variable at byte offset `off`; an indirect variable is reached
    // through A1, which is loaded here
    std::string varMem(int v, int off = 0) {
        if (!indirect(v)) return varLabel(v) + (off ? "+" + std::to_string(off) : "");
        if (off == 0) line("MOVE.L  " + ptrLabel(v) + ",A1");
        return off ? std::to_string(off) + "(A1)" : "(A1)";
    }

    // value of type t from memory (m0 = operand at offset 0, m4 = at offset 4) into a register slot
    void loadValue(hir::Type t, const std::string& m0, const std::string& m4, const Reg& dst) {
        switch (t) {
            case hir::Type::Float:
                line("MOVE.L  " + m0 + "," + slot(dst));
                line("MOVE.L  " + m4 + "," + slot(dst, 4));
                return;
            case hir::Type::Word:
                line("MOVE.W  " + m0 + ",D0");
                line("EXT.L   D0");
                break;
            case hir::Type::Byte:
                line("MOVEQ   #0,D0");
                line("MOVE.B  " + m0 + ",D0");
                break;
            case hir::Type::Bool:
                line("MOVE.B  " + m0 + ",D0");
                line("EXT.W   D0");
                line("EXT.L   D0");
                break;
            default: line("MOVE.L  " + m0 + ",D0"); break;
        }
        line("MOVE.L  D0," + slot(dst));
    }

    void storeValue(hir::Type t, const std::string& m0, const std::string& m4, const Reg& src) {
        switch (t) {
            case hir::Type::Float:
                line("MOVE.L  " + slot(src) + "," + m0);
                line("MOVE.L  " + slot(src, 4) + "," + m4);
                return;
            case hir::Type::Word: line("MOVE.W  " + slot(src, 2) + "," + m0); return;
            case hir::Type::Byte:
            case hir::Type::Bool: line("MOVE.B  " + slot(src, 3) + "," + m0); return;
            default: line("MOVE.L  " + slot(src) + "," + m0); return;
        }
    }

    int local_ = 0;  // numbering of local labels of the backend itself
    std::set<int> savedVars_;  // variables with a Save (ON ERROR: descriptors for the save log)

    // ---- ON ERROR (docs/runtime.md): statement record in the frame, save log -------------------
    std::string rec(int off = 0) const { return std::to_string(-8 * slots_ - 24 + off) + "(A6)"; }
    bool hasRecord() const { return p_.onError && !f_->deffn; }

    // descriptors of the saved variables for undoing (rt_onerr) and the addresses of RESUME labels
    void errorTables() {
        for (int v : savedVars_) {
            const bool str = varType(v) == hir::Type::Str;
            out_.data += "vd" + std::to_string(v) + ":\n        DC.L    " + varLabel(v) + "," + (indirect(v) ? ptrLabel(v) : std::string("0")) + "," +
                         (str ? "rt_str_free,rt_str_relink" : "0,0") + "\n";
            if (str) {
                uses_.insert("rt_str_free");
                uses_.insert("rt_str_relink");
            }
        }
        if (uses_.count("rt_resume") != 0) {
            out_.bss += "prg_rlab:\n        DS.L    " + std::to_string(2 * std::max<std::size_t>(1, p_.hir->labels.size())) + "\n";
            out_.provides.push_back("prg_rlab");
        }
    }

    // DATA items for rt_data: descriptors (address, length, quoted) and their characters
    void dataTable() {
        const auto& items = p_.hir->data;
        out_.data += "prg_data_count:\n        DC.L    " + std::to_string(items.size()) + "\nprg_data:\n";
        std::string texts;
        for (std::size_t k = 0; k < items.size(); ++k) {
            const std::string l = "d" + std::to_string(k);
            out_.data += "        DC.L    " + (items[k].text.empty() ? std::string("0") : l) + "\n        DC.W    " +
                         std::to_string(items[k].text.size()) + "," + (items[k].quoted ? "1" : "0") + "\n";
            if (!items[k].text.empty()) texts += l + ":\n        DC.B    " + bytes(items[k].text) + "\n";
        }
        out_.data += texts + "        EVEN\n";
        out_.provides.push_back("prg_data");
        out_.provides.push_back("prg_data_count");
    }

    static std::string bytes(const std::string& t) {
        std::string b;
        for (std::size_t k = 0; k < t.size(); ++k) b += (k ? "," : "") + std::to_string(static_cast<unsigned char>(t[k]));
        return b;
    }

    std::string fname(int routine) const {
        if (routine < 0) return "main";
        std::string n = "u_";
        for (char c : p_.hir->routines[static_cast<std::size_t>(routine)].name)
            n += (c == '$' || c == '%' || c == '&' || c == '|' || c == '!' || c == '#') ? '_' : c;
        return n;
    }

    // slot of a virtual register in the frame
    // frame slot of a virtual register (shared by registers whose lifetimes do not overlap); a
    // register that only ever holds one integer constant is the immediate value instead (reads
    // only: such a register is never written after its ConstI, which is not emitted)
    std::string slot(const Reg& r, int offset = 0) const {
        const auto k = static_cast<std::size_t>(r.id);
        if (k < alias_.size() && !alias_[k].empty() && offset == 0) return alias_[k];
        if (k < constOf_.size() && constKnown_[k]) {
            const std::int32_t v = constOf_[k];
            if (offset == 2) return "#" + std::to_string(static_cast<std::int16_t>(v & 0xFFFF));
            if (offset == 3) return "#" + std::to_string(v & 0xFF);
            return "#" + std::to_string(v);
        }
        return std::to_string(-8 * (slotOf_[k] + 1) + offset) + "(A6)";
    }
    // a %-variable addressed directly (not through a VAR pointer)
    bool directLong(int v) const { return v >= 0 && !indirect(v) && varType(v) == hir::Type::Int; }
    static bool reads(const Instr& in, const Reg& r) {
        if (in.a.id == r.id || in.b.id == r.id) return true;
        for (const Reg& x : in.args)
            if (x.id == r.id) return true;
        return false;
    }

    bool isConst(const Reg& r) const {
        return r.id >= 0 && static_cast<std::size_t>(r.id) < constKnown_.size() && constKnown_[static_cast<std::size_t>(r.id)];
    }

    // a register that is a direct %-variable: loaded and read right after (Load folded into its
    // single use), or computed and stored right after (the Store folded into its definition)
    std::vector<std::string> alias_;
    std::vector<std::int32_t> constOf_;
    std::vector<bool> constKnown_;
    std::vector<int> uses_count_;

    // registers defined exactly once, by ConstI; number of reads of every register
    void scanConsts(const lir::Function& f) {
        const std::size_t n = static_cast<std::size_t>(f.regs);
        std::vector<int> defs(n, 0);
        alias_.assign(n, std::string());  // per routine, before the prologue reads the slots
        constOf_.assign(n, 0);
        constKnown_.assign(n, false);
        uses_count_.assign(n, 0);
        for (const Reg& r : f.params) defs[static_cast<std::size_t>(r.id)] += 2;  // set on entry
        for (const Instr& in : f.code) {
            if (in.dst.id >= 0) {
                const auto k = static_cast<std::size_t>(in.dst.id);
                ++defs[k];
                if (in.kind == Kind::ConstI) constOf_[k] = in.imm;
            }
            for (const Reg* r : {&in.a, &in.b})
                if (r->id >= 0) ++uses_count_[static_cast<std::size_t>(r->id)];
            for (const Reg& r : in.args)
                if (r.id >= 0) ++uses_count_[static_cast<std::size_t>(r.id)];
        }
        for (const Instr& in : f.code)
            if (in.kind == Kind::ConstI && in.dst.id >= 0 && defs[static_cast<std::size_t>(in.dst.id)] == 1)
                constKnown_[static_cast<std::size_t>(in.dst.id)] = true;
    }

    std::vector<int> slotOf_;  // virtual register -> slot
    int slots_ = 0;

    // Lifetimes in code order: from the first to the last appearance of a register. A register that
    // lives into a loop (a backward jump to a label inside its lifetime, or used before and after
    // it) lives to the end of the loop. Then a linear scan gives each register a free slot.
    void assignSlots(const lir::Function& f) {
        const std::size_t n = static_cast<std::size_t>(f.regs);
        std::vector<long> first(n, 0), last(n, 0);
        std::vector<bool> seen(n, false);
        auto touch = [&](const Reg& r, long pos) {
            if (r.id < 0) return;
            const auto k = static_cast<std::size_t>(r.id);
            if (!seen[k] || pos < first[k]) first[k] = pos;
            if (!seen[k] || pos > last[k]) last[k] = pos;
            seen[k] = true;
        };
        for (const Reg& r : f.params) touch(r, -1);
        std::vector<long> labelPos(static_cast<std::size_t>(f.labels), -1);
        for (std::size_t i = 0; i < f.code.size(); ++i) {
            const Instr& in = f.code[i];
            const long pos = static_cast<long>(i);
            touch(in.dst, pos);
            touch(in.a, pos);
            touch(in.b, pos);
            for (const Reg& r : in.args) touch(r, pos);
            if (in.kind == Kind::Label) labelPos[static_cast<std::size_t>(in.label)] = pos;
        }
        // backward jumps (loops, GOTO); with ON ERROR also the jumps of the runtime: RESUME repeats a
        // statement (or a header line), RESUME label may come from anywhere after the label
        std::vector<std::pair<long, long>> loops;
        const long end = static_cast<long>(f.code.size());
        for (std::size_t i = 0; i < f.code.size(); ++i) {
            const Instr& in = f.code[i];
            if ((in.kind == Kind::Jump || in.kind == Kind::BranchZ || in.kind == Kind::BranchNZ) && in.label >= 0) {
                const long target = labelPos[static_cast<std::size_t>(in.label)];
                if (target >= 0 && target <= static_cast<long>(i)) loops.emplace_back(target, static_cast<long>(i));
            }
            if (in.kind == Kind::Line && in.label >= 0 && in.label2 >= 0) {
                const long from = labelPos[static_cast<std::size_t>(in.label)];
                const long to = labelPos[static_cast<std::size_t>(in.label2)];
                if (from >= 0) loops.emplace_back(from, to >= from ? to : end);
            }
        }
        for (const auto& rl : f.resumeLabels) {
            const long target = labelPos[static_cast<std::size_t>(rl.second)];
            if (target >= 0) loops.emplace_back(target, end);
        }
        for (bool grown = true; grown;) {
            grown = false;
            for (const auto& [from, to] : loops)
                for (std::size_t k = 0; k < n; ++k) {
                    if (!seen[k] || last[k] < from || first[k] > to) continue;  // no overlap
                    if (first[k] >= from && last[k] <= to) continue;               // inside one pass
                    if (first[k] > from) first[k] = from, grown = true;
                    if (last[k] < to) last[k] = to, grown = true;
                }
        }
        // linear scan
        std::vector<std::size_t> order;
        for (std::size_t k = 0; k < n; ++k)
            if (seen[k]) order.push_back(k);
        std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return first[a] < first[b]; });
        slotOf_.assign(n, 0);
        slots_ = 0;
        std::vector<long> slotEnd;  // last position that uses the slot
        for (std::size_t k : order) {
            int chosen = -1;
            for (std::size_t sl = 0; sl < slotEnd.size(); ++sl)
                if (slotEnd[sl] < first[k]) {
                    chosen = static_cast<int>(sl);
                    break;
                }
            if (chosen < 0) {
                chosen = static_cast<int>(slotEnd.size());
                slotEnd.push_back(0);
            }
            slotEnd[static_cast<std::size_t>(chosen)] = last[k];
            slotOf_[k] = chosen;
        }
        slots_ = static_cast<int>(slotEnd.size());
    }

    void addrTo(int v, const std::string& areg) {
        if (indirect(v)) line("MOVE.L  " + ptrLabel(v) + "," + areg);
        else line("LEA     " + varLabel(v) + "," + areg);
    }

    hir::Type varType(int v) const { return p_.hir->vars[static_cast<std::size_t>(v)].type; }

    // register arguments: an f64 takes a pair (D0:D1 or D2:D3), an integer the next data register,
    // an address (string descriptor, variable, element) the next of A0, A1
    void loadArgs(const std::vector<Reg>& args) {
        int next = 0, nextA = 0;
        for (const Reg& a : args) {
            if (a.type == VT::Str || a.type == VT::Addr) {
                line("MOVE.L  " + slot(a) + ",A" + std::to_string(nextA++));
            } else if (a.type == VT::F64) {
                if (next % 2 != 0) ++next;
                line("MOVE.L  " + slot(a) + ",D" + std::to_string(next));
                line("MOVE.L  " + slot(a, 4) + ",D" + std::to_string(next + 1));
                next += 2;
            } else {
                line("MOVE.L  " + slot(a) + ",D" + std::to_string(next));
                ++next;
            }
        }
    }

    void storeResult(const Reg& d) {
        if (d.id < 0) return;
        if (d.type == VT::Str || d.type == VT::Addr) {
            line("MOVE.L  A0," + slot(d));
            return;
        }
        line("MOVE.L  D0," + slot(d));
        if (d.type == VT::F64) line("MOVE.L  D1," + slot(d, 4));
    }

    void function(const lir::Function& f) {
        f_ = &f;
        const std::string name = fname(f.routine);
        out_.code += "; ---- " + (f.routine < 0 ? std::string("main program") : f.name) + "\n" + name + ":\n";
        scanConsts(f);
        assignSlots(f);
        line("LINK    A6,#" + std::to_string(-8 * slots_ - (p_.onError ? 24 : 0)));
        if (hasRecord()) {  // own statement record; until the first statement a copy of the caller's
            uses_.insert("rt_err_ctx");
            const std::string l = ".E" + std::to_string(local_++);
            line("MOVE.L  rt_err_ctx,A0");
            line("MOVE.L  A0," + rec());
            line("BEQ     " + l);
            for (int off = 4; off <= 16; off += 4) line("MOVE.L  " + std::to_string(off) + "(A0)," + rec(off));
            out_.code += l + ":\n";
            line("LEA     " + name + ",A0");
            line("MOVE.L  A0," + rec(20));
            line("LEA     " + rec() + ",A0");
            line("MOVE.L  A0,rt_err_ctx");
        }
        for (const auto& [hirLabel, lirLabel] : f.resumeLabels) {  // addresses for RESUME label
            line("LEA     .L" + std::to_string(lirLabel) + ",A0");
            line("MOVE.L  A0,prg_rlab+" + std::to_string(8 * hirLabel));
            line("LEA     " + name + ",A0");
            line("MOVE.L  A0,prg_rlab+" + std::to_string(8 * hirLabel + 4));
        }
        for (std::size_t k = 0; k < f.params.size(); ++k) {  // incoming arguments: 8 bytes each from 8(A6)
            const Reg& r = f.params[k];
            line("MOVE.L  " + std::to_string(8 + 8 * k) + "(A6)," + slot(r));
            if (r.type == VT::F64) line("MOVE.L  " + std::to_string(12 + 8 * k) + "(A6)," + slot(r, 4));
        }
        for (std::size_t k = 0; k < f.code.size(); ++k) {
            const Instr& i = f.code[k];
            const bool once = i.dst.id >= 0 && uses_count_[static_cast<std::size_t>(i.dst.id)] == 1;
            if (once && i.kind == Kind::Load && k + 1 < f.code.size() && directLong(i.var) && reads(f.code[k + 1], i.dst)) {
                alias_[static_cast<std::size_t>(i.dst.id)] = varLabel(i.var);  // read from the variable itself
                continue;
            }
            if (once && (i.kind == Kind::Op || i.kind == Kind::RtCall) && i.dst.type == VT::I32 && k + 1 < f.code.size()) {
                const Instr& n = f.code[k + 1];
                if (n.kind == Kind::Store && n.a.id == i.dst.id && directLong(n.var)) {
                    alias_[static_cast<std::size_t>(i.dst.id)] = varLabel(n.var);  // result straight into the variable
                    instr(i);
                    ++k;
                    continue;
                }
            }
            // a comparison only read by the branch right after it: CMP and a conditional branch
            if (i.kind == Kind::Cmp && k + 1 < f.code.size() && uses_count_[static_cast<std::size_t>(i.dst.id)] == 1) {
                const Instr& b = f.code[k + 1];
                if ((b.kind == Kind::BranchZ || b.kind == Kind::BranchNZ) && b.a.id == i.dst.id) {
                    cmpBranch(i, b.kind == Kind::BranchNZ, b.label);
                    ++k;
                    continue;
                }
            }
            instr(i);
        }
    }

    // Cmp a <cond> b followed by a branch on its result (taken if the result is non-zero for BranchNZ,
    // zero for BranchZ)
    void cmpBranch(const Instr& c, bool ifTrue, int target) {
        static const std::map<std::string, std::pair<const char*, const char*>> cc = {
            {"eq", {"BEQ", "BNE"}}, {"ne", {"BNE", "BEQ"}}, {"lt", {"BLT", "BGE"}},
            {"le", {"BLE", "BGT"}}, {"gt", {"BGT", "BLE"}}, {"ge", {"BGE", "BLT"}}};
        const auto& [yes, no] = cc.at(c.op);
        line("MOVE.L  " + slot(c.a) + ",D0");
        line("CMP.L   " + slot(c.b) + ",D0");
        line(std::string(ifTrue ? yes : no) + "     .L" + std::to_string(target));
    }

    void instr(const Instr& i) {
        switch (i.kind) {
            case Kind::ConstI:
                if (!isConst(i.dst)) line("MOVE.L  #" + std::to_string(i.imm) + "," + slot(i.dst));
                return;
            case Kind::ConstF:
                line("MOVE.L  #" + hex32(static_cast<std::uint32_t>(i.fimm >> 32)) + "," + slot(i.dst));
                line("MOVE.L  #" + hex32(static_cast<std::uint32_t>(i.fimm)) + "," + slot(i.dst, 4));
                return;
            case Kind::ConstS: {
                const std::string l = "c" + std::to_string(strings_++);
                out_.data += l + ":\n        DC.L    " + (i.text.empty() ? std::string("0") : l + "t") + "\n        DC.W    " +
                             std::to_string(i.text.size()) + ",0\n";
                if (!i.text.empty()) out_.data += l + "t:\n        DC.B    " + bytes(i.text) + "\n        EVEN\n";
                line("LEA     " + l + ",A0");
                line("MOVE.L  A0," + slot(i.dst));
                return;
            }
            case Kind::Load:
                if (varType(i.var) == hir::Type::Str) {  // the descriptor of the variable
                    addrTo(i.var, "A0");
                    line("MOVE.L  A0," + slot(i.dst));
                    return;
                }
                {
                    const std::string m0 = varMem(i.var);
                    loadValue(varType(i.var), m0, varMem(i.var, 4), i.dst);
                }
                return;
            case Kind::Store: {
                const std::string m0 = varMem(i.var);
                storeValue(varType(i.var), m0, varMem(i.var, 4), i.a);
                return;
            }
            case Kind::Addr:
                addrTo(i.var, "A0");
                line("MOVE.L  A0," + slot(i.dst));
                return;
            case Kind::LoadAt:
                line("MOVE.L  " + slot(i.a) + ",A1");
                loadValue(i.mem, "(A1)", "4(A1)", i.dst);
                return;
            case Kind::StoreAt:
                line("MOVE.L  " + slot(i.a) + ",A1");
                storeValue(i.mem, "(A1)", "4(A1)", i.b);
                return;
            case Kind::Bind: {  // after Save: flag at (SP), saved pointer at 4(SP) (8 more with the log)
                const std::string l = ".B" + std::to_string(local_++);
                const int o = p_.onError ? 8 : 0;
                line("MOVE.L  " + slot(i.a) + ",D0");
                line("CMP.L   " + std::to_string(4 + o) + "(SP),D0");
                line("BNE     " + l);
                line("MOVE.L  #1," + (o ? std::to_string(o) : std::string()) + "(SP)");  // already this storage: keep its value at the end (no restore)
                if (varType(i.var) == hir::Type::Str) {
                    uses_.insert("rt_str_relink");
                    line("MOVE.L  D0,A0");
                    line("JSR     rt_str_relink");
                    line("MOVE.L  " + slot(i.a) + ",D0");
                }
                out_.code += l + ":\n";
                line("MOVE.L  D0," + ptrLabel(i.var));
                return;
            }
            case Kind::Op: {
                if (i.op == "mov") {
                    line("MOVE.L  " + slot(i.a) + "," + slot(i.dst));
                    if (i.dst.type == VT::F64) line("MOVE.L  " + slot(i.a, 4) + "," + slot(i.dst, 4));
                    return;
                }
                line("MOVE.L  " + slot(i.a) + ",D0");
                if (i.op == "not") line("NOT.L   D0");
                else if (i.op == "neg") line("NEG.L   D0");
                else {
                    const std::string m = i.op == "add" ? "ADD" : i.op == "sub" ? "SUB" : i.op == "and" ? "AND" : i.op == "or" ? "OR" : "EOR";
                    if (m == "EOR") {
                        line("MOVE.L  " + slot(i.b) + ",D1");
                        line("EOR.L   D1,D0");
                    } else {
                        line(m + ".L   " + slot(i.b) + ",D0");
                    }
                }
                line("MOVE.L  D0," + slot(i.dst));
                return;
            }
            case Kind::Cmp: {
                const std::string s = i.op == "eq" ? "SEQ" : i.op == "ne" ? "SNE" : i.op == "lt" ? "SLT" : i.op == "le" ? "SLE" : i.op == "gt" ? "SGT" : "SGE";
                line("MOVE.L  " + slot(i.a) + ",D0");
                line("CMP.L   " + slot(i.b) + ",D0");
                line(s + "     D0");
                line("EXT.W   D0");
                line("EXT.L   D0");
                line("MOVE.L  D0," + slot(i.dst));
                return;
            }
            case Kind::RtCall:
                loadArgs(i.args);
                uses_.insert(i.op);
                line("JSR     " + i.op);
                storeResult(i.dst);
                return;
            case Kind::Call: {
                for (auto it = i.args.rbegin(); it != i.args.rend(); ++it) {  // 8 bytes each, first argument on top
                    line("MOVE.L  " + slot(*it, 4) + ",-(SP)");
                    line("MOVE.L  " + slot(*it) + ",-(SP)");
                }
                line("JSR     " + fname(i.routine));
                if (!i.args.empty()) line("LEA     " + std::to_string(8 * i.args.size()) + "(SP),SP");
                if (i.dst.type == VT::Str && i.dst.id >= 0) {  // string result: a temporary of the caller
                    uses_.insert("rt_str_take_ret");
                    line("JSR     rt_str_take_ret");
                }
                storeResult(i.dst);
                return;
            }
            case Kind::Jump: line("BRA     .L" + std::to_string(i.label)); return;
            case Kind::BranchZ:
            case Kind::BranchNZ:
                if (isConst(i.a)) {  // a known condition
                    const bool zero = constOf_[static_cast<std::size_t>(i.a.id)] == 0;
                    if (zero == (i.kind == Kind::BranchZ)) line("BRA     .L" + std::to_string(i.label));
                    return;
                }
                line("TST.L   " + slot(i.a));
                line(std::string(i.kind == Kind::BranchZ ? "BEQ" : "BNE") + "     .L" + std::to_string(i.label));
                return;
            case Kind::Label: out_.code += ".L" + std::to_string(i.label) + ":\n"; return;
            case Kind::Return:
                if (hasRecord()) line("MOVE.L  " + rec() + ",rt_err_ctx");
                if (i.a.id >= 0) {
                    line("MOVE.L  " + slot(i.a) + ",D0");
                    if (i.a.type == VT::F64) line("MOVE.L  " + slot(i.a, 4) + ",D1");
                }
                line("UNLK    A6");
                line("RTS");
                return;
            case Kind::Save: {
                const std::string v = varLabel(i.var);
                const bool str = varType(i.var) == hir::Type::Str;
                line("MOVE.L  " + v + "+4,-(SP)");
                line("MOVE.L  " + v + ",-(SP)");
                if (str) {  // the saved copy owns the block now
                    uses_.insert("rt_str_relink");
                    line("MOVE.L  SP,A0");
                    line("JSR     rt_str_relink");
                }
                if (indirect(i.var)) {  // pointer and flag (set by Bind), then the own storage again
                    line("MOVE.L  " + ptrLabel(i.var) + ",-(SP)");
                    line("CLR.L   -(SP)");
                    line("MOVE.L  #" + v + "," + ptrLabel(i.var));
                }
                if (p_.onError) {  // save log entry: descriptor, next entry
                    savedVars_.insert(i.var);
                    uses_.insert("rt_err_log");
                    line("MOVE.L  rt_err_log,-(SP)");
                    line("PEA     vd" + std::to_string(i.var));
                    line("MOVE.L  SP,rt_err_log");
                }
                return;
            }
            case Kind::Clear:
                line("CLR.L   " + varLabel(i.var));
                line("CLR.L   " + varLabel(i.var) + "+4");
                return;
            case Kind::Restore: {
                const std::string v = varLabel(i.var);
                const bool str = varType(i.var) == hir::Type::Str;
                std::string done;
                if (p_.onError) {
                    line("ADDQ.L  #4,SP");
                    line("MOVE.L  (SP)+,rt_err_log");
                }
                if (indirect(i.var)) {
                    const std::string keep = ".R" + std::to_string(local_++);
                    done = ".R" + std::to_string(local_++);
                    line("MOVE.L  (SP)+,D0");
                    line("MOVE.L  (SP)+," + ptrLabel(i.var));
                    line("TST.L   D0");
                    line("BEQ     " + keep);
                    line("ADDQ.L  #8,SP");
                    line("BRA     " + done);
                    out_.code += keep + ":\n";
                }
                if (str) {  // free the current string, take back the saved one
                    uses_.insert("rt_str_free");
                    line("LEA     " + v + ",A0");
                    line("JSR     rt_str_free");
                }
                line("MOVE.L  (SP)+," + v);
                line("MOVE.L  (SP)+," + v + "+4");
                if (str) {
                    line("LEA     " + v + ",A0");
                    line("JSR     rt_str_relink");
                }
                if (!done.empty()) out_.code += done + ":\n";
                return;
            }
            case Kind::PrintStr: {
                if (i.text.empty()) return;
                const std::string l = "s" + std::to_string(strings_++);
                out_.data += l + ":\n        DC.B    " + bytes(i.text) + "\n        EVEN\n";  // descriptors follow: even addresses
                uses_.insert("rt_print_str");
                line("LEA     " + l + ",A0");
                line("MOVE.L  #" + std::to_string(i.text.size()) + ",D0");
                line("JSR     rt_print_str");
                return;
            }
            case Kind::Line:
                line("; line " + std::to_string(i.imm));
                if (i.label >= 0 && hasRecord()) {
                    line("LEA     .L" + std::to_string(i.label) + ",A0");
                    line("MOVE.L  A0," + rec(4));
                    line("LEA     .L" + std::to_string(i.label2) + ",A0");
                    line("MOVE.L  A0," + rec(8));
                    line("MOVE.L  SP," + rec(12));
                    line("MOVE.L  A6," + rec(16));
                }
                return;
            case Kind::RoutineAddr:
                line("LEA     " + fname(i.routine) + ",A0");
                line("MOVE.L  A0," + slot(i.dst));
                return;
        }
    }
};

}  // namespace

link::Program generateM68k(const lir::Program& program) {
    Gen g(program);
    return g.run();
}

std::string buildM68kProgram(const hir::Program& program, M68kTarget target, Diagnostics& diags) {
    const lir::Program l = lower(program, diags);
    if (!diags.empty()) return "";
    const link::Program prog = generateM68k(l);
    const bool amiga = target == M68kTarget::Amiga;
    std::vector<link::Module> mods = link::embeddedModulesOf({"runtime/m68k", amiga ? "platform/amiga" : "platform/m68k-test"}, diags);
    const link::Library lib = link::makeLibrary(std::move(mods), diags);
    const link::Result r =
        amiga ? link::link(lib, prog, {"plat_start"}, link::Layout::Sections, "; generated by basicc (target amiga)\n", diags)
              : link::link(lib, prog, {"plat_start"}, link::Layout::Plain, "; generated by basicc (target m68k-test)\n        ORG     $1000\n", diags);
    return diags.empty() ? r.source : "";
}

}  // namespace basic
