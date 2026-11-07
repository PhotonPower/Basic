#include "interp.hpp"

#include <cstring>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "rt_pc.hpp"
#include "rt_ref.h"

namespace basic {

namespace {

using hir::Op;
using hir::SKind;
using hir::Type;

// Error numbers determined on the original (probe9, docs/language.md).
constexpr int kErrDimTwice = 14;     // redimensioned array
constexpr int kErrNotDimmed = 15;    // array undimensioned
constexpr int kErrDimRange = 17;     // dim out of range
constexpr int kErrIndices = 18;      // wrong # of indices
constexpr int kErrOutOfData = 34;    // out of data
constexpr int kErrNotNumeric = 35;   // data not numeric


using RuntimeError = brt::Error;

// Errors of the program structure at run time that have no GFA number (RESUME outside the error
// routine, a FUNCTION left without RETURN).
struct FatalError {
    std::string message;
};

// ---- values ------------------------------------------------------------------------------------
struct Value {
    Type type = Type::Int;  // Int (all integer types), Float or Str
    std::int32_t i = 0;
    f64 f = 0;
    std::string s;

    static Value ofInt(std::int32_t v) {
        Value x;
        x.i = v;
        return x;
    }
    static Value ofFloat(f64 v) {
        Value x;
        x.type = Type::Float;
        x.f = v;
        return x;
    }
    static Value ofStr(std::string v) {
        Value x;
        x.type = Type::Str;
        x.s = std::move(v);
        return x;
    }
};

struct Array {
    bool dimmed = false;
    std::vector<std::int32_t> upper;  // per dimension
    std::vector<std::int32_t> ints;
    std::vector<f64> floats;
    std::vector<std::string> strs;
};

struct Slot {
    std::int32_t i = 0;
    f64 f = 0;
    std::string s;
    std::shared_ptr<Array> arr;
    int alias = -1;  // VAR parameter: the variable it stands for (probe10: a true reference)
};

// ---- control flow signals ----------------------------------------------------------------------
struct ExitLoop {};
struct GotoSignal {
    int label;
};
struct ReturnSignal {
    Value value;
};
struct ResumeSignal {
    int mode;  // 0 retry, 1 next, 2 label
    int label;
};
struct EndProgram {};
struct StepLimit {};

using brt::check;

std::int32_t toI32(f64 v) { return brt::toInt(v); }
f64 ofI32(std::int32_t v) { return brt::flt(v); }

class Interp {
public:
    Interp(const hir::Program& p, const RunOptions& o) : p_(p), opt_(o), slots_(p.vars.size()) {
        rt_randomize(o.seed);
        kbd_.in = o.input;
        if (o.live != nullptr)
            kbd_.flush = [this] {
                *opt_.live << out_.buf;
                opt_.live->flush();
                out_.buf.clear();
            };
    }

    RunResult run() {
        RunResult r;
        try {
            execTop(p_.main);
        } catch (const EndProgram&) {
        } catch (const RuntimeError& e) {
            r.ok = false;
            r.error = e.code;
            r.line = line_;
            r.message = errorText(e.code);
        } catch (const brt::Unhandled& e) {
            r.ok = false;
            r.error = e.code;
            r.line = line_;
            r.message = errorText(e.code);
        } catch (const brt::MemoryFault& m) {
            r.ok = false;
            r.line = line_;
            char buf[64];
            std::snprintf(buf, sizeof buf, "memory access outside the BASIC data at $%08X", m.address);
            r.message = buf;
        } catch (const StepLimit&) {
            r.ok = false;
            r.line = line_;
            r.message = "step limit reached";
        } catch (const GotoSignal&) {
            r.ok = false;
            r.line = line_;
            r.message = "GOTO target not reachable";
        } catch (const ExitLoop&) {
            r.ok = false;
            r.line = line_;
            r.message = "EXIT IF outside a loop";
        } catch (const ResumeSignal&) {
            r.ok = false;
            r.line = line_;
            r.message = "RESUME outside the error routine";
        } catch (const FatalError& f) {
            r.ok = false;
            r.line = line_;
            r.message = f.message;
        }
        r.output = std::move(out_.buf);
        return r;
    }

private:
    const hir::Program& p_;
    RunOptions opt_;
    std::vector<Slot> slots_;
    brt::Output out_;
    brt::Files files_;
    brt::Keyboard kbd_;
    brt::Memory mem_;
    int line_ = 0;
    std::uint64_t steps_ = 0;
    int err_ = 0;
    int onError_ = -1;
    int handlerDepth_ = 0;
    int base_ = 0;
    int matBase_ = 1;  // MAT BASE
    std::size_t dataPos_ = 0;
    struct Frame;
    std::vector<Frame*> frames_;

    // ---- output ---------------------------------------------------------------------------------
    void putValue(const Value& v, brt::Output& out) {
        if (v.type == Type::Str) out.value(v.s);
        else if (v.type == Type::Float) out.value(v.f);
        else out.value(v.i);
    }

    // ---- variables ------------------------------------------------------------------------------
    // The storage of a variable; a VAR parameter leads to the caller's variable.
    int resolve(int var) const {
        while (slots_[static_cast<std::size_t>(var)].alias >= 0) var = slots_[static_cast<std::size_t>(var)].alias;
        return var;
    }
    Slot& slot(int var) { return slots_[static_cast<std::size_t>(resolve(var))]; }
    Slot& raw(int var) { return slots_[static_cast<std::size_t>(var)]; }
    const hir::Var& var(int v) const { return p_.vars[static_cast<std::size_t>(v)]; }

    Value load(int v) {
        const Slot& s = slot(v);
        switch (var(v).type) {
            case Type::Str: return Value::ofStr(s.s);
            case Type::Float: return Value::ofFloat(s.f);
            default: return Value::ofInt(s.i);
        }
    }

    // Stores a value that already has the variable's type (conversions are explicit in the HIR).
    void store(int v, Value x) {
        Slot& s = slot(v);
        switch (var(v).type) {
            case Type::Str: s.s = std::move(x.s); break;
            case Type::Float: s.f = x.f; break;
            default: s.i = x.i; break;
        }
    }

    Array& array(int v, bool mustBeDimmed = true) {
        Slot& s = slot(v);
        if (!s.arr) s.arr = std::make_shared<Array>();
        if (mustBeDimmed && !s.arr->dimmed) throw RuntimeError{kErrNotDimmed};
        return *s.arr;
    }

    std::size_t elemIndex(const hir::Expr& e) {
        Array& a = array(e.var);
        if (e.args.size() != a.upper.size()) throw RuntimeError{kErrIndices};
        std::size_t index = 0, mul = 1;
        for (std::size_t k = 0; k < e.args.size(); ++k) {
            const std::int32_t ix = eval(*e.args[k]).i;
            if (ix < base_ || ix > a.upper[k]) throw RuntimeError{RT_ERR_INDEX};
            index += static_cast<std::size_t>(ix - base_) * mul;
            mul *= static_cast<std::size_t>(a.upper[k] - base_ + 1);
        }
        return index;
    }

