#include "backend/z80/z80_backend.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>

#include "ir/lower.hpp"
#include "link/embedded.hpp"

namespace basic {

namespace {

using lir::Instr;
using lir::Kind;
using lir::Reg;
using lir::VT;

struct Unsupported {
    int line;
    std::string what;
};

class Z80Gen {
public:
    Z80Gen(const lir::Program& p, Diagnostics& d) : p_(p), diags_(d) {}

    link::Program run() {
        for (std::size_t v = 0; v < p_.hir->vars.size(); ++v) {
            out_.bss += "v" + std::to_string(v) + ":\n        DS      8                       ; " + p_.hir->vars[v].display() + "\n";
            if (indirect(static_cast<int>(v)))  // VAR parameter: the pointer to the current storage
                out_.data += "p" + std::to_string(v) + ":\n        DW      v" + std::to_string(v) + "\n";
        }
        out_.bss += "prg_ret:\n        DS      8\nprg_tmp:\n        DS      2\n";
        for (const lir::Function& f : p_.functions) {
            try {
                function(f);
            } catch (const Unsupported& u) {
                diags_.push_back({u.line, 0, u.what + " not supported by the Z80 backend yet"});
                return out_;
            }
        }
        out_.provides.push_back("main");
        if (p_.onError) {  // descriptors of the saved variables for the undo of RESUME
            for (int v : savedVars_) {
                const bool str = varType(v) == hir::Type::Str;
                out_.data += "vd" + std::to_string(v) + ":\n        DW      " + var(v) + "," + (indirect(v) ? "p" + std::to_string(v) : std::string("0")) +
                             "," + (str ? "rt_str_free,rt_str_relink" : "0,0") + "\n";
                if (str) {
                    uses_.insert("rt_str_free");
                    uses_.insert("rt_str_relink");
                }
            }
        }
        // the DATA table for rt_data: READ, RESTORE, MAT READ
        if (uses_.count("rt_data_str") + uses_.count("rt_data_num") + uses_.count("rt_data_restore") + uses_.count("rt_mat_read") != 0) {
            const auto& items = p_.hir->data;
            out_.data += "prg_data_count:\n        DW      " + std::to_string(items.size()) + "\nprg_data:\n";
            std::string texts;
            for (std::size_t k = 0; k < items.size(); ++k) {
                const std::string l = "d" + std::to_string(k);
                out_.data += "        DW      " + (items[k].text.empty() ? std::string("0") : l) + "," + std::to_string(items[k].text.size()) +
                             "\n        DB      " + (items[k].quoted ? "1" : "0") + ",0\n";
                if (!items[k].text.empty()) texts += l + ":\n        DB      " + dbBytes(items[k].text) + "\n";
            }
            out_.data += texts;
            out_.provides.push_back("prg_data");
            out_.provides.push_back("prg_data_count");
        }
        if (uses_.count("rt_resume") != 0) {  // RESUME label: per HIR label its address and routine
            out_.bss += "prg_rlab:\n        DS      " + std::to_string(4 * std::max<std::size_t>(1, p_.hir->labels.size())) + "\n";
            out_.provides.push_back("prg_rlab");
        }
        if (uses_.count("rt_type") + uses_.count("rt_swapptr") != 0) {  // TYPE(p), SWAP *p: address and TYPE number
            out_.data += "prg_vtypes_count:\n        DW      " + std::to_string(p_.hir->vars.size()) + "\nprg_vtypes:\n";
            for (std::size_t v = 0; v < p_.hir->vars.size(); ++v) {
                const hir::Var& hv = p_.hir->vars[v];
                int code = hv.type == hir::Type::Float ? 0 : hv.type == hir::Type::Str ? 1 : hv.type == hir::Type::Bool ? 3
                         : hv.type == hir::Type::Word ? 8 : hv.type == hir::Type::Byte ? 9 : 2;
                if (hv.array) code += 4;  // arrays: 4 5 6 7 12 13
                out_.data += "        DW      v" + std::to_string(v) + "\n        DB      " + std::to_string(code) + "\n";
            }
            out_.provides.push_back("prg_vtypes");
            out_.provides.push_back("prg_vtypes_count");
        }
        out_.uses.assign(uses_.begin(), uses_.end());
        return out_;
    }

private:
    const lir::Program& p_;
    Diagnostics& diags_;
    link::Program out_;
    std::set<std::string> uses_;
    const lir::Function* f_ = nullptr;
    int fn_ = 0;  // number of the function (labels)
    int line_ = 0;
    int strings_ = 0;

    [[noreturn]] void unsupported(const std::string& what) { throw Unsupported{line_, what}; }

