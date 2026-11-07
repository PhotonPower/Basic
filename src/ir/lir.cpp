#include "lir.hpp"

namespace basic::lir {

namespace {

std::string reg(const Reg& r) {
    return (r.type == VT::F64 ? "f" : r.type == VT::Str ? "s" : r.type == VT::Addr ? "a" : "t") + std::to_string(r.id);
}

std::string hex(std::uint64_t v) {
    static const char* d = "0123456789ABCDEF";
    std::string s = "$";
    for (int i = 15; i >= 0; --i) s += d[(v >> (i * 4)) & 15];
    return s;
}

}  // namespace

std::string dump(const Program& p) {
    std::string o;
    for (const Function& f : p.functions) {
        o += "function " + f.name + "(";
        for (std::size_t k = 0; k < f.params.size(); ++k) o += (k ? ", " : "") + reg(f.params[k]);
        o += ")\n";
        for (const Instr& i : f.code) {
            auto var = [&](int v) { return p.hir->vars[static_cast<std::size_t>(v)].display(); };
            switch (i.kind) {
                case Kind::ConstI: o += "  " + reg(i.dst) + " = " + std::to_string(i.imm) + "\n"; break;
                case Kind::ConstF: o += "  " + reg(i.dst) + " = " + hex(i.fimm) + "\n"; break;
                case Kind::ConstS: o += "  " + reg(i.dst) + " = \"" + i.text + "\"\n"; break;
                case Kind::Load: o += "  " + reg(i.dst) + " = ld " + var(i.var) + "\n"; break;
                case Kind::Store: o += "  st " + var(i.var) + ", " + reg(i.a) + "\n"; break;
                case Kind::Op:
                    o += "  " + reg(i.dst) + " = " + i.op + " " + reg(i.a) + (i.b.id >= 0 ? ", " + reg(i.b) : std::string()) + "\n";
                    break;
                case Kind::Cmp: o += "  " + reg(i.dst) + " = cmp." + i.op + " " + reg(i.a) + ", " + reg(i.b) + "\n"; break;
                case Kind::RtCall:
                case Kind::Call: {
                    std::string c = i.kind == Kind::RtCall ? "rtcall " + i.op : "call " + p.hir->routines[static_cast<std::size_t>(i.routine)].name;
                    for (std::size_t k = 0; k < i.args.size(); ++k) c += (k ? ", " : " ") + reg(i.args[k]);
                    o += "  " + (i.dst.id >= 0 ? reg(i.dst) + " = " : std::string()) + c + "\n";
                    break;
                }
                case Kind::Jump: o += "  br L" + std::to_string(i.label) + "\n"; break;
                case Kind::BranchZ: o += "  br.z " + reg(i.a) + ", L" + std::to_string(i.label) + "\n"; break;
                case Kind::BranchNZ: o += "  br.nz " + reg(i.a) + ", L" + std::to_string(i.label) + "\n"; break;
                case Kind::Label: o += "L" + std::to_string(i.label) + ":\n"; break;
                case Kind::Return: o += "  ret" + (i.a.id >= 0 ? " " + reg(i.a) : std::string()) + "\n"; break;
                case Kind::Save: o += "  save " + var(i.var) + "\n"; break;
                case Kind::Clear: o += "  clear " + var(i.var) + "\n"; break;
                case Kind::Restore: o += "  restore " + var(i.var) + "\n"; break;
                case Kind::PrintStr: o += "  print \"" + i.text + "\"\n"; break;
                case Kind::Line: o += "  stmt " + std::to_string(i.imm) + "\n"; break;
                case Kind::Addr: o += "  " + reg(i.dst) + " = addr " + var(i.var) + "\n"; break;
                case Kind::LoadAt: o += "  " + reg(i.dst) + " = ld." + hir::typeName(i.mem) + " [" + reg(i.a) + "]\n"; break;
                case Kind::StoreAt: o += "  st." + std::string(hir::typeName(i.mem)) + " [" + reg(i.a) + "], " + reg(i.b) + "\n"; break;
                case Kind::Bind: o += "  bind " + var(i.var) + ", " + reg(i.a) + "\n"; break;
            }
        }
    }
    return o;
}

}  // namespace basic::lir