    Value loadElem(const hir::Expr& e) {
        const std::size_t ix = elemIndex(e);
        const Array& a = array(e.var);
        switch (var(e.var).type) {
            case Type::Str: return Value::ofStr(a.strs[ix]);
            case Type::Float: return Value::ofFloat(a.floats[ix]);
            default: return Value::ofInt(a.ints[ix]);
        }
    }

    void storeTarget(const hir::Expr& t, Value x) {
        if (t.op == Op::Load) return store(t.var, std::move(x));
        const std::size_t ix = elemIndex(t);
        Array& a = array(t.var);
        switch (var(t.var).type) {
            case Type::Str: a.strs[ix] = std::move(x.s); break;
            case Type::Float: a.floats[ix] = x.f; break;
            default: a.ints[ix] = x.i; break;
        }
    }

    Value loadTarget(const hir::Expr& t) { return t.op == Op::Load ? load(t.var) : loadElem(t); }

    void dim(const hir::Expr& t) {
        Array& a = array(t.var, false);
        if (a.dimmed) throw RuntimeError{kErrDimTwice};
        std::size_t n = 1;
        a.upper.clear();
        for (const auto& d : t.args) {
            const std::int32_t u = eval(*d).i;
            if (u < base_) throw RuntimeError{kErrDimRange};
            a.upper.push_back(u);
            n *= static_cast<std::size_t>(u - base_ + 1);
            if (n > 16u * 1024 * 1024) throw RuntimeError{kErrDimRange};
        }
        const Type ty = var(t.var).type;
        a.ints.assign(ty == Type::Str || ty == Type::Float ? 0 : n, 0);
        a.floats.assign(ty == Type::Float ? n : 0, 0);
        a.strs.assign(ty == Type::Str ? n : 0, std::string());
        a.dimmed = true;
    }

    // ---- conversions ----------------------------------------------------------------------------
    // Assignment semantics: truncation and range errors 2/3/4; Bool: non-zero -> -1.
    Value convert(Value v, Type to) {
        if (to == Type::Float) return v.type == Type::Float ? v : Value::ofFloat(ofI32(v.i));
        if (to == Type::Str) return v;
        const bool f = v.type == Type::Float;
        switch (to) {
            case Type::Bool: return Value::ofInt(f ? brt::convBool(v.f) : brt::convBool(v.i));
            case Type::Byte: return Value::ofInt(f ? brt::convByte(v.f) : brt::convByte(v.i));
            case Type::Word: return Value::ofInt(f ? brt::convWord(v.f) : brt::convWord(v.i));
            default: return Value::ofInt(f ? brt::toInt(v.f) : v.i);
        }
    }

    static Value wrapTo(Type t, std::int64_t v) {
        switch (t) {
            case Type::Byte: return Value::ofInt(brt::wrapByte(v));
            case Type::Word: return Value::ofInt(brt::wrapWord(v));
            case Type::Bool: return Value::ofInt(brt::wrapBool(v));
            default: return Value::ofInt(brt::wrapInt(v));
        }
    }

    f64 num(const Value& v) const { return v.type == Type::Float ? v.f : ofI32(v.i); }

    // ---- expressions ----------------------------------------------------------------------------
    Value eval(const hir::Expr& e) {
        switch (e.op) {
            case Op::ConstInt: return Value::ofInt(e.ival);
            case Op::ConstFloat: return Value::ofFloat(e.fval);
            case Op::ConstStr: return Value::ofStr(e.sval);
            case Op::Load: return load(e.var);
            case Op::LoadElem: return loadElem(e);
            case Op::Conv: return convert(eval(*e.args[0]), e.type);
            case Op::ToInt: return Value::ofInt(toI32(eval(*e.args[0]).f));
            case Op::Neg: return Value::ofFloat(fp_neg(eval(*e.args[0]).f));
            case Op::Add: return floatOp(e, brt::add);
            case Op::Sub: return floatOp(e, brt::sub);
            case Op::Mul: return floatOp(e, brt::mul);
            case Op::Div: return floatOp(e, brt::div);
            case Op::Pow: return floatOp(e, brt::power);
            case Op::IDiv: return intOp(e, brt::idiv);
            case Op::IMod: return intOp(e, brt::imod);
            case Op::Not: return Value::ofInt(~eval(*e.args[0]).i);
            case Op::And: return intOp(e, [](std::int32_t a, std::int32_t b) { return a & b; });
            case Op::Or: return intOp(e, [](std::int32_t a, std::int32_t b) { return a | b; });
            case Op::Xor: return intOp(e, [](std::int32_t a, std::int32_t b) { return a ^ b; });
            case Op::Imp: return intOp(e, brt::imp);
            case Op::Eqv: return intOp(e, brt::eqv);
            case Op::Eq: case Op::Ne: case Op::Lt: case Op::Gt: case Op::Le: case Op::Ge: return compare(e);
            case Op::ApproxEq: {
                const f64 a = eval(*e.args[0]).f, b = eval(*e.args[1]).f;
                return Value::ofInt(brt::approxEq(a, b));
            }
            case Op::Concat: {
                Value a = eval(*e.args[0]);
                a.s = brt::concat(std::move(a.s), eval(*e.args[1]).s);
                return a;
            }
            case Op::Builtin: return builtin(e);
            case Op::CallFunc: return callFunc(e);
        }
        return Value();
    }

    Value floatOp(const hir::Expr& e, f64 (*fn)(f64, f64)) {
        const f64 a = eval(*e.args[0]).f, b = eval(*e.args[1]).f;
        return Value::ofFloat(fn(a, b));
    }

    template <class F>
    Value intOp(const hir::Expr& e, F fn) {
        const std::int32_t a = eval(*e.args[0]).i, b = eval(*e.args[1]).i;
        return Value::ofInt(fn(a, b));
    }

    Value compare(const hir::Expr& e) {
        const Value a = eval(*e.args[0]), b = eval(*e.args[1]);
        int c;
        if (a.type == Type::Str) c = brt::compare(a.s, b.s);
        else c = brt::compare(num(a), num(b));
        bool r = false;
        switch (e.op) {
            case Op::Eq: r = c == 0; break;
            case Op::Ne: r = c != 0; break;
            case Op::Lt: r = c < 0; break;
            case Op::Gt: r = c > 0; break;
            case Op::Le: r = c <= 0; break;
            default: r = c >= 0; break;
        }
        return Value::ofInt(r ? -1 : 0);
    }

    // ---- built-in functions ---------------------------------------------------------------------
    // The arguments are evaluated from left to right first, then platform/pc does the work.
    // bytes of a value of this type in memory (VARPTR): %, & , |/! and float
    static int widthOf(Type t) { return t == Type::Word ? 2 : (t == Type::Byte || t == Type::Bool) ? 1 : 4; }

    // TYPE numbers (probe19 T5): # 0, $ 1, % 2, ! 3, & 8, | 9; arrays # 4, $ 5, % 6, ! 7, & 12, | 13
    static int typeCode(Type t, bool array) {
        switch (t) {
            case Type::Float: return array ? 4 : 0;
            case Type::Str: return array ? 5 : 1;
            case Type::Bool: return array ? 7 : 3;
            case Type::Word: return array ? 12 : 8;
            case Type::Byte: return array ? 13 : 9;
            default: return array ? 6 : 2;
        }
    }