    void line(const std::string& s) { out_.code += "        " + s + "\n"; }
    std::string label(int l) const { return "f" + std::to_string(fn_) + "_L" + std::to_string(l); }
    std::string fname(int routine) const { return routine < 0 ? std::string("main") : "r" + std::to_string(routine); }
    hir::Type varType(int v) const { return p_.hir->vars[static_cast<std::size_t>(v)].type; }
    static std::string var(int v) { return "v" + std::to_string(v); }
    bool indirect(int v) const { return p_.indirect[static_cast<std::size_t>(v)]; }
    // HL = address of the storage of v (through the pointer of a VAR-bound variable)
    std::string varAddr(int v) const { return indirect(v) ? "LD      HL,(p" + std::to_string(v) + ")" : "LD      HL," + var(v); }
    int local_ = 0;
    // per register of the function: the label of its constant (no slot), or its slot number
    std::vector<std::string> constCell_;
    std::vector<int> slot_;
    int slots_ = 0;
    std::map<std::string, std::string> cells_;  // constant contents -> label (shared by all functions)
    std::set<int> savedVars_;  // variables with a Save (ON ERROR: descriptors for the save log)
    // ON ERROR: the record of the running statement (12 bytes below the slots)
    bool hasRecord() const { return p_.onError && !f_->deffn; }
    void recToHL() {
        line("PUSH    IX");
        line("POP     HL");
        line("LD      BC," + std::to_string(-8 * slots_ - 12));
        line("ADD     HL,BC");
    }

    bool isConst(const Reg& r) const { return !constCell_[static_cast<std::size_t>(r.id)].empty(); }
    int offset(const Reg& r) const { return -8 * (slot_[static_cast<std::size_t>(r.id)] + 1); }
    static std::string ix(int o) { return o < 0 ? "(IX-" + std::to_string(-o) + ")" : "(IX+" + std::to_string(o) + ")"; }
    bool near(const Reg& r) const { return !isConst(r) && offset(r) >= -128; }  // IX+d reaches both bytes

    // rr (HL, DE, BC) = address of the slot of r or of its constant cell, the other pairs kept;
    // false if the slot is too far for rt_ix* (then only slotToHL)
    bool addrTo(const char* rr, const Reg& r) {
        if (isConst(r)) {
            line(std::string("LD      ") + rr + "," + constCell_[static_cast<std::size_t>(r.id)]);
            return true;
        }
        if (!near(r)) return false;
        const std::string pair(rr);
        uses_.insert("rt_ix" + std::string(pair == "HL" ? "hl" : pair == "DE" ? "de" : "bc"));
        line("CALL    rt_ix" + std::string(pair == "HL" ? "hl" : pair == "DE" ? "de" : "bc"));
        line("DB      " + std::to_string(256 + offset(r)));
        return true;
    }
    // rr = the value of a string or address register (16 bits), the other pairs kept; false if far
    bool valTo(const char* rr, const Reg& r) {
        const std::string pair(rr);
        if (isConst(r)) {
            line("LD      " + pair + ",(" + constCell_[static_cast<std::size_t>(r.id)] + ")");
            return true;
        }
        if (!near(r)) return false;
        line(std::string("LD      ") + pair[1] + "," + ix(offset(r)));
        line(std::string("LD      ") + pair[0] + "," + ix(offset(r) + 1));
        return true;
    }
    // rr = r as a runtime routine gets it (numbers by address, strings and addresses by value)
    bool argTo(const char* rr, const Reg& r) { return r.type == VT::Str || r.type == VT::Addr ? valTo(rr, r) : addrTo(rr, r); }

    // HL = address of the slot of r (IX - 8 * (slot + 1); a constant: its cell); far: uses BC
    void slotToHL(const Reg& r) {
        if (addrTo("HL", r)) return;
        line("PUSH    IX");
        line("POP     HL");
        line("LD      BC," + std::to_string(offset(r)));
        line("ADD     HL,BC");
    }
    // HL = the argument as the runtime gets it: numbers by address, strings and addresses by value
    void argToHL(const Reg& r) {
        if (r.type != VT::Str && r.type != VT::Addr) {
            slotToHL(r);
        } else if (isConst(r)) {
            line("LD      HL,(" + constCell_[static_cast<std::size_t>(r.id)] + ")");
        } else if (near(r)) {
            line("LD      L," + ix(offset(r)));
            line("LD      H," + ix(offset(r) + 1));
        } else {
            slotToHL(r);
            line("LD      A,(HL)");
            line("INC     HL");
            line("LD      H,(HL)");
            line("LD      L,A");
        }
    }
    // stores the 16-bit value in HL into the slot of r
    void storeHL(const Reg& r) {
        if (near(r)) {
            line("LD      " + ix(offset(r)) + ",L");
            line("LD      " + ix(offset(r) + 1) + ",H");
            return;
        }
        line("EX      DE,HL");
        slotToHL(r);
        line("LD      (HL),E");
        line("INC     HL");
        line("LD      (HL),D");
    }
    // the label of an 8-byte constant cell with this contents (DB/DW operands)
    std::string cell(const std::string& contents) {
        const auto it = cells_.find(contents);
        if (it != cells_.end()) return it->second;
        const std::string l = "k" + std::to_string(cells_.size());
        cells_.emplace(contents, l);
        out_.data += l + ":\n        " + contents + "\n";
        return l;
    }
    static std::string dbOf(std::uint64_t v, int n) {
        std::string r = "DB      ";
        for (int k = 0; k < 8; ++k) r += (k ? "," : "") + std::to_string(k < n ? static_cast<unsigned>((v >> (8 * k)) & 0xFF) : 0u);
        return r;
    }
    // registers set once by a constant (number, string, address of a variable or routine) and nowhere
    // else get a cell instead of a slot; the other registers are numbered without gaps
    std::string lastRt_;  // the previous runtime call (CLS is rt_print_cls + rt_gfx_cmd)

