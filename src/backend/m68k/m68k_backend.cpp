#include "m68k_backend.hpp"

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
            if (var.array) continue;
            out_.bss += varLabel(static_cast<int>(v)) + ":\n        DS.L    2                       ; " + var.display() + "\n";
        }
        for (const lir::Function& f : p_.functions) function(f);
        out_.provides.push_back("main");
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
    static std::string slot(const Reg& r, int offset = 0) { return std::to_string(-8 * (r.id + 1) + offset) + "(A6)"; }

    hir::Type varType(int v) const { return p_.hir->vars[static_cast<std::size_t>(v)].type; }

    // register arguments: an f64 takes a pair (D0:D1 or D2:D3), an integer the next data register,
    // a string descriptor the next of A0, A1
    void loadArgs(const std::vector<Reg>& args) {
        int next = 0, nextA = 0;
        for (const Reg& a : args) {
            if (a.type == VT::Str) {
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
        if (d.type == VT::Str) {
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
        line("LINK    A6,#" + std::to_string(-8 * f.regs));
        for (std::size_t k = 0; k < f.params.size(); ++k) {  // incoming arguments: 8 bytes each from 8(A6)
            const Reg& r = f.params[k];
            line("MOVE.L  " + std::to_string(8 + 8 * k) + "(A6)," + slot(r));
            if (r.type == VT::F64) line("MOVE.L  " + std::to_string(12 + 8 * k) + "(A6)," + slot(r, 4));
        }
        for (const Instr& i : f.code) instr(i);
    }

    void instr(const Instr& i) {
        switch (i.kind) {
            case Kind::ConstI: line("MOVE.L  #" + std::to_string(i.imm) + "," + slot(i.dst)); return;
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
            case Kind::Load: {
                const std::string v = varLabel(i.var);
                switch (varType(i.var)) {
                    case hir::Type::Str:  // the descriptor of the variable
                        line("LEA     " + v + ",A0");
                        line("MOVE.L  A0," + slot(i.dst));
                        return;
                    case hir::Type::Float:
                        line("MOVE.L  " + v + "," + slot(i.dst));
                        line("MOVE.L  " + v + "+4," + slot(i.dst, 4));
                        return;
                    case hir::Type::Word:
                        line("MOVE.W  " + v + ",D0");
                        line("EXT.L   D0");
                        break;
                    case hir::Type::Byte:
                        line("MOVEQ   #0,D0");
                        line("MOVE.B  " + v + ",D0");
                        break;
                    case hir::Type::Bool:
                        line("MOVE.B  " + v + ",D0");
                        line("EXT.W   D0");
                        line("EXT.L   D0");
                        break;
                    default: line("MOVE.L  " + v + ",D0"); break;
                }
                line("MOVE.L  D0," + slot(i.dst));
                return;
            }
            case Kind::Store: {
                const std::string v = varLabel(i.var);
                switch (varType(i.var)) {
                    case hir::Type::Float:
                        line("MOVE.L  " + slot(i.a) + "," + v);
                        line("MOVE.L  " + slot(i.a, 4) + "," + v + "+4");
                        return;
                    case hir::Type::Word: line("MOVE.W  " + slot(i.a, 2) + "," + v); return;
                    case hir::Type::Byte:
                    case hir::Type::Bool: line("MOVE.B  " + slot(i.a, 3) + "," + v); return;
                    default: line("MOVE.L  " + slot(i.a) + "," + v); return;
                }
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
                line("TST.L   " + slot(i.a));
                line("BEQ     .L" + std::to_string(i.label));
                return;
            case Kind::BranchNZ:
                line("TST.L   " + slot(i.a));
                line("BNE     .L" + std::to_string(i.label));
                return;
            case Kind::Label: out_.code += ".L" + std::to_string(i.label) + ":\n"; return;
            case Kind::Return:
                if (i.a.id >= 0) {
                    line("MOVE.L  " + slot(i.a) + ",D0");
                    if (i.a.type == VT::F64) line("MOVE.L  " + slot(i.a, 4) + ",D1");
                }
                line("UNLK    A6");
                line("RTS");
                return;
            case Kind::Save:
                line("MOVE.L  " + varLabel(i.var) + "+4,-(SP)");
                line("MOVE.L  " + varLabel(i.var) + ",-(SP)");
                if (varType(i.var) == hir::Type::Str) {  // the saved copy owns the block now
                    uses_.insert("rt_str_relink");
                    line("MOVE.L  SP,A0");
                    line("JSR     rt_str_relink");
                }
                return;
            case Kind::Clear:
                line("CLR.L   " + varLabel(i.var));
                line("CLR.L   " + varLabel(i.var) + "+4");
                return;
            case Kind::Restore:
                if (varType(i.var) == hir::Type::Str) {  // free the current string, take back the saved one
                    uses_.insert("rt_str_free");
                    line("LEA     " + varLabel(i.var) + ",A0");
                    line("JSR     rt_str_free");
                }
                line("MOVE.L  (SP)+," + varLabel(i.var));
                line("MOVE.L  (SP)+," + varLabel(i.var) + "+4");
                if (varType(i.var) == hir::Type::Str) {
                    line("LEA     " + varLabel(i.var) + ",A0");
                    line("JSR     rt_str_relink");
                }
                return;
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
            case Kind::Line: line("; line " + std::to_string(i.imm)); return;
        }
    }
};

}  // namespace

link::Program generateM68k(const lir::Program& program) {
    Gen g(program);
    return g.run();
}

}  // namespace basic