    std::int32_t varptr(const hir::Expr& t) {
        const Type ty = var(t.var).type;
        if (t.op == Op::Load) {
            Slot& sl = slot(t.var);
            if (ty == Type::Str) return static_cast<std::int32_t>(mem_.of(sl.s));
            if (ty == Type::Float) return static_cast<std::int32_t>(mem_.of(sl.f, typeCode(ty, false)));
            return static_cast<std::int32_t>(mem_.of(sl.i, widthOf(ty), typeCode(ty, false)));
        }
        const std::size_t ix = elemIndex(t);
        Array& a = array(t.var);
        if (ty == Type::Str) return static_cast<std::int32_t>(mem_.of(a.strs[ix]));
        if (ty == Type::Float) return static_cast<std::int32_t>(mem_.of(a.floats, a.upper, base_, a.floats[ix]));
        return static_cast<std::int32_t>(mem_.of(a.ints, a.upper, base_, a.ints[ix], widthOf(ty)));
    }

    // ARRPTR(x), *x: numbers as VARPTR; a string or an array: its descriptor (probe19 T1-T3)
    std::int32_t arrptr(const hir::Expr& t) {
        const Type ty = var(t.var).type;
        if (!(t.op == Op::Load && var(t.var).array)) {
            if (ty == Type::Str && t.op == Op::Load) return static_cast<std::int32_t>(mem_.descriptor(slot(t.var).s));
            return varptr(t);
        }
        Slot* sl = &slot(t.var);
        auto block = [this, sl, ty]() -> std::uint32_t {
            if (!sl->arr || !sl->arr->dimmed || ty == Type::Str) return 0;  // string arrays: no block bytes
            Array& a = *sl->arr;
            const std::uint32_t first = ty == Type::Float ? mem_.of(a.floats, a.upper, base_, a.floats[0])
                                                          : mem_.of(a.ints, a.upper, base_, a.ints[0], widthOf(ty));
            return first - static_cast<std::uint32_t>(4 * a.upper.size());
        };
        auto dims = [sl]() { return sl->arr && sl->arr->dimmed ? static_cast<int>(sl->arr->upper.size()) : 0; };
        return static_cast<std::int32_t>(mem_.descriptor(sl, typeCode(ty, true), block, dims));
    }