    void planRegisters(const lir::Function& f) {
        const std::size_t n = static_cast<std::size_t>(f.regs);
        std::vector<int> defs(n, 0);
        std::vector<const Instr*> def(n, nullptr);
        for (const Instr& i : f.code)
            if (i.dst.id >= 0) {
                ++defs[static_cast<std::size_t>(i.dst.id)];
                def[static_cast<std::size_t>(i.dst.id)] = &i;
            }
        for (const Reg& r : f.params) defs[static_cast<std::size_t>(r.id)] += 2;
        constCell_.assign(n, "");
        slot_.assign(n, -1);
        slots_ = 0;
        for (std::size_t k = 0; k < n; ++k) {
            const Instr* i = def[k];
            if (defs[k] == 1 && i != nullptr) {
                if (i->kind == Kind::ConstI) constCell_[k] = cell(dbOf(static_cast<std::uint32_t>(i->imm), 4));
                else if (i->kind == Kind::ConstF) constCell_[k] = cell(dbOf(i->fimm, 8));
                else if (i->kind == Kind::ConstS) constCell_[k] = cell("DW      " + constString(i->text) + ",0,0,0");
                else if (i->kind == Kind::Addr && !indirect(i->var)) constCell_[k] = cell("DW      " + var(i->var) + ",0,0,0");
                else if (i->kind == Kind::RoutineAddr) constCell_[k] = cell("DW      " + fname(i->routine) + ",0,0,0");
            }
        }
        shareSlots(f);
    }

    // slots by lifetime: registers whose live ranges (as intervals over the code, from liveness on the
    // flow graph) do not overlap share a slot. A register written and read by the same instruction
    // conflicts with it (no runtime routine gets its result on top of an operand). With ON ERROR a
    // register live at the start of a statement stays live to its end (RESUME repeats it).
    void shareSlots(const lir::Function& f) {
        const std::size_t n = static_cast<std::size_t>(f.regs), m = f.code.size(), W = (n + 63) / 64;
        std::map<int, std::size_t> at;  // label -> index
        for (std::size_t i = 0; i < m; ++i)
            if (f.code[i].kind == Kind::Label) at[f.code[i].label] = i;
        auto counted = [&](const Reg& r) { return r.id >= 0 && constCell_[static_cast<std::size_t>(r.id)].empty(); };
        std::vector<std::uint64_t> in(m * W, 0), use(m * W, 0);
        std::vector<int> def(m, -1);
        auto bit = [&](std::vector<std::uint64_t>& v, std::size_t i, int r) { v[i * W + static_cast<std::size_t>(r) / 64] |= std::uint64_t{1} << (r % 64); };
        for (std::size_t i = 0; i < m; ++i) {
            const Instr& x = f.code[i];
            for (const Reg* r : {&x.a, &x.b})
                if (counted(*r)) bit(use, i, r->id);
            for (const Reg& r : x.args)
                if (counted(r)) bit(use, i, r.id);
            if (counted(x.dst)) def[i] = x.dst.id;
        }
        auto succ = [&](std::size_t i, std::size_t* s2) -> int {  // successors of i
            const Instr& x = f.code[i];
            int c = 0;
            if (x.kind == Kind::Jump || x.kind == Kind::BranchZ || x.kind == Kind::BranchNZ || x.kind == Kind::OpOvf) {
                const auto it = at.find(x.label);
                if (it != at.end()) s2[c++] = it->second;
            }
            if (x.kind != Kind::Jump && x.kind != Kind::Return && i + 1 < m) s2[c++] = i + 1;
            return c;
        };
        std::vector<std::uint64_t> out(W);
        for (bool changed = true; changed;) {
            changed = false;
            for (std::size_t i = m; i-- > 0;) {
                std::fill(out.begin(), out.end(), 0);
                std::size_t s2[2];
                const int c = succ(i, s2);
                for (int k = 0; k < c; ++k)
                    for (std::size_t w = 0; w < W; ++w) out[w] |= in[s2[k] * W + w];
                if (def[i] >= 0) out[static_cast<std::size_t>(def[i]) / 64] &= ~(std::uint64_t{1} << (def[i] % 64));
                for (std::size_t w = 0; w < W; ++w) {
                    const std::uint64_t v = out[w] | use[i * W + w];
                    if (v != in[i * W + w]) {
                        in[i * W + w] = v;
                        changed = true;
                    }
                }
            }
        }
        // intervals: the instructions where a register is live on entry or written
        std::vector<long> lo(n, -1), hi(n, -1);
        auto occupy = [&](std::size_t r, long i) {
            if (lo[r] < 0 || i < lo[r]) lo[r] = i;
            if (i > hi[r]) hi[r] = i;
        };
        for (const Reg& r : f.params) occupy(static_cast<std::size_t>(r.id), 0);
        for (std::size_t i = 0; i < m; ++i) {
            if (def[i] >= 0) occupy(static_cast<std::size_t>(def[i]), static_cast<long>(i));
            for (std::size_t w = 0; w < W; ++w)
                for (std::uint64_t v = in[i * W + w]; v != 0; v &= v - 1) occupy(w * 64 + static_cast<std::size_t>(__builtin_ctzll(v)), static_cast<long>(i));
        }
        if (hasRecord()) {  // a statement repeated by RESUME: what is live at its start, to its end
            std::size_t start = m;
            for (std::size_t i = 0; i <= m; ++i) {
                if (i < m && f.code[i].kind != Kind::Line) continue;
                if (start < m)
                    for (std::size_t w = 0; w < W; ++w)
                        for (std::uint64_t v = in[start * W + w]; v != 0; v &= v - 1)
                            occupy(w * 64 + static_cast<std::size_t>(__builtin_ctzll(v)), static_cast<long>(i) - 1);
                start = i;
            }
        }
        std::vector<std::size_t> order;
        for (std::size_t r = 0; r < n; ++r)
            if (constCell_[r].empty() && lo[r] >= 0) order.push_back(r);
        std::sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) { return lo[x] != lo[y] ? lo[x] < lo[y] : x < y; });
        std::vector<long> busyTo;  // per slot: the last instruction of its current register
        for (std::size_t r : order) {
            std::size_t k = 0;
            while (k < busyTo.size() && busyTo[k] >= lo[r]) ++k;
            if (k == busyTo.size()) busyTo.push_back(hi[r]);
            else busyTo[k] = hi[r];
            slot_[r] = static_cast<int>(k);
        }
        slots_ = static_cast<int>(busyTo.size());
        for (std::size_t r = 0; r < n; ++r)  // never live (not used): a slot of its own is not needed
            if (constCell_[r].empty() && slot_[r] < 0) slot_[r] = 0;
        if (slots_ == 0 && n > 0) slots_ = 1;
    }
    // the descriptor of a string constant (shared by equal texts)
    std::string constString(const std::string& text) {
        const auto it = strConsts_.find(text);
        if (it != strConsts_.end()) return it->second;
        const std::string l = "c" + std::to_string(strings_++);
        out_.data += l + ":\n        DW      " + (text.empty() ? std::string("0") : l + "t") + "," + std::to_string(text.size()) + "\n";
        if (!text.empty()) out_.data += l + "t:\n        DB      " + dbBytes(text) + "\n";
        strConsts_.emplace(text, l);
        return l;
    }
    std::map<std::string, std::string> strConsts_;
    void bytesAt(const std::vector<std::uint8_t>& b) {  // HL -> the bytes, HL advanced
        for (std::size_t k = 0; k < b.size(); ++k) {
            line("LD      (HL)," + std::to_string(b[k]));
            if (k + 1 < b.size()) line("INC     HL");
        }
    }
    static std::string dbBytes(const std::string& t) {
        std::string r;
        for (std::size_t k = 0; k < t.size(); ++k) r += (k ? "," : "") + std::to_string(static_cast<unsigned char>(t[k]));
        return r;
    }

