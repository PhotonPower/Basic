#include "backend/z80/z80_backend.hpp"

#include <algorithm>
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
    std::set<int> savedVars_;  // variables with a Save (ON ERROR: descriptors for the save log)
    // ON ERROR: the record of the running statement (12 bytes below the slots)
    bool hasRecord() const { return p_.onError && !f_->deffn; }
    void recToHL() {
        line("PUSH    IX");
        line("POP     HL");
        line("LD      BC," + std::to_string(-8 * f_->regs - 12));
        line("ADD     HL,BC");
    }

    // HL = address of the slot of r (IX - 8 * (id + 1)); uses BC
    void slotToHL(const Reg& r) {
        line("PUSH    IX");
        line("POP     HL");
        line("LD      BC," + std::to_string(-8 * (r.id + 1)));
        line("ADD     HL,BC");
    }
    // HL = the argument as the runtime gets it: numbers by address, strings and addresses by value
    void argToHL(const Reg& r) {
        slotToHL(r);
        if (r.type == VT::Str || r.type == VT::Addr) {
            line("LD      A,(HL)");
            line("INC     HL");
            line("LD      H,(HL)");
            line("LD      L,A");
        }
    }
    // stores the 16-bit value in HL into the slot of r
    void storeHL(const Reg& r) {
        line("EX      DE,HL");
        slotToHL(r);
        line("LD      (HL),E");
        line("INC     HL");
        line("LD      (HL),D");
    }
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
        if (args.size() >= 2) {
            argToHL(args[1]);
            line("PUSH    HL");
        }
        if (!args.empty()) {
            argToHL(args[0]);
            line("PUSH    HL");
        }
        const bool numeric = dst.id >= 0 && (dst.type == VT::I32 || dst.type == VT::F64);
        if (numeric) slotToHL(dst);
        if (!args.empty()) line("POP     DE");
        if (args.size() >= 2) line("POP     BC");
        line("CALL    " + sym);
        if (dst.id >= 0 && !numeric) storeHL(dst);
    }

    void function(const lir::Function& f) {
        f_ = &f;
        ++fn_;
        out_.code += "; ---- " + (f.routine < 0 ? std::string("main program") : f.name) + "\n" + fname(f.routine) + ":\n";
        line("PUSH    IX");
        line("LD      IX,0");
        line("ADD     IX,SP");
        const int frame = 8 * f.regs + (hasRecord() ? 12 : 0);
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
            case Kind::ConstS: {  // descriptor: address, length
                const std::string l = "c" + std::to_string(strings_++);
                out_.data += l + ":\n        DW      " + (i.text.empty() ? std::string("0") : l + "t") + "," + std::to_string(i.text.size()) + "\n";
                if (!i.text.empty()) out_.data += l + "t:\n        DB      " + dbBytes(i.text) + "\n";
                line("LD      HL," + l);
                storeHL(i.dst);
                return;
            }
            case Kind::Load: {
                const hir::Type t = varType(i.var);
                if (t == hir::Type::Str) {  // the descriptor of the variable
                    line(varAddr(i.var));
                    storeHL(i.dst);
                    return;
                }
                slotToHL(i.dst);
                line("EX      DE,HL");
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
                if (i.op == "mov" && i.dst.type == VT::F64) {  // 8 bytes
                    slotToHL(i.dst);
                    line("PUSH    HL");
                    slotToHL(i.a);
                    line("POP     DE");
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
                slotToHL(i.a);
                line("LD      A,(HL)");
                for (int k = 0; k < 3; ++k) {
                    line("INC     HL");
                    line("OR      (HL)");
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
                if (i.label >= 0 && hasRecord()) {  // the record: repeat, after, SP, IX
                    line("LD      (prg_tmp),SP");
                    recToHL();
                    line("INC     HL");
                    line("INC     HL");
                    for (const std::string& v : {label(i.label), label(i.label2)}) {
                        line("LD      DE," + v);
                        line("LD      (HL),E");
                        line("INC     HL");
                        line("LD      (HL),D");
                        line("INC     HL");
                    }
                    line("LD      DE,(prg_tmp)");
                    line("LD      (HL),E");
                    line("INC     HL");
                    line("LD      (HL),D");
                    line("INC     HL");
                    line("PUSH    IX");
                    line("POP     DE");
                    line("LD      (HL),E");
                    line("INC     HL");
                    line("LD      (HL),D");
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
                                          ")\n        ORG     &4000\n",
                                      diags);
    return diags.empty() ? r.source : "";
}

}  // namespace basic