    Value builtin(const hir::Expr& e) {
        const std::string& n = e.sval;
        if (n == "VARPTR") return Value::ofInt(varptr(*e.args[0]));
        if (n == "ARRPTR") return Value::ofInt(arrptr(*e.args[0]));
        if (n == "DIM?") {
            const Array& arr = array(e.args[0]->var, false);
            if (!arr.dimmed) return Value::ofInt(0);
            std::int32_t c = 1;
            for (std::int32_t u : arr.upper) c *= u - base_ + 1;
            return Value::ofInt(c);
        }
        std::vector<Value> a;
        a.reserve(e.args.size());
        for (const auto& x : e.args) a.push_back(eval(*x));
        const std::size_t argc = a.size();
        auto I = [](std::int32_t v) { return Value::ofInt(v); };
        auto F = [](f64 v) { return Value::ofFloat(v); };
        auto S = [](std::string v) { return Value::ofStr(std::move(v)); };
        auto f = [&](std::size_t k) { return a[k].f; };
        auto s = [&](std::size_t k) -> const std::string& { return a[k].s; };
        if (n == "ABS") return F(brt::abs(f(0)));
        if (n == "SGN") return I(brt::sgn(f(0)));
        if (n == "INT") return F(brt::intf(f(0)));
        if (n == "TRUNC" || n == "FIX") return F(brt::truncf(f(0)));
        if (n == "FRAC") return F(brt::frac(f(0)));
        if (n == "CINT") return I(brt::cint(f(0)));
        if (n == "CFLOAT") return F(f(0));
        if (n == "ROUND") return F(brt::round(f(0), argc > 1 ? brt::toInt(f(1)) : 0));
        if (n == "SQR") return F(brt::sqr(f(0)));
        if (n == "EXP") return F(brt::expf(f(0)));
        if (n == "LOG") return F(brt::logf(f(0)));
        if (n == "LOG10") return F(brt::log10f(f(0)));
        if (n == "SIN") return F(brt::sinf(f(0)));
        if (n == "COS") return F(brt::cosf(f(0)));
        if (n == "TAN") return F(brt::tanf(f(0)));
        if (n == "ATN") return F(brt::atnf(f(0)));
        if (n == "ASIN") return F(brt::asinf(f(0)));
        if (n == "ACOS") return F(brt::acosf(f(0)));
        if (n == "DEG") return F(brt::degf(f(0)));
        if (n == "RAD") return F(brt::radf(f(0)));
        if (n == "SINQ") return F(brt::sinq(f(0)));
        if (n == "COSQ") return F(brt::cosq(f(0)));
        if (n == "ODD") return I(brt::odd(f(0)));
        if (n == "EVEN") return I(brt::even(f(0)));
        if (n == "MIN" || n == "MAX") {
            std::size_t best = 0;
            for (std::size_t k = 1; k < argc; ++k) {
                const int c = a[k].type == Type::Str ? brt::compare(a[k].s, a[best].s) : brt::compare(a[k].f, a[best].f);
                if ((n == "MIN" && c < 0) || (n == "MAX" && c > 0)) best = k;
            }
            return a[best];
        }
        if (n == "RND") return F(brt::rnd());
        if (n == "RANDOM") return I(brt::random(f(0)));
        if (n == "RAND") return I(brt::rand(f(0)));
        if (n == "ADD") return I(brt::fnAdd(f(0), f(1)));
        if (n == "SUB") return I(brt::fnSub(f(0), f(1)));
        if (n == "MUL") return I(brt::fnMul(f(0), f(1)));
        if (n == "DIV") return I(brt::fnDiv(f(0), f(1)));
        if (n == "MOD") return I(brt::fnMod(f(0), f(1)));
        if (n == "PRED") return e.type == Type::Str ? S(brt::pred(s(0))) : I(brt::pred(f(0)));
        if (n == "SUCC") return e.type == Type::Str ? S(brt::succ(s(0))) : I(brt::succ(f(0)));
        if (n == "BCLR") return I(brt::bitOp('C', f(0), f(1)));
        if (n == "BSET") return I(brt::bitOp('S', f(0), f(1)));
        if (n == "BCHG") return I(brt::bitOp('X', f(0), f(1)));
        if (n == "BTST") return I(brt::bitOp('T', f(0), f(1)));
        if (n.rfind("SHL", 0) == 0 || n.rfind("SHR", 0) == 0 || n.rfind("ROL", 0) == 0 || n.rfind("ROR", 0) == 0) {
            const int width = n.size() == 3 ? 32 : n[3] == '&' ? 16 : 8;
            return I(brt::shift(f(0), f(1), width, n[2] == 'L', n[0] == 'R'));
        }
        if (n == "AND") return I(brt::toInt(f(0)) & brt::toInt(f(1)));
        if (n == "OR") return I(brt::toInt(f(0)) | brt::toInt(f(1)));
        if (n == "XOR") return I(brt::toInt(f(0)) ^ brt::toInt(f(1)));
        if (n == "IMP") return I(brt::imp(brt::toInt(f(0)), brt::toInt(f(1))));
        if (n == "EQV") return I(brt::eqv(brt::toInt(f(0)), brt::toInt(f(1))));
        if (n == "SWAP") return I(brt::swapWords(f(0)));
        if (n == "BYTE") return I(brt::byteOf(f(0)));
        if (n == "CARD") return I(brt::cardOf(f(0)));
        if (n == "WORD") return I(brt::wordOf(f(0)));
        if (n == "TRUE") return I(-1);
        if (n == "FALSE") return I(0);
        if (n == "PI") return F(brt::kPi);
        if (n == "ERR") return I(static_cast<std::int8_t>(static_cast<std::uint8_t>(err_ & 0xFF)));  // probe9 T6
        if (n == "LEN") return I(brt::len(s(0)));
        if (n == "EOF") return I(files_.eof(f(0)));
        if (n == "LOF") return I(files_.lof(f(0)));
        if (n == "EXIST") return I(brt::Files::exist(s(0)));
        if (n == "LOC") return I(files_.loc(f(0)));
        if (n.size() > 2 && n.substr(n.size() - 2) == "{}") {
            const char k = n[0] == 'D' ? 'D' : n[0];
            if (n == "CHAR{}") return S(mem_.getChars(f(0)));
            if (k == 'F' || k == 'S' || k == 'D') return F(mem_.getf(f(0), k));
            return I(mem_.get(f(0), k));
        }
        if (n == "PEEK") return I(mem_.peek(f(0), 1));
        if (n == "TYPE") return I(mem_.typeAt(f(0)));
        if (n == "DPEEK") return I(mem_.peek(f(0), 2));
        if (n == "LPEEK") return I(mem_.peek(f(0), 4));
        if (n == "MALLOC") return I(mem_.malloc(f(0), argc > 1 ? f(1) : 0));
        if (n == "MFREE") return I(mem_.mfree(f(0), f(1)));
        if (n == "MKI$") return S(brt::mki(f(0)));
        if (n == "MKL$") return S(brt::mkl(f(0)));
        if (n == "MKS$") return S(brt::mks(f(0)));
        if (n == "MKD$") return S(brt::mkd(f(0)));
        if (n == "CVI") return I(brt::cvi(s(0)));
        if (n == "CVL") return I(brt::cvl(s(0)));
        if (n == "CVS") return F(brt::cvs(s(0)));
        if (n == "CVD") return F(brt::cvd(s(0)));
        if (n == "INP") return I(files_.inp(f(0)));
        if (n == "INPUT$") return argc == 1 ? S(kbd_.chars(f(0))) : S(files_.inputChars(f(0), f(1)));
        if (n == "INKEY$") return S(kbd_.inkey());
        if (n == "LEFT$") return S(brt::left(s(0), argc > 1 ? f(1) : F64_ONE));
        if (n == "RIGHT$") return S(brt::right(s(0), argc > 1 ? f(1) : F64_ONE));
        if (n == "MID$") return S(argc > 2 ? brt::mid(s(0), f(1), f(2)) : brt::mid(s(0), f(1)));
        if (n == "RINSTR") {
            if (a[0].type == Type::Str) return I(argc > 2 ? brt::rinstr(s(0), s(1), f(2)) : brt::rinstr(s(0), s(1)));
            return I(brt::rinstr(s(1), s(2), f(0)));
        }
        if (n == "POS") return I(out_.pos());
        if (n == "CRSCOL") return I(out_.crsCol);
        if (n == "CRSLIN") return I(out_.crsLin);
        if (n == "TIMER") return I(brt::timer());
        if (n == "INSTR") {
            if (a[0].type == Type::Str) return I(brt::instr(s(0), s(1), argc > 2 ? f(2) : F64_ONE));
            return I(brt::instr(s(1), s(2), f(0)));
        }
        if (n == "STRING$") return S(a[1].type == Type::Str ? brt::stringOf(f(0), s(1)) : brt::stringOf(f(0), f(1)));
        if (n == "SPACE$") return S(brt::space(f(0)));
        if (n == "CHR$") return S(brt::chr(f(0)));
        if (n == "ASC") return I(brt::asc(s(0)));
        if (n == "STR$") {
            if (argc == 1) return S(brt::str(f(0), out_.defnum));  // STR$ follows DEFNUM (probe2 T33)
            if (argc == 2) return S(brt::str(f(0), f(1)));
            return S(brt::str(f(0), f(1), f(2), out_.mode));
        }
        if (n == "VAL") return F(brt::val(s(0)));
        if (n == "VAL?") return I(brt::valCount(s(0)));
        if (n == "UPPER$") return S(brt::upper(s(0)));
        if (n == "TRIM$") return S(brt::trim(s(0)));
        if (n == "HEX$") return S(brt::radix(f(0), 16, argc > 1 ? f(1) : 0));
        if (n == "BIN$") return S(brt::radix(f(0), 2, argc > 1 ? f(1) : 0));
        if (n == "OCT$") return S(brt::radix(f(0), 8, argc > 1 ? f(1) : 0));
        if (n == "ERR$") return S(brt::errorText(brt::toInt(f(0))));
        if (n == "#PACK") return I(brt::pack(s(0)));
        throw RuntimeError{RT_ERR_OVERFLOW};  // not reached: the semantic pass rejects unknown functions
    }

    // ---- routines -------------------------------------------------------------------------------
    // Saves variables on entry to a routine and restores them on every exit (D15).
    struct Frame {
        Interp& in;
        std::vector<std::pair<int, Slot>> saved;
        bool normalExit = false;
        explicit Frame(Interp& i) : in(i) { in.frames_.push_back(this); }
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
        void save(int v) {
            for (const auto& s : saved)
                if (s.first == v) return;
            saved.emplace_back(v, std::move(in.raw(v)));
            in.raw(v) = Slot();
        }
        ~Frame() {
            in.frames_.pop_back();
            for (auto it = saved.rbegin(); it != saved.rend(); ++it) in.raw(it->first) = std::move(it->second);
        }
    };

    void bind(Frame& f, const hir::Routine& r, const std::vector<hir::ExprPtr>& args) {
        std::vector<Value> values;
        values.reserve(args.size());
        for (std::size_t k = 0; k < args.size(); ++k)
            values.push_back(r.params[k].byRef ? Value() : eval(*args[k]));
        for (std::size_t k = 0; k < args.size(); ++k) {
            const int pv = r.params[k].var;
            if (r.params[k].byRef) {  // the parameter name stands for the caller's variable
                const int target = resolve(args[k]->var);
                if (target == resolve(pv)) continue;  // already the same variable
                f.save(pv);
                raw(pv).alias = target;
            } else {
                f.save(pv);
                store(pv, std::move(values[k]));
            }
        }
    }

    void callProc(int id, const std::vector<hir::ExprPtr>& args) {
        const hir::Routine& r = p_.routines[static_cast<std::size_t>(id)];
        Frame f(*this);
        bind(f, r, args);
        exec(r.body);
        f.normalExit = true;
    }