    // a runtime call: args in DE, BC, then rt_argp; a numeric result through HL, else returned in HL
    void call(const std::string& sym, const std::vector<Reg>& args, const Reg& dst) {
        uses_.insert(sym);
        for (std::size_t k = 2; k < args.size(); ++k) {
            uses_.insert("rt_argp");
            argToHL(args[k]);
            line("LD      (rt_argp+" + std::to_string(2 * (k - 2)) + "),HL");
        }
        const bool numeric = dst.id >= 0 && (dst.type == VT::I32 || dst.type == VT::F64);
        // all near or constant: straight into BC, DE, HL (rt_ix* keep the other pairs)
        const bool allNear = std::all_of(args.begin(), args.begin() + static_cast<long>(std::min<std::size_t>(args.size(), 2)),
                                         [&](const Reg& r) { return isConst(r) || near(r); }) &&
                             (!numeric || isConst(dst) || near(dst));
        if (sym == "rt_str_release" && near(args[0]) && !isConst(args[0])) {  // the slot as inline byte
            uses_.insert("rt_str_relx");
            line("CALL    rt_str_relx");
            line("DB      " + std::to_string(256 + offset(args[0])));
            return;
        }
        if (allNear) {
            if (args.size() >= 2) argTo("BC", args[1]);
            if (!args.empty()) argTo("DE", args[0]);
            if (numeric) addrTo("HL", dst);
            line("CALL    " + sym);
            if (dst.id >= 0 && !numeric) storeHL(dst);
            return;
        }
        // the second argument first (computing a far slot address uses BC), then the first one into
        // DE, the numeric result address into HL; constants directly
        const bool push1 = args.size() >= 2 && !isConst(args[1]);
        if (push1) {
            argToHL(args[1]);
            line("PUSH    HL");
        }
        if (!args.empty() && !isConst(args[0])) {
            argToHL(args[0]);
            line("EX      DE,HL");
        }
        if (numeric) slotToHL(dst);
        if (push1) line("POP     BC");
        auto direct = [&](const Reg& r, const char* rr) {
            const std::string& c = constCell_[static_cast<std::size_t>(r.id)];
            line(std::string("LD      ") + rr + "," + (r.type == VT::Str || r.type == VT::Addr ? "(" + c + ")" : c));
        };
        if (!args.empty() && isConst(args[0])) direct(args[0], "DE");
        if (args.size() >= 2 && isConst(args[1])) direct(args[1], "BC");
        line("CALL    " + sym);
        if (dst.id >= 0 && !numeric) storeHL(dst);
    }

    void function(const lir::Function& f) {
        f_ = &f;
        ++fn_;
        planRegisters(f);
        out_.code += "; ---- " + (f.routine < 0 ? std::string("main program") : f.name) + "\n" + fname(f.routine) + ":\n";
        line("PUSH    IX");
        line("LD      IX,0");
        line("ADD     IX,SP");
        const int frame = 8 * slots_ + (hasRecord() ? 12 : 0);
        if (frame > 0) {
            line("LD      HL," + std::to_string(-frame));
            line("ADD     HL,SP");
            line("LD      SP,HL");
        }
        if (hasRecord()) {  // own statement record; until the first statement a copy of the caller's
            uses_.insert("rt_err_ctx");
            recToHL();
            line("EX      DE,HL");
            line("LD      HL,(rt_err_ctx)");
            line("EX      DE,HL");
            line("LD      (HL),E");
            line("INC     HL");
            line("LD      (HL),D");
            line("INC     HL");
            line("LD      A,D");
            line("OR      E");
            const std::string l = "f" + std::to_string(fn_) + "_E";
            line("JR      Z," + l);
            line("EX      DE,HL");
            line("INC     HL");
            line("INC     HL");
            line("LD      BC,8");
            line("LDIR");
            line("EX      DE,HL");
            out_.code += l + ":\n";
            recToHL();
            line("LD      BC,10");
            line("ADD     HL,BC");
            line("LD      DE," + fname(f.routine));
            line("LD      (HL),E");
            line("INC     HL");
            line("LD      (HL),D");
            recToHL();
            line("LD      (rt_err_ctx),HL");
        }
        for (const auto& [hirLabel, lirLabel] : f.resumeLabels) {  // addresses for RESUME label
            line("LD      HL," + label(lirLabel));
            line("LD      (prg_rlab+" + std::to_string(4 * hirLabel) + "),HL");
            line("LD      HL," + fname(f.routine));
            line("LD      (prg_rlab+" + std::to_string(4 * hirLabel + 2) + "),HL");
        }
        for (std::size_t k = 0; k < f.params.size(); ++k) {  // incoming arguments: 8 bytes each from IX+4
            line("PUSH    IX");
            line("POP     HL");
            line("LD      BC," + std::to_string(4 + 8 * k));
            line("ADD     HL,BC");
            line("PUSH    HL");
            slotToHL(f.params[k]);
            line("EX      DE,HL");
            line("POP     HL");
            line("LD      BC,8");
            line("LDIR");
        }
        for (const Instr& i : f.code) instr(i);
    }