    Value callFunc(const hir::Expr& e) {
        const hir::Routine& r = p_.routines[static_cast<std::size_t>(e.func)];
        Frame f(*this);
        bind(f, r, e.args);
        if (r.kind == hir::Routine::DefFn) {
            Value v = eval(*r.expr);
            f.normalExit = true;
            return v;
        }
        try {
            exec(r.body);
        } catch (ReturnSignal& ret) {
            f.normalExit = true;
            return std::move(ret.value);
        }
        throw FatalError{"FUNCTION " + r.name + " ended without RETURN"};
    }

    // ---- statements -----------------------------------------------------------------------------
    void execTop(const hir::Block& b) { exec(b); }

    // Executes a block. Errors of a statement go to the error routine (ON ERROR GOSUB); GOTO to a
    // label of this block continues there.
    void exec(const hir::Block& b, std::size_t from = 0) {
        std::size_t k = from;
        while (k < b.size()) {
            const hir::Stmt& s = *b[k];
            int jump = -1;  // label to continue at (GOTO, RESUME label)
            try {
                execStmt(s);
                ++k;
            } catch (RuntimeError& e) {
                if (onError_ < 0) throw;
                const int r = handle(e.code);
                if (r == 1) ++k;          // RESUME NEXT; 0: RESUME, the statement again
                else if (r >= 2) jump = r - 2;  // RESUME label
            } catch (GotoSignal& g) {
                jump = g.label;
            }
            if (jump >= 0) {
                const std::size_t target = findLabel(b, jump);
                if (target == b.size()) throw GotoSignal{jump};
                k = target;
            }
        }
    }

    static std::size_t findLabel(const hir::Block& b, int label) {
        for (std::size_t k = 0; k < b.size(); ++k)
            if (b[k]->kind == SKind::Label && b[k]->label == label) return k;
        return b.size();
    }

    // Calls the error routine; returns 0 for RESUME, 1 for RESUME NEXT, 2 + label for RESUME label.
    int handle(int code) {
        err_ = code;
        const int proc = onError_;
        onError_ = -1;  // the routine has to set ON ERROR GOSUB again
        ++handlerDepth_;
        try {
            static const std::vector<hir::ExprPtr> none;
            callProc(proc, none);
        } catch (ResumeSignal& r) {
            --handlerDepth_;
            err_ = 0;  // RESUME clears ERR (probe12 T1)
            return r.mode == 2 ? 2 + r.label : r.mode;
        } catch (...) {
            --handlerDepth_;
            throw;
        }
        --handlerDepth_;
        // the routine ended without RESUME: the error counts as unhandled, the program ends with
        // its message (probe12 T2)
        throw brt::Unhandled{code};
    }

    void step(const hir::Stmt& s) {
        line_ = s.line;
        if (opt_.maxSteps != 0 && ++steps_ > opt_.maxSteps) throw StepLimit{};
    }

    bool truth(const hir::Expr& e) {
        const Value v = eval(e);
        return v.type == Type::Float ? fp_cmp(v.f, 0) != 0 : v.i != 0;
    }

    void loop(const hir::Block& body) {
        exec(body);
    }

    void execStmt(const hir::Stmt& s) {
        step(s);
        switch (s.kind) {
            case SKind::Assign: storeTarget(*s.target, eval(*s.value)); return;
            case SKind::Print: return print(s);
            case SKind::If:
                for (const hir::Branch& br : s.branches)
                    if (header(*br.cond) != 0) return exec(br.body);
                return exec(s.elseBody);
            case SKind::For: return forLoop(s);
            case SKind::While:
                try {
                    while (header(*s.cond) != 0) loop(s.body);
                } catch (const ExitLoop&) {
                }
                return;
            case SKind::Repeat:
                try {
                    do loop(s.body);
                    while (!truth(*s.cond));
                } catch (const ExitLoop&) {
                }
                return;
            case SKind::Do:
                try {
                    for (;;) {
                        if (s.pre == 1 && header(*s.cond) == 0) break;
                        if (s.pre == 2 && header(*s.cond) == 1) break;
                        loop(s.body);
                        if (s.post == 1 && !truth(*s.cond2)) break;
                        if (s.post == 2 && truth(*s.cond2)) break;
                    }
                } catch (const ExitLoop&) {
                }
                return;
            case SKind::ExitIf:
                if (truth(*s.cond)) throw ExitLoop{};
                return;
            case SKind::Select: return select(s);
            case SKind::Call: return callProc(s.proc, s.args);
            case SKind::Return: throw ReturnSignal{eval(*s.value)};
            case SKind::Local:  // save, clear; restored when the routine ends
                for (int v : s.vars) frames_.back()->save(v);
                return;
            case SKind::Goto: throw GotoSignal{s.label};
            case SKind::Label: return;
            case SKind::Read: return read(s);
            case SKind::Restore:
                dataPos_ = s.label < 0 ? 0 : static_cast<std::size_t>(p_.labels[static_cast<std::size_t>(s.label)].data);
                return;
            case SKind::OnError: onError_ = s.proc; return;
            case SKind::Resume:
                if (handlerDepth_ == 0) throw FatalError{"RESUME outside the error routine"};
                throw ResumeSignal{s.mode, s.label};
            case SKind::Cmd: return command(s);
        }
    }

    // Condition of a header line (WHILE, IF, ELSE IF, DO WHILE/UNTIL). An error there belongs to
    // that line (probe11): RESUME evaluates it again, RESUME NEXT goes on with the next line, the
    // first one of the body. Returns 1 (true), 0 (false) or -1 (RESUME NEXT after an error).
    int header(const hir::Expr& e) {
        for (;;) {
            try {
                return truth(e) ? 1 : 0;
            } catch (RuntimeError& err) {
                if (onError_ < 0) throw;
                const int r = handle(err.code);
                if (r == 0) continue;
                if (r == 1) return -1;
                throw GotoSignal{r - 2};
            }
        }
    }

    void forLoop(const hir::Stmt& s) {
        // GFA semantics (probe1 T18-T21, probe2 T10-T14): end and step evaluated once; the
        // condition is checked before the first pass; the variable is increased (integers wrap)
        // and the new value is checked.
        const hir::Expr& t = *s.target;
        const Type ty = var(t.var).type;
        f64 end = 0, step = F64_ONE;
        bool failed = false;  // error in the FOR line and RESUME NEXT (D19, probe11 T4, T12)
        for (;;) {
            try {
                storeTarget(t, eval(*s.start));
                end = eval(*s.end).f;
                step = eval(*s.step).f;
                break;
            } catch (RuntimeError& err) {
                if (onError_ < 0) throw;
                const int r = handle(err.code);
                if (r == 0) continue;
                if (r >= 2) throw GotoSignal{r - 2};
                failed = true;
                break;
            }
        }
        const bool down = fp_cmp(step, 0) < 0;
        auto inRange = [&](f64 v) { return down ? fp_cmp(v, end) >= 0 : fp_cmp(v, end) <= 0; };
        auto increase = [&]() {
            const f64 next = fp_add(num(loadTarget(t)), step);
            check();
            if (ty == Type::Float) {
                storeTarget(t, Value::ofFloat(next));
            } else {
                // integers wrap like INC: |next| < 2^32, so one correction by 2^32 suffices
                const f64 two31 = 0x41E0000000000000u, two32 = 0x41F0000000000000u;
                f64 tr = fp_trunc(next);
                if (fp_cmp(tr, two31) >= 0) tr = fp_sub(tr, two32);
                else if (fp_cmp(tr, fp_neg(two31)) < 0) tr = fp_add(tr, two32);
                storeTarget(t, wrapTo(ty, toI32(tr)));
            }
            return next;
        };
        if (failed) {  // the body once, NEXT increases the variable and leaves the loop
            try {
                loop(s.body);
                increase();
            } catch (const ExitLoop&) {
            }
            return;
        }
        if (!inRange(num(loadTarget(t)))) return;
        try {
            for (;;) {
                loop(s.body);
                if (!inRange(increase())) break;
            }
        } catch (const ExitLoop&) {
        }
    }