    void instr(const Instr& i) {
        if (i.dst.id >= 0 && isConst(i.dst)) return;  // the value lies in its constant cell
        switch (i.kind) {
            case Kind::ConstI: {
                slotToHL(i.dst);
                const auto v = static_cast<std::uint32_t>(i.imm);
                bytesAt({static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v >> 16),
                         static_cast<std::uint8_t>(v >> 24)});
                return;
            }
            case Kind::ConstF: {
                slotToHL(i.dst);
                std::vector<std::uint8_t> b;
                for (int k = 0; k < 8; ++k) b.push_back(static_cast<std::uint8_t>(i.fimm >> (8 * k)));
                bytesAt(b);
                return;
            }
            case Kind::ConstS:  // descriptor: address, length
                line("LD      HL," + constString(i.text));
                storeHL(i.dst);
                return;
            case Kind::Load: {
                const hir::Type t = varType(i.var);
                if (t == hir::Type::Str) {  // the descriptor of the variable
                    line(varAddr(i.var));
                    storeHL(i.dst);
                    return;
                }
                if (!addrTo("DE", i.dst)) {
                    slotToHL(i.dst);
                    line("EX      DE,HL");
                }
                line(varAddr(i.var));
                loadFromHL(t);
                return;
            }
            case Kind::LoadAt: {  // the element type from the address in slot a
                argToHL(i.a);
                line("PUSH    HL");
                slotToHL(i.dst);
                line("EX      DE,HL");
                line("POP     HL");
                loadFromHL(i.mem);
                return;
            }
            case Kind::StoreAt: {
                if (i.mem == hir::Type::Str) unsupported("string elements");
                argToHL(i.a);
                line("PUSH    HL");
                slotToHL(i.b);
                line("POP     DE");
                storeToDE(i.mem);
                return;
            }
            case Kind::Addr:
                line(varAddr(i.var));
                storeHL(i.dst);
                return;
            case Kind::Store: {
                const hir::Type t = varType(i.var);
                if (t == hir::Type::Str) unsupported("string variables");
                if (indirect(i.var)) {
                    line(varAddr(i.var));
                    line("PUSH    HL");
                    slotToHL(i.a);
                    line("POP     DE");
                } else {
                    slotToHL(i.a);
                    line("LD      DE," + var(i.var));
                }
                storeToDE(t);
                return;
            }
            default: break;
        }
        instr2(i);
    }

    // DE = destination slot, HL = source of type t: the value widened to 32 bits (Float: 8 bytes)
    void loadFromHL(hir::Type t) {
                switch (t) {
                    case hir::Type::Float: line("LD      BC,8"); line("LDIR"); return;
                    case hir::Type::Int: line("LD      BC,4"); line("LDIR"); return;
                    case hir::Type::Word:  // sign-extended
                        line("LD      BC,2");
                        line("LDIR");
                        line("DEC     HL");
                        line("LD      A,(HL)");
                        line("RLCA");
                        line("SBC     A,A");
                        line("LD      (DE),A");
                        line("INC     DE");
                        line("LD      (DE),A");
                        return;
                    case hir::Type::Byte:  // zero-extended
                        line("LDI");
                        line("XOR     A");
                        line("LD      (DE),A");
                        line("INC     DE");
                        line("LD      (DE),A");
                        line("INC     DE");
                        line("LD      (DE),A");
                        return;
                    default:  // Bool: sign-extended
                        line("LD      A,(HL)");
                        line("LD      (DE),A");
                        line("INC     DE");
                        line("RLCA");
                        line("SBC     A,A");
                        line("LD      (DE),A");
                        line("INC     DE");
                        line("LD      (DE),A");
                        line("INC     DE");
                        line("LD      (DE),A");
                        return;
                }
    }

    // HL = slot with the value, DE = destination of type t (the value already fits)
    void storeToDE(hir::Type t) {
        const int n = t == hir::Type::Float ? 8 : t == hir::Type::Int ? 4 : t == hir::Type::Word ? 2 : 1;
        line("LD      BC," + std::to_string(n));
        line("LDIR");
    }

    void instr2(const Instr& i) {
        switch (i.kind) {
            case Kind::Op: {
                if (i.op == "mov" && (i.dst.type == VT::Str || i.dst.type == VT::Addr)) {  // a 16-bit address
                    argToHL(i.a);
                    storeHL(i.dst);
                    return;
                }
                if (i.op == "mov" && (i.a.type == VT::Str || i.a.type == VT::Addr)) {  // VARPTR: the address as 32 bits
                    argToHL(i.a);
                    if (near(i.dst)) {
                        line("LD      " + ix(offset(i.dst)) + ",L");
                        line("LD      " + ix(offset(i.dst) + 1) + ",H");
                        line("LD      " + ix(offset(i.dst) + 2) + ",0");
                        line("LD      " + ix(offset(i.dst) + 3) + ",0");
                        return;
                    }
                    line("EX      DE,HL");
                    slotToHL(i.dst);
                    line("LD      (HL),E");
                    line("INC     HL");
                    line("LD      (HL),D");
                    line("INC     HL");
                    line("LD      (HL),0");
                    line("INC     HL");
                    line("LD      (HL),0");
                    return;
                }
                if (i.op == "mov" && i.dst.type == VT::F64) {  // 8 bytes
                    if (addrTo("DE", i.dst)) {
                        slotToHL(i.a);
                    } else {
                        slotToHL(i.dst);
                        line("PUSH    HL");
                        slotToHL(i.a);
                        line("POP     DE");
                    }
                    line("LD      BC,8");
                    line("LDIR");
                    return;
                }
                static const std::set<std::string> two = {"add", "sub", "and", "or", "xor"};
                if (two.count(i.op) != 0) call("zi_" + i.op, {i.a, i.b}, i.dst);
                else if (i.op == "not" || i.op == "neg" || i.op == "mov") call("zi_" + i.op, {i.a}, i.dst);
                else unsupported("operation " + i.op);
                return;
            }
            case Kind::OpOvf: {  // CF: the result does not fit 32 bits
                call("zi_" + i.op + "v", {i.a, i.b}, i.dst);
                line("JP      C," + label(i.label));
                return;
            }
            case Kind::Cmp: {
                static const std::map<std::string, int> codes = {{"eq", 0}, {"ne", 1}, {"lt", 2}, {"le", 3}, {"gt", 4}, {"ge", 5}};
                const auto it = codes.find(i.op);
                if (it == codes.end()) unsupported("comparison " + i.op);
                uses_.insert("zi_cmp_code");
                line("LD      A," + std::to_string(it->second));
                line("LD      (zi_cmp_code),A");
                call("zi_cmp", {i.a, i.b}, i.dst);
                return;
            }
            case Kind::RtCall:
                if (i.op == "rt_gfx_cmd" && lastRt_ == "rt_print_cls") {  // CLS: text and graphics share
                    lastRt_.clear();                                    // the screen (no graphics yet)
                    return;
                }
                lastRt_ = i.op;
                if (i.op == "rt_error") {  // ERROR n: the runtime takes the number in A
                    uses_.insert("rt_error");
                    argToHL(i.args[0]);
                    line("LD      A,(HL)");
                    line("CALL    rt_error");
                    return;
                }
                call(i.op, i.args, i.dst);
                return;
            case Kind::Call: {
                const std::size_t n = i.args.size();
                if (n > 0) {  // the arguments as 8-byte values on the stack, the first one on top
                    line("LD      HL," + std::to_string(-8 * static_cast<int>(n)));
                    line("ADD     HL,SP");
                    line("LD      SP,HL");
                    for (std::size_t k = 0; k < n; ++k) {
                        slotToHL(i.args[k]);
                        line("PUSH    HL");
                        line("LD      HL," + std::to_string(8 * k + 2));
                        line("ADD     HL,SP");
                        line("EX      DE,HL");
                        line("POP     HL");
                        line("LD      BC,8");
                        line("LDIR");
                    }
                }
                line("CALL    " + fname(i.routine));
                if (n > 0) {
                    line("LD      HL," + std::to_string(8 * n));
                    line("ADD     HL,SP");
                    line("LD      SP,HL");
                }
                if (i.dst.id >= 0 && i.dst.type == VT::Str) {  // a temporary of the caller takes the result
                    uses_.insert("rt_str_take_ret");
                    line("CALL    rt_str_take_ret");
                    storeHL(i.dst);
                } else if (i.dst.id >= 0) {
                    slotToHL(i.dst);
                    line("EX      DE,HL");
                    line("LD      HL,prg_ret");
                    line("LD      BC,8");
                    line("LDIR");
                }
                return;
            }
            case Kind::Jump: line("JP      " + label(i.label)); return;
            case Kind::BranchZ:
            case Kind::BranchNZ:
                if (near(i.a)) {
                    line("LD      A," + ix(offset(i.a)));
                    for (int k = 1; k < 4; ++k) line("OR      " + ix(offset(i.a) + k));
                } else {
                    slotToHL(i.a);
                    line("LD      A,(HL)");
                    for (int k = 0; k < 3; ++k) {
                        line("INC     HL");
                        line("OR      (HL)");
                    }
                }
                line(std::string(i.kind == Kind::BranchZ ? "JP      Z," : "JP      NZ,") + label(i.label));
                return;
            case Kind::Label: out_.code += label(i.label) + ":\n"; return;
            case Kind::Return:
                if (hasRecord()) {  // the caller's record is current again
                    recToHL();
                    line("LD      A,(HL)");
                    line("INC     HL");
                    line("LD      H,(HL)");
                    line("LD      L,A");
                    line("LD      (rt_err_ctx),HL");
                }
                if (i.a.id >= 0) {
                    slotToHL(i.a);
                    line("LD      DE,prg_ret");
                    line("LD      BC,8");
                    line("LDIR");
                }
                line("LD      SP,IX");
                line("POP     IX");
                line("RET");
                return;
            case Kind::PrintStr: {
                if (i.text.empty()) return;
                const std::string l = "s" + std::to_string(strings_++);
                out_.data += l + ":\n        DB      " + dbBytes(i.text) + "\n";
                uses_.insert("rt_print_str");
                line("LD      DE," + l);
                line("LD      BC," + std::to_string(i.text.size()));
                line("CALL    rt_print_str");
                return;
            }
            case Kind::Line:
                line_ = i.imm;
                line("; line " + std::to_string(i.imm));
                if (i.label >= 0 && hasRecord()) {  // the record: repeat, after, SP, IX (rt_err_line)
                    uses_.insert("rt_err_line");
                    line("CALL    rt_err_line");
                    line("DW      " + label(i.label) + "," + label(i.label2));
                }
                return;
            case Kind::Save: {  // the 8 bytes of the variable on the stack
                const std::string v = var(i.var);
                for (int off = 6; off >= 0; off -= 2) {
                    line("LD      HL,(" + v + "+" + std::to_string(off) + ")");
                    line("PUSH    HL");
                }
                if (varType(i.var) == hir::Type::Str) {  // the saved copy owns the block now
                    uses_.insert("rt_str_relink");
                    line("LD      HL,0");
                    line("ADD     HL,SP");
                    line("CALL    rt_str_relink");
                }
                if (indirect(i.var)) {  // pointer and flag (set by Bind), then the own storage again
                    const std::string pv = "p" + std::to_string(i.var);
                    line("LD      HL,(" + pv + ")");
                    line("PUSH    HL");
                    line("LD      HL,0");
                    line("PUSH    HL");
                    line("LD      HL," + v);
                    line("LD      (" + pv + "),HL");
                }
                if (p_.onError) {  // save log entry: descriptor, next entry
                    savedVars_.insert(i.var);
                    uses_.insert("rt_err_log");
                    line("LD      HL,(rt_err_log)");
                    line("PUSH    HL");
                    line("LD      HL,vd" + std::to_string(i.var));
                    line("PUSH    HL");
                    line("LD      HL,0");
                    line("ADD     HL,SP");
                    line("LD      (rt_err_log),HL");
                }
                return;
            }
            case Kind::Clear: {
                line("LD      HL," + var(i.var));
                bytesAt({0, 0, 0, 0, 0, 0, 0, 0});
                return;
            }
            case Kind::Restore: {
                const std::string v = var(i.var);
                const bool str = varType(i.var) == hir::Type::Str;
                std::string done;
                if (p_.onError) {  // the save log entry
                    line("POP     HL");
                    line("POP     HL");
                    line("LD      (rt_err_log),HL");
                }
                if (indirect(i.var)) {  // flag, pointer; bound to its own storage: drop the saved value
                    const std::string keep = "f" + std::to_string(fn_) + "_R" + std::to_string(local_++);
                    done = "f" + std::to_string(fn_) + "_R" + std::to_string(local_++);
                    line("POP     DE");
                    line("POP     HL");
                    line("LD      (p" + std::to_string(i.var) + "),HL");
                    line("LD      A,D");
                    line("OR      E");
                    line("JR      Z," + keep);
                    line("LD      HL,8");
                    line("ADD     HL,SP");
                    line("LD      SP,HL");
                    line("JP      " + done);
                    out_.code += keep + ":\n";
                }
                if (str) {  // free the current string, take back the saved one
                    uses_.insert("rt_str_free");
                    line("LD      HL," + v);
                    line("CALL    rt_str_free");
                }
                for (int off = 0; off <= 6; off += 2) {
                    line("POP     HL");
                    line("LD      (" + v + "+" + std::to_string(off) + "),HL");
                }
                if (str) {
                    uses_.insert("rt_str_relink");
                    line("LD      HL," + v);
                    line("CALL    rt_str_relink");
                }
                if (!done.empty()) out_.code += done + ":\n";
                return;
            }
            case Kind::Bind: {  // after Save: flag at (SP), saved pointer at 2(SP) (4 more with the log)
                const std::string l = "f" + std::to_string(fn_) + "_B" + std::to_string(local_++);
                const int o = p_.onError ? 4 : 0;
                argToHL(i.a);
                line("LD      (prg_tmp),HL");
                line("LD      HL," + std::to_string(2 + o));
                line("ADD     HL,SP");
                line("LD      E,(HL)");
                line("INC     HL");
                line("LD      D,(HL)");
                line("LD      HL,(prg_tmp)");
                line("OR      A");
                line("SBC     HL,DE");
                line("JR      NZ," + l);
                line("LD      HL," + std::to_string(o));  // already this storage: keep its value at the end (no restore)
                line("ADD     HL,SP");
                line("LD      (HL),1");
                if (varType(i.var) == hir::Type::Str) {
                    uses_.insert("rt_str_relink");
                    line("LD      HL,(prg_tmp)");
                    line("CALL    rt_str_relink");
                }
                out_.code += l + ":\n";
                line("LD      HL,(prg_tmp)");
                line("LD      (p" + std::to_string(i.var) + "),HL");
                return;
            }
            case Kind::RoutineAddr:
                line("LD      HL," + fname(i.routine));
                storeHL(i.dst);
                return;
            default: break;
        }
    }
};

}  // namespace