    void select(const hir::Stmt& s) {
        const std::int32_t v = eval(*s.value).i;
        std::size_t k = 0;
        for (; k < s.cases.size(); ++k) {
            bool hit = false;
            for (const hir::CaseItem& it : s.cases[k].items) {
                if (!it.range) hit = eval(*it.lo).i == v;
                else hit = (!it.lo || eval(*it.lo).i <= v) && (!it.hi || v <= eval(*it.hi).i);
                if (hit) break;
            }
            if (hit) break;
        }
        // run the case (and with CONT the following ones), else DEFAULT
        for (; k < s.cases.size(); ++k) {
            exec(s.cases[k].body);
            if (!s.cases[k].cont) return;
        }
        exec(s.elseBody);
    }

    // the next DATA item as a number (MAT READ)
    f64 readNumber() {
        if (dataPos_ >= p_.data.size()) throw RuntimeError{kErrOutOfData};
        const hir::DataItem& d = p_.data[dataPos_];
        std::int32_t used = 0;
        const f64 v = rt_val(d.text.data(), static_cast<std::int32_t>(d.text.size()), &used);
        if (d.quoted || d.text.empty() || used != static_cast<std::int32_t>(d.text.size())) throw RuntimeError{kErrNotNumeric};
        ++dataPos_;
        return v;
    }

    // ---- MAT (D23) --------------------------------------------------------------------------------
    brt::MatBase matBase() const { return brt::MatBase{matBase_, base_}; }
    brt::MatArray matArray(const hir::Expr& t) {
        Array& a = array(t.var);
        return brt::MatArray{a.floats.data(), &a.upper, a.dimmed};
    }
    std::vector<std::int32_t> matAt(const hir::Expr& t) {
        std::vector<std::int32_t> at;
        for (const auto& x : t.args) at.push_back(eval(*x).i);
        return at;
    }
    rt_mat view(const hir::Expr& t) { return brt::matView(matArray(t), matBase()); }

    void mat(const hir::Stmt& s) {
        const std::string w = s.name.substr(4);
        const auto& T = s.targets;
        auto arg = [&](std::size_t k) { return eval(*s.args[k]); };
        if (w == "BASE") {
            matBase_ = arg(0).i != 0 ? 1 : 0;
        } else if (w == "CLR" || w == "SET") {
            brt::matFill(matArray(*T[0]), matBase(), w == "SET" ? arg(0).f : F64_ZERO);
        } else if (w == "ONE") {
            const rt_mat m = view(*T[0]);
            rt_mat_one(&m);
        } else if (w == "READ") {
            const rt_mat m = view(*T[0]);
            for (std::int32_t i = 0; i < m.rows; ++i)
                for (std::int32_t j = 0; j < m.cols; ++j) brt::matAt(m, i, j) = readNumber();
        } else if (w == "PRINT") {
            f64 ch = 0;
            if (s.channel) ch = eval(*s.channel).f;
            brt::Output& out = s.channel ? files_.output(ch, out_) : out_;
            const rt_mat m = view(*T[0]);
            if (s.args.size() == 2) {
                const f64 g = arg(0).f;
                brt::matPrint(out, m, true, g, arg(1).f);
            } else {
                brt::matPrint(out, m, false, 0, 0);
            }
            if (s.channel) files_.flush(ch);
        } else if (w == "INPUT") {  // values read before an error stay (probe18 T4, T5)
            const f64 ch = eval(*s.channel).f;
            const rt_mat m = view(*T[0]);
            for (std::int32_t i = 0; i < m.rows; ++i)
                for (std::int32_t j = 0; j < m.cols; ++j) brt::matAt(m, i, j) = files_.inputNumber(ch);
        } else if (w == "CPY" || w == "XCPY") {
            const std::vector<std::int32_t> da = matAt(*T[0]), sa = matAt(*T[1]);
            std::int32_t h = -1, wd = -1;
            if (s.args.size() == 2) {
                h = arg(0).i;
                wd = arg(1).i;
            }
            brt::matCpy(matArray(*T[0]), da, matArray(*T[1]), sa, h, wd, w == "XCPY", matBase());
        } else if (w == "TRANS") {
            const rt_mat a = view(*T[0]);
            const rt_mat b = T.size() > 1 ? view(*T[1]) : a;
            rt_mat_trans(&a, &b);
        } else if (w == "INV") {
            const rt_mat b = view(*T[0]), a = view(*T[1]);
            rt_mat_inv(&b, &a);
        } else if (w == "ADD" || w == "SUB") {
            const rt_mat a = view(*T[0]);
            if (s.mode == 2) {
                const f64 x = arg(0).f;
                rt_mat_scalar(&a, x, w == "ADD" ? 0 : 1);
            } else {
                const rt_mat b = view(*T[1]);
                const rt_mat c = s.mode == 0 ? view(*T[2]) : b;
                if (s.mode == 0) rt_mat_addsub(&a, &b, &c, w == "SUB");
                else rt_mat_addsub(&a, &a, &b, w == "SUB");
            }
        } else if (w == "MUL") {
            if (s.mode == 0) {  // two vectors: the outer product (column times row)
                const rt_mat a = view(*T[0]);
                const bool vectors = array(T[1]->var).upper.size() == 1 && array(T[2]->var).upper.size() == 1;
                const rt_mat b = brt::matView(matArray(*T[1]), matBase(), vectors), c = view(*T[2]);
                rt_mat_mul(&a, &b, &c);
            } else if (s.mode == 1 || s.mode == 2) {
                const rt_mat a = view(*T[1]), b = view(*T[2]);
                f64 x = 0;
                if (s.mode == 1) {
                    x = rt_mat_dot(&a, &b);
                } else {
                    const rt_mat c = view(*T[3]);
                    x = rt_mat_triple(&a, &b, &c);
                }
                check();
                storeTarget(*T[0], convert(Value::ofFloat(x), var(T[0]->var).type));
            } else {
                const rt_mat a = view(*T[0]);
                rt_mat_scalar(&a, arg(0).f, 2);
            }
        } else if (w == "NORM") {
            const rt_mat a = view(*T[0]);
            rt_mat_norm(&a, arg(0).i != 0);
        } else if (w == "DET" || w == "QDET" || w == "RANG") {
            const std::int32_t n = s.args.empty() ? -1 : arg(0).i;
            const rt_mat a = brt::matSub(matArray(*T[1]), matBase(), matAt(*T[1]), n);
            const f64 x = w == "RANG" ? rt_mat_rank(&a) : rt_mat_det(&a, w == "QDET");
            check();
            storeTarget(*T[0], convert(Value::ofFloat(x), var(T[0]->var).type));
        }
        check();
    }

    void read(const hir::Stmt& s) {
        for (const auto& t : s.targets) {
            if (dataPos_ >= p_.data.size()) throw RuntimeError{kErrOutOfData};
            const hir::DataItem& d = p_.data[dataPos_];
            const Type ty = var(t->var).type;
            if (ty == Type::Str) {
                storeTarget(*t, Value::ofStr(d.text));
            } else {
                // a non-numeric item is an error and stays the next item (probe9 T1, T2)
                std::int32_t used = 0;
                const f64 v = rt_val(d.text.data(), static_cast<std::int32_t>(d.text.size()), &used);
                if (d.quoted || d.text.empty() || used != static_cast<std::int32_t>(d.text.size()))
                    throw RuntimeError{kErrNotNumeric};
                storeTarget(*t, convert(Value::ofFloat(v), ty));
            }
            ++dataPos_;
        }
    }

    void print(const hir::Stmt& s) {
        f64 ch = 0;
        if (s.channel) ch = eval(*s.channel).f;
        brt::Output& out = s.channel ? files_.output(ch, out_) : out_;
        if (s.usingFmt) {
            printUsing(s, out);
        } else {
            out.begin();
            char last = 0;
            for (const hir::PrintItem& it : s.items) {
                if (it.expr) {
                    if (it.kind == hir::PrintItem::Value) putValue(eval(*it.expr), out);
                    else if (it.kind == hir::PrintItem::Spc) out.spc(eval(*it.expr).f);
                    else if (it.kind == hir::PrintItem::At) {
                        const f64 c = eval(*it.expr).f;
                        out.at(c, eval(*it.expr2).f);
                    }
                    else out.tab(eval(*it.expr).f);
                }
                last = it.sep;
                if (it.sep == ',') out.comma();
                else if (it.sep == '\'') out.apostrophe();
            }
            if (last != ';' && last != ',') out.newline();
        }
        if (s.channel) files_.flush(ch);
    }

    void printUsing(const hir::Stmt& s, brt::Output& out) {
        brt::Using u(out, eval(*s.usingFmt).s);
        char last = 0;
        for (const hir::PrintItem& it : s.items) {
            if (it.expr) {
                const Value v = eval(*it.expr);
                if (v.type == Type::Str) u.value(v.s);
                else u.value(num(v));
            }
            last = it.sep;
        }
        u.end();
        if (last != ';' && last != ',') out.newline();
    }