link::Program generateZ80(const lir::Program& program, Diagnostics& diags) { return Z80Gen(program, diags).run(); }

std::vector<std::uint8_t> amsdosHeader(const std::string& fileName, std::uint16_t load, std::uint16_t entry, std::size_t length) {
    std::vector<std::uint8_t> h(128, 0);
    std::string base = fileName.substr(fileName.find_last_of("/\\") == std::string::npos ? 0 : fileName.find_last_of("/\\") + 1);
    std::string ext;
    if (const auto dot = base.rfind('.'); dot != std::string::npos) {
        ext = base.substr(dot + 1);
        base.resize(dot);
    }
    for (int k = 0; k < 8; ++k)
        h[1 + static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(k < static_cast<int>(base.size()) ? std::toupper(static_cast<unsigned char>(base[static_cast<std::size_t>(k)])) : ' ');
    for (int k = 0; k < 3; ++k)
        h[9 + static_cast<std::size_t>(k)] = static_cast<std::uint8_t>(k < static_cast<int>(ext.size()) ? std::toupper(static_cast<unsigned char>(ext[static_cast<std::size_t>(k)])) : ' ');
    h[0x12] = 2;  // binary
    h[0x15] = static_cast<std::uint8_t>(load);
    h[0x16] = static_cast<std::uint8_t>(load >> 8);
    h[0x18] = static_cast<std::uint8_t>(length);
    h[0x19] = static_cast<std::uint8_t>(length >> 8);
    h[0x1A] = static_cast<std::uint8_t>(entry);
    h[0x1B] = static_cast<std::uint8_t>(entry >> 8);
    h[0x40] = static_cast<std::uint8_t>(length);
    h[0x41] = static_cast<std::uint8_t>(length >> 8);
    h[0x42] = static_cast<std::uint8_t>(length >> 16);
    unsigned sum = 0;
    for (int k = 0; k <= 0x42; ++k) sum += h[static_cast<std::size_t>(k)];
    h[0x43] = static_cast<std::uint8_t>(sum);
    h[0x44] = static_cast<std::uint8_t>(sum >> 8);
    return h;
}

std::uint16_t z80Origin(Z80Target target) { return target == Z80Target::Cpc ? 0x0100 : 0x4000; }

std::string buildZ80Program(const hir::Program& program, Z80Target target, Diagnostics& diags) {
    const lir::Program l = lower(program, diags);
    if (!diags.empty()) return "";
    const link::Program prog = generateZ80(l, diags);
    if (!diags.empty()) return "";
    std::vector<link::Module> mods =
        link::embeddedModulesOf({"runtime/z80", target == Z80Target::Cpc ? "platform/cpc" : "platform/z80-test"}, diags);
    const link::Library lib = link::makeLibrary(std::move(mods), diags);
    const link::Result r = link::link(lib, prog, {"plat_start"}, link::Layout::PlainZ80,
                                      std::string("; generated by basicc (target ") + (target == Z80Target::Cpc ? "cpc" : "z80-test") +
                                          ")\n        ORG     " + std::to_string(z80Origin(target)) + "\n",
                                      diags);
    return diags.empty() ? r.source : "";
}

}  // namespace basic