    void command(const hir::Stmt& s) {
        const std::string& n = s.name;
        if (n.rfind("MAT ", 0) == 0) {
            mat(s);
            return;
        }
        if (n == "CLS") {
            out_.cls();
            return;
        }
        if (n == "LOCATE") {
            const f64 c = eval(*s.args[0]).f;
            out_.locate(c, eval(*s.args[1]).f);
            return;
        }
        if (n == "HTAB" || n == "VTAB") {
            const f64 v = eval(*s.args[0]).f;
            if (n == "HTAB") out_.htab(v);
            else out_.vtab(v);
            return;
        }
        if (n == "INC" || n == "DEC") {
            const hir::Expr& t = *s.targets[0];
            const Value v = loadTarget(t);
            const Type ty = var(t.var).type;
            if (ty == Type::Float) storeTarget(t, Value::ofFloat(fp_add(v.f, n == "INC" ? F64_ONE : fp_neg(F64_ONE))));
            else storeTarget(t, wrapTo(ty, static_cast<std::int64_t>(v.i) + (n == "INC" ? 1 : -1)));
            check();
            return;
        }
        if (n == "ADD" || n == "SUB" || n == "MUL" || n == "DIV") {
            const hir::Expr& t = *s.targets[0];
            const Value v = loadTarget(t);
            const f64 y = eval(*s.args[0]).f;
            const Type ty = var(t.var).type;
            if (ty == Type::Float) {
                const f64 r = n == "ADD" ? fp_add(v.f, y) : n == "SUB" ? fp_sub(v.f, y) : n == "MUL" ? fp_mul(v.f, y) : fp_div(v.f, y);
                check();
                storeTarget(t, Value::ofFloat(r));
            } else {  // integer arithmetic without overflow check; y truncated (MUL a%,3.5 = MUL a%,3)
                const std::int64_t a = v.i, b = toI32(fp_trunc(y));
                if (n == "DIV" && b == 0) throw RuntimeError{RT_ERR_DIV_ZERO};
                const std::int64_t r = n == "ADD" ? a + b : n == "SUB" ? a - b : n == "MUL" ? a * b : a / b;
                storeTarget(t, wrapTo(ty, r));
            }
            return;
        }
        if (n == "SWAP" && s.mode == 1) {  // SWAP *p,a(): the array p points to (probe19 T4)
            const f64 p = eval(*s.args[0]).f;
            const hir::Expr& a = *s.targets[0];
            int code = -1;
            const void* key = mem_.keyAt(p, code);
            if (key == nullptr || code != typeCode(var(a.var).type, true)) throw RuntimeError{68};  // var-type mismatch (ours)
            std::swap(static_cast<Slot*>(const_cast<void*>(key))->arr, slot(a.var).arr);
            return;
        }
        if (n == "SWAP") {
            const hir::Expr& a = *s.targets[0];
            const hir::Expr& b = *s.targets[1];
            if (var(a.var).array && a.op == Op::Load) {
                std::swap(slot(a.var).arr, slot(b.var).arr);
                return;
            }
            Value va = loadTarget(a), vb = loadTarget(b);
            storeTarget(a, std::move(vb));
            storeTarget(b, std::move(va));
            return;
        }
        if (n == "CLR") {
            for (const auto& t : s.targets) slot(t->var) = Slot();
            return;
        }
        if (n == "ERASE") {
            for (const auto& t : s.targets) slot(t->var).arr.reset();
            return;
        }
        if (n == "DIM") {
            for (const auto& t : s.targets) dim(*t);
            return;
        }
        if (n == "MID$=") {  // MID$(a$,x[,y])=b$: overwrite, length unchanged (probe4 T46)
            const hir::Expr& t = *s.targets[0];
            std::string a = loadTarget(t).s;
            std::int32_t from = toI32(fp_trunc(eval(*s.args[0]).f));
            std::int32_t len = s.args.size() > 1 ? toI32(fp_trunc(eval(*s.args[1]).f)) : 32767;
            const std::string b = eval(*s.value).s;
            if (from < 1) from = 1;
            for (std::int32_t k = 0; k < len && k < static_cast<std::int32_t>(b.size()); ++k) {
                const std::size_t pos = static_cast<std::size_t>(from - 1 + k);
                if (pos >= a.size()) break;
                a[pos] = b[static_cast<std::size_t>(k)];
            }
            storeTarget(t, Value::ofStr(std::move(a)));
            return;
        }
        if (n == "LSET" || n == "RSET") {
            const hir::Expr& t = *s.targets[0];
            const std::string a = loadTarget(t).s;
            std::string b = eval(*s.value).s;
            if (b.size() > a.size()) b.resize(a.size());
            const std::string fill(a.size() - b.size(), ' ');
            storeTarget(t, Value::ofStr(n == "LSET" ? b + fill : fill + b));
            return;
        }
        if (n == "ON GOSUB") {
            const std::int32_t k = toI32(fp_trunc(eval(*s.args[0]).f));
            static const std::vector<hir::ExprPtr> none;
            if (k >= 1 && k <= static_cast<std::int32_t>(s.vars.size())) callProc(s.vars[static_cast<std::size_t>(k - 1)], none);
            return;
        }
        if (n == "VOID") {
            eval(*s.args[0]);
            return;
        }
        if (n == "END" || n == "STOP" || n == "QUIT" || n == "SYSTEM" || n == "EDIT" || n == "NEW") throw EndProgram{};
        if (n == "DEFNUM") {
            out_.defnum = std::min(13, std::max(2, toI32(fp_trunc(eval(*s.args[0]).f))));
            return;
        }
        if (n == "OPEN") {
            const std::string mode = eval(*s.args[0]).s;
            const f64 ch = eval(*s.args[1]).f;
            const std::string name = eval(*s.args[2]).s;
            files_.open(mode, ch, name, s.args.size() > 3 ? eval(*s.args[3]).f : 0);
            return;
        }
        if (n == "CLOSE") {
            if (s.args.empty()) files_.closeAll();
            else files_.close(eval(*s.args[0]).f);
            return;
        }
        if (n == "KILL") {
            files_.kill(eval(*s.args[0]).s);
            return;
        }
        if (n == "NAME") {
            const std::string from = eval(*s.args[0]).s;
            files_.name(from, eval(*s.args[1]).s);
            return;
        }
        if (n == "FIELD") {  // the variables are bound to the record of the channel (probe16)
            const f64 ch = eval(*s.args[0]).f;
            std::vector<f64> lens;
            std::vector<std::string*> vars;
            for (std::size_t k = 1; k < s.args.size(); ++k) lens.push_back(eval(*s.args[k]).f);
            for (const auto& t : s.targets) vars.push_back(&slot(t->var).s);
            files_.field(ch, vars, lens);
            return;
        }
        if (n == "GET#" || n == "PUT#") {
            const f64 ch = eval(*s.args[0]).f;
            const bool numbered = s.args.size() > 1;
            const f64 rec = numbered ? eval(*s.args[1]).f : 0;
            if (n == "GET#") files_.get(ch, numbered, rec);
            else files_.put(ch, numbered, rec);
            return;
        }
        if (n == "RECORD" || n == "SEEK" || n == "RELSEEK") {
            const f64 ch = eval(*s.args[0]).f;
            const f64 v = eval(*s.args[1]).f;
            if (n == "RECORD") files_.record(ch, v);
            else if (n == "SEEK") files_.seek(ch, v);
            else files_.relseek(ch, v);
            return;
        }
        if (n == "POKE" || n == "DPOKE" || n == "LPOKE") {
            const f64 a = eval(*s.args[0]).f;
            mem_.poke(a, n == "POKE" ? 1 : n == "DPOKE" ? 2 : 4, eval(*s.args[1]).f);
            return;
        }
        if (n.size() > 2 && n.substr(n.size() - 2) == "{}") {
            const f64 a = eval(*s.args[0]).f;
            const char k = n[0];
            if (n == "CHAR{}") mem_.setChars(a, eval(*s.args[1]).s);
            else if (k == 'F' || k == 'S' || k == 'D') mem_.setf(a, k, eval(*s.args[1]).f);
            else mem_.set(a, k, eval(*s.args[1]).f);
            return;
        }
        if (n == "BMOVE") {
            const f64 from = eval(*s.args[0]).f;
            const f64 to = eval(*s.args[1]).f;
            mem_.bmove(from, to, eval(*s.args[2]).f);
            return;
        }
        if (n == "OUT") {
            const f64 ch = eval(*s.args[0]).f;
            for (std::size_t k = 1; k < s.args.size(); ++k) files_.out(ch, eval(*s.args[k]).f);
            return;
        }
        if (n == "INPUT" || n == "LINE INPUT") {  // from the keyboard
            kbd_.begin(out_, eval(*s.args[0]).s, s.mode);
            for (const auto& t : s.targets) {
                const Type ty = var(t->var).type;
                if (n == "LINE INPUT") storeTarget(*t, Value::ofStr(kbd_.line(out_)));
                else if (ty == Type::Str) storeTarget(*t, Value::ofStr(kbd_.field(out_)));
                else storeTarget(*t, convert(Value::ofFloat(kbd_.number(out_)), ty));
            }
            brt::Keyboard::end(out_);
            return;
        }
        if (n == "INPUT#" || n == "LINE INPUT#") {
            const f64 ch = eval(*s.args[0]).f;
            for (const auto& t : s.targets) {
                const Type ty = var(t->var).type;
                if (n == "LINE INPUT#") storeTarget(*t, Value::ofStr(files_.lineInput(ch)));
                else if (ty == Type::Str) storeTarget(*t, Value::ofStr(files_.inputString(ch)));
                else storeTarget(*t, convert(Value::ofFloat(files_.inputNumber(ch)), ty));
            }
            return;
        }
        if (n == "WRITE" || n == "WRITE#") {
            const bool file = n == "WRITE#";
            const f64 ch = file ? eval(*s.args[0]).f : 0;
            brt::Output& out = file ? files_.output(ch, out_) : out_;
            for (std::size_t k = file ? 1 : 0; k < s.args.size(); ++k) {
                const Value v = eval(*s.args[k]);
                const bool first = k == (file ? 1u : 0u);
                if (v.type == Type::Str) brt::writeValue(out, v.s, first);
                else brt::writeValue(out, v.f, first);
            }
            if (s.mode != 1) out.newline();  // WRITE ...; : no line feed
            if (file) files_.flush(ch);
            return;
        }
        if (n == "MODE") {
            out_.mode = toI32(fp_trunc(eval(*s.args[0]).f)) & 3;
            return;
        }
        if (n == "OPTION BASE") {
            base_ = toI32(fp_trunc(eval(*s.args[0]).f)) != 0 ? 1 : 0;
            return;
        }
        if (n == "RANDOMIZE") {
            const std::int32_t seed = s.args.empty() ? 0 : toI32(fp_trunc(eval(*s.args[0]).f));
            rt_randomize(seed != 0 ? static_cast<std::uint32_t>(seed) : opt_.seed);
            return;
        }
        if (n == "ERROR") throw RuntimeError{toI32(fp_trunc(eval(*s.args[0]).f))};
        if (n == "CLEAR") {
            for (Slot& sl : slots_) sl = Slot();
            return;
        }
        // CLS, PAUSE, DELAY, TRON, TROFF: no effect on the output of the reference
        for (const auto& a : s.args) eval(*a);
    }
};

}  // namespace

std::string errorText(int code) {
    return brt::errorText(code);
}

RunResult interpret(const hir::Program& program, const RunOptions& options) {
    Interp in(program, options);
    return in.run();
}

}  // namespace basic
