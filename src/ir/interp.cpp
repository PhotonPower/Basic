#include "interp.hpp"

#include <cstring>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "rt_bridge.hpp"
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

constexpr f64 kPi = 0x400921FB54442D18u;

struct RuntimeError {
    int code;
};

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

// Checks the runtime error recorded by runtime/ref (rt_error) and raises it.
void check() {
    const int e = takeRuntimeError();
    if (e >= 0) throw RuntimeError{e};
}

std::int32_t toI32(f64 v) {
    const std::int32_t r = fp_to_i32(v);
    check();
    return r;
}

f64 ofI32(std::int32_t v) {
    return fp_from_i32(v);
}

std::string fmtFloat(f64 v, int digits) {
    char buf[RT_FMT_FLT_MAX];
    rt_fmt_flt(buf, v, digits);
    return buf;
}

void checkLen(const std::string& s) {
    if (s.size() > 32767) throw RuntimeError{RT_ERR_STR_LONG};
}

class Interp {
public:
    Interp(const hir::Program& p, const RunOptions& o) : p_(p), opt_(o), slots_(p.vars.size()) {
        rt_randomize(o.seed);
        takeRuntimeError();
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
        r.output = std::move(out_);
        return r;
    }

private:
    const hir::Program& p_;
    RunOptions opt_;
    std::vector<Slot> slots_;
    std::string out_;
    int col_ = 0;  // characters since the last line feed (POS)
    int line_ = 0;
    std::uint64_t steps_ = 0;
    int err_ = 0;
    int onError_ = -1;
    int handlerDepth_ = 0;
    int defnum_ = 13;
    int mode_ = 0;
    int base_ = 0;
    std::size_t dataPos_ = 0;
    struct Frame;
    std::vector<Frame*> frames_;

    // ---- output ---------------------------------------------------------------------------------
    void put(const std::string& s) {
        for (char c : s) {
            out_ += c;
            col_ = c == '\n' ? 0 : col_ + 1;
        }
    }

    std::string str(const Value& v) const {
        if (v.type == Type::Str) return v.s;
        if (v.type == Type::Float) return fmtFloat(v.f, defnum_);
        return fmtFloat(ofI32(v.i), defnum_);
    }

    // ---- variables ------------------------------------------------------------------------------
    Slot& slot(int var) { return slots_[static_cast<std::size_t>(var)]; }
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
        std::int32_t i = v.i;
        if (v.type == Type::Float) {
            if (to == Type::Bool) return Value::ofInt(fp_cmp(v.f, 0) != 0 ? -1 : 0);
            if (to == Type::Word) {
                const std::int16_t w = fp_to_i16(v.f);
                check();
                return Value::ofInt(w);
            }
            if (to == Type::Byte) {
                const std::uint8_t b = fp_to_u8(v.f);
                check();
                return Value::ofInt(b);
            }
            i = toI32(v.f);
        }
        switch (to) {
            case Type::Bool: return Value::ofInt(i != 0 ? -1 : 0);
            case Type::Byte:
                if (i < 0 || i > 255) throw RuntimeError{RT_ERR_NO_BYTE};
                break;
            case Type::Word:
                if (i < -32768 || i > 32767) throw RuntimeError{RT_ERR_NO_WORD};
                break;
            default: break;
        }
        return Value::ofInt(i);
    }

    static Value wrapTo(Type t, std::int64_t v) {
        switch (t) {
            case Type::Byte: return Value::ofInt(static_cast<std::int32_t>(v & 0xFF));
            case Type::Word: return Value::ofInt(static_cast<std::int16_t>(static_cast<std::uint16_t>(v & 0xFFFF)));
            case Type::Bool: return Value::ofInt(v != 0 ? -1 : 0);
            default: return Value::ofInt(static_cast<std::int32_t>(static_cast<std::uint32_t>(v & 0xFFFFFFFF)));
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
            case Op::Add: return floatOp(e, fp_add);
            case Op::Sub: return floatOp(e, fp_sub);
            case Op::Mul: return floatOp(e, fp_mul);
            case Op::Div: return floatOp(e, fp_div);
            case Op::Pow: return Value::ofFloat(power(eval(*e.args[0]).f, eval(*e.args[1]).f));
            case Op::IDiv:
            case Op::IMod: {
                const std::int32_t a = eval(*e.args[0]).i, b = eval(*e.args[1]).i;
                if (b == 0) throw RuntimeError{RT_ERR_DIV_ZERO};
                if (b == -1) return Value::ofInt(e.op == Op::IDiv ? static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(a)) : 0);
                return Value::ofInt(e.op == Op::IDiv ? a / b : a % b);
            }
            case Op::Not: return Value::ofInt(~eval(*e.args[0]).i);
            case Op::And: return intOp(e, [](std::int32_t a, std::int32_t b) { return a & b; });
            case Op::Or: return intOp(e, [](std::int32_t a, std::int32_t b) { return a | b; });
            case Op::Xor: return intOp(e, [](std::int32_t a, std::int32_t b) { return a ^ b; });
            case Op::Imp: return intOp(e, [](std::int32_t a, std::int32_t b) { return ~a | b; });
            case Op::Eqv: return intOp(e, [](std::int32_t a, std::int32_t b) { return ~(a ^ b); });
            case Op::Eq: case Op::Ne: case Op::Lt: case Op::Gt: case Op::Le: case Op::Ge: return compare(e);
            case Op::ApproxEq: {
                const f64 a = eval(*e.args[0]).f, b = eval(*e.args[1]).f;
                return Value::ofInt(fp_approx_eq(a, b) ? -1 : 0);
            }
            case Op::Concat: {
                Value a = eval(*e.args[0]);
                a.s += eval(*e.args[1]).s;
                checkLen(a.s);
                return a;
            }
            case Op::Builtin: return builtin(e);
            case Op::CallFunc: return callFunc(e);
        }
        return Value();
    }

    Value floatOp(const hir::Expr& e, f64 (*fn)(f64, f64)) {
        const f64 a = eval(*e.args[0]).f, b = eval(*e.args[1]).f;
        const f64 r = fn(a, b);
        check();
        return Value::ofFloat(r);
    }

    template <class F>
    Value intOp(const hir::Expr& e, F fn) {
        const std::int32_t a = eval(*e.args[0]).i, b = eval(*e.args[1]).i;
        return Value::ofInt(fn(a, b));
    }

    // x^y: integer exponents by repeated multiplication (exact for small powers, e.g. 2^-26);
    // other exponents need EXP/LOG, which the runtime does not provide yet.
    f64 power(f64 x, f64 y) {
        const f64 t = fp_trunc(y);
        if (fp_cmp(t, y) != 0) throw RuntimeError{RT_ERR_OVERFLOW};  // TODO: EXP(y*LOG(x))
        const f64 lim = 0x41DFFFFFFFC00000u;  // 2^31-1
        if (fp_cmp(t & ~(std::uint64_t{1} << 63), lim) > 0) throw RuntimeError{RT_ERR_OVERFLOW};
        std::int64_t n = toI32(t);
        const bool neg = n < 0;
        if (neg) n = -n;
        f64 r = F64_ONE, b = x;
        while (n > 0) {
            if (n & 1) r = fp_mul(r, b);
            n >>= 1;
            if (n > 0) b = fp_mul(b, b);
            check();
        }
        if (neg) r = fp_div(F64_ONE, r);
        check();
        return r;
    }

    Value compare(const hir::Expr& e) {
        const Value a = eval(*e.args[0]), b = eval(*e.args[1]);
        int c;
        if (a.type == Type::Str) c = a.s < b.s ? -1 : (a.s > b.s ? 1 : 0);
        else c = fp_cmp(num(a), num(b));
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
    std::int32_t argInt(const hir::Expr& e, std::size_t k) { return toI32(fp_trunc(eval(*e.args[k]).f)); }
    f64 argF(const hir::Expr& e, std::size_t k) { return eval(*e.args[k]).f; }
    std::string argS(const hir::Expr& e, std::size_t k) { return eval(*e.args[k]).s; }

    static std::string radix(std::uint32_t v, unsigned base, std::int32_t width) {
        static const char* digits = "0123456789ABCDEF";
        std::string r;
        do {
            r.insert(r.begin(), digits[v % base]);
            v /= base;
        } while (v != 0);
        if (width > 0) {
            if (static_cast<std::int32_t>(r.size()) < width) r.insert(0, static_cast<std::size_t>(width) - r.size(), '0');
            else r = r.substr(r.size() - static_cast<std::size_t>(width));
        }
        return r;
    }

    static std::uint32_t shift(std::uint32_t x, std::int32_t n, int width, bool left, bool rotate, bool signExtend) {
        const std::uint32_t mask = width == 32 ? 0xFFFFFFFFu : (1u << width) - 1;
        n &= 31;
        std::uint32_t v = x & mask;
        if (rotate) {
            n %= width;
            if (n != 0) v = left ? ((v << n) | (v >> (width - n))) & mask : ((v >> n) | (v << (width - n))) & mask;
        } else {
            v = n >= width ? 0 : (left ? (v << n) & mask : v >> n);
        }
        if (width == 16 && signExtend) return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(v)));
        return v;
    }

    Value builtin(const hir::Expr& e) {
        const std::string& n = e.sval;
        auto I = [&](std::int32_t v) { return Value::ofInt(v); };
        auto F = [&](f64 v) {
            check();
            return Value::ofFloat(v);
        };
        auto S = [&](std::string v) {
            checkLen(v);
            return Value::ofStr(std::move(v));
        };
        if (n == "ABS") return F(argF(e, 0) & ~(std::uint64_t{1} << 63));
        if (n == "SGN") return I(fp_cmp(argF(e, 0), 0));
        if (n == "INT") return F(fp_int(argF(e, 0)));
        if (n == "TRUNC" || n == "FIX") return F(fp_trunc(argF(e, 0)));
        if (n == "FRAC") return F(fp_frac(argF(e, 0)));
        if (n == "CINT") return I(toI32(fp_cint(argF(e, 0))));
        if (n == "CFLOAT") return F(argF(e, 0));
        if (n == "ROUND") return F(fp_round(argF(e, 0), e.args.size() > 1 ? argInt(e, 1) : 0));
        if (n == "SQR") return F(fp_sqrt(argF(e, 0)));
        if (n == "ODD") return I((argInt(e, 0) & 1) != 0 ? -1 : 0);
        if (n == "EVEN") return I((argInt(e, 0) & 1) == 0 ? -1 : 0);
        if (n == "MIN" || n == "MAX") {
            Value best = eval(*e.args[0]);
            for (std::size_t k = 1; k < e.args.size(); ++k) {
                Value v = eval(*e.args[k]);
                const int c = v.type == Type::Str ? (v.s < best.s ? -1 : (v.s > best.s ? 1 : 0)) : fp_cmp(v.f, best.f);
                if ((n == "MIN" && c < 0) || (n == "MAX" && c > 0)) best = std::move(v);
            }
            return best;
        }
        if (n == "RND") {
            if (!e.args.empty()) eval(*e.args[0]);
            return F(rt_rnd());
        }
        if (n == "RANDOM") return I(toI32(fp_int(fp_mul(rt_rnd(), argF(e, 0)))));
        if (n == "RAND") return I(rt_rand(argInt(e, 0)));
        if (n == "ADD" || n == "SUB" || n == "MUL") {
            const std::int64_t a = argInt(e, 0), b = argInt(e, 1);
            return wrapTo(Type::Int, n == "ADD" ? a + b : n == "SUB" ? a - b : a * b);
        }
        if (n == "DIV" || n == "MOD") {
            const std::int32_t a = argInt(e, 0), b = argInt(e, 1);
            if (b == 0) throw RuntimeError{RT_ERR_DIV_ZERO};
            if (b == -1) return I(n == "DIV" ? static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(a)) : 0);
            return I(n == "DIV" ? a / b : a % b);
        }
        if (n == "PRED" || n == "SUCC") {
            const int d = n == "PRED" ? -1 : 1;
            if (e.type == Type::Str) {
                const std::string s = argS(e, 0);
                return S(std::string(1, static_cast<char>((s.empty() ? 0 : static_cast<unsigned char>(s[0])) + d)));
            }
            return wrapTo(Type::Int, static_cast<std::int64_t>(argInt(e, 0)) + d);
        }
        if (n == "BCLR" || n == "BSET" || n == "BCHG" || n == "BTST") {
            const std::uint32_t x = static_cast<std::uint32_t>(argInt(e, 0));
            const std::uint32_t bit = 1u << (argInt(e, 1) & 31);
            if (n == "BTST") return I((x & bit) != 0 ? -1 : 0);
            return I(static_cast<std::int32_t>(n == "BCLR" ? x & ~bit : n == "BSET" ? x | bit : x ^ bit));
        }
        if (n.rfind("SHL", 0) == 0 || n.rfind("SHR", 0) == 0 || n.rfind("ROL", 0) == 0 || n.rfind("ROR", 0) == 0) {
            const int width = n.size() == 3 ? 32 : n[3] == '&' ? 16 : 8;
            const std::uint32_t r = shift(static_cast<std::uint32_t>(argInt(e, 0)), argInt(e, 1), width, n[2] == 'L',
                                          n[0] == 'R', true);
            return I(static_cast<std::int32_t>(r));
        }
        if (n == "AND") return I(argInt(e, 0) & argInt(e, 1));
        if (n == "OR") return I(argInt(e, 0) | argInt(e, 1));
        if (n == "XOR") return I(argInt(e, 0) ^ argInt(e, 1));
        if (n == "IMP") return I(~argInt(e, 0) | argInt(e, 1));
        if (n == "EQV") return I(~(argInt(e, 0) ^ argInt(e, 1)));
        if (n == "SWAP") {
            const std::uint32_t x = static_cast<std::uint32_t>(argInt(e, 0));
            return I(static_cast<std::int32_t>((x << 16) | (x >> 16)));
        }
        if (n == "BYTE") return I(argInt(e, 0) & 0xFF);
        if (n == "CARD") return I(argInt(e, 0) & 0xFFFF);
        if (n == "WORD") return I(static_cast<std::int16_t>(static_cast<std::uint16_t>(argInt(e, 0) & 0xFFFF)));
        if (n == "TRUE") return I(-1);
        if (n == "FALSE") return I(0);
        if (n == "PI") return F(kPi);
        if (n == "ERR") return I(static_cast<std::int8_t>(static_cast<std::uint8_t>(err_ & 0xFF)));  // probe9 T6
        // strings
        if (n == "LEN") return I(static_cast<std::int32_t>(argS(e, 0).size()));
        if (n == "LEFT$" || n == "RIGHT$") {
            const std::string s = argS(e, 0);
            const std::int32_t k = e.args.size() > 1 ? argInt(e, 1) : 1;
            if (k < 0) throw RuntimeError{RT_ERR_STR_LONG};  // probe4 T45
            const std::size_t m = std::min(static_cast<std::size_t>(k), s.size());
            return S(n == "LEFT$" ? s.substr(0, m) : s.substr(s.size() - m));
        }
        if (n == "MID$") {
            const std::string s = argS(e, 0);
            std::int32_t from = argInt(e, 1);
            if (from < 0) throw RuntimeError{RT_ERR_STR_LONG};
            if (from == 0) from = 1;  // probe4 T44
            std::int32_t len = e.args.size() > 2 ? argInt(e, 2) : 32767;
            if (len < 0) throw RuntimeError{RT_ERR_STR_LONG};
            if (static_cast<std::size_t>(from) > s.size()) return S("");
            return S(s.substr(static_cast<std::size_t>(from - 1), static_cast<std::size_t>(len)));
        }
        if (n == "INSTR") return I(instr(e));
        if (n == "STRING$") {
            const std::int32_t k = argInt(e, 0);
            if (k < 0 || k > 32767) throw RuntimeError{RT_ERR_STR_LONG};
            const Value v = eval(*e.args[1]);
            const std::string unit = v.type == Type::Str ? v.s : std::string(1, static_cast<char>(toI32(fp_trunc(v.f)) & 0xFF));
            std::string r;
            for (std::int32_t j = 0; j < k; ++j) r += unit;
            return S(std::move(r));
        }
        if (n == "SPACE$") {
            const std::int32_t k = argInt(e, 0);
            if (k < 0 || k > 32767) throw RuntimeError{RT_ERR_STR_LONG};
            return S(std::string(static_cast<std::size_t>(k), ' '));
        }
        if (n == "CHR$") return S(std::string(1, static_cast<char>(argInt(e, 0) & 0xFF)));
        if (n == "ASC") {
            const std::string s = argS(e, 0);
            return I(s.empty() ? 0 : static_cast<unsigned char>(s[0]));
        }
        if (n == "STR$") {
            const f64 x = argF(e, 0);
            char buf[512];
            if (e.args.size() == 1) return S(fmtFloat(x, 13));
            if (e.args.size() == 2) return S(std::string(buf, static_cast<std::size_t>(rt_str_n(buf, sizeof buf, x, argInt(e, 1)))));
            return S(std::string(buf, static_cast<std::size_t>(rt_str_fmt(buf, sizeof buf, x, argInt(e, 1), argInt(e, 2), mode_))));
        }
        if (n == "VAL" || n == "VAL?") {
            const std::string s = argS(e, 0);
            std::int32_t used = 0;
            const f64 v = rt_val(s.data(), static_cast<std::int32_t>(s.size()), &used);
            return n == "VAL" ? F(v) : I(used);
        }
        if (n == "UPPER$") {
            std::string s = argS(e, 0);
            for (char& c : s) {
                const unsigned char u = static_cast<unsigned char>(c);
                if ((u >= 'a' && u <= 'z') || (u >= 0xE0 && u <= 0xFE && u != 0xF7)) c = static_cast<char>(u - 32);
            }
            return S(std::move(s));
        }
        if (n == "TRIM$") {
            std::string s = argS(e, 0);
            const std::size_t a = s.find_first_not_of(' ');
            if (a == std::string::npos) return S("");
            return S(s.substr(a, s.find_last_not_of(' ') - a + 1));
        }
        if (n == "HEX$" || n == "BIN$" || n == "OCT$") {
            const unsigned base = n == "HEX$" ? 16 : n == "BIN$" ? 2 : 8;
            return S(radix(static_cast<std::uint32_t>(argInt(e, 0)), base, e.args.size() > 1 ? argInt(e, 1) : 0));
        }
        if (n == "ERR$") return S(errorText(argInt(e, 0)));
        if (n == "DIM?") {
            const Array& a = array(e.args[0]->var, false);
            if (!a.dimmed) return I(0);
            std::int32_t c = 1;
            for (std::int32_t u : a.upper) c *= u - base_ + 1;
            return I(c);
        }
        if (n == "#PACK") {  // SELECT on a string: up to 4 characters, big-endian
            const std::string s = argS(e, 0);
            std::uint32_t v = 0;
            for (std::size_t k = 0; k < s.size() && k < 4; ++k) v = (v << 8) | static_cast<unsigned char>(s[k]);
            return I(static_cast<std::int32_t>(v));
        }
        throw RuntimeError{RT_ERR_OVERFLOW};  // not reached: the semantic pass rejects unknown functions
    }

    // INSTR(a$,b$[,x]) or INSTR(x,a$,b$); empty search string as decided in D12.
    std::int32_t instr(const hir::Expr& e) {
        std::string a, b;
        std::int32_t from = 1;
        if (e.args[0]->type == Type::Str) {
            a = argS(e, 0);
            b = argS(e, 1);
            if (e.args.size() > 2) from = argInt(e, 2);
        } else {
            from = argInt(e, 0);
            a = argS(e, 1);
            b = argS(e, 2);
        }
        if (from < 0) throw RuntimeError{RT_ERR_STR_LONG};
        if (from == 0) from = 1;
        if (b.empty()) return from <= static_cast<std::int32_t>(a.size()) + 1 ? (a.empty() ? 1 : from) : 0;
        if (static_cast<std::size_t>(from) > a.size()) return 0;
        const std::size_t p = a.find(b, static_cast<std::size_t>(from - 1));
        return p == std::string::npos ? 0 : static_cast<std::int32_t>(p + 1);
    }

    // ---- routines -------------------------------------------------------------------------------
    // Saves variables on entry to a routine and restores them on every exit (D15).
    struct Frame {
        Interp& in;
        std::vector<std::pair<int, Slot>> saved;
        std::vector<std::pair<int, int>> byRef;  // (parameter, caller variable): copied back on exit
        bool normalExit = false;
        explicit Frame(Interp& i) : in(i) { in.frames_.push_back(this); }
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
        void save(int v) {
            for (const auto& s : saved)
                if (s.first == v) return;
            saved.emplace_back(v, in.slot(v));
            in.slot(v) = Slot();
        }
        ~Frame() {
            in.frames_.pop_back();
            if (normalExit)
                for (const auto& [param, callerVar] : byRef) copyBack(param, callerVar);
            for (auto it = saved.rbegin(); it != saved.rend(); ++it) in.slot(it->first) = std::move(it->second);
        }
        void copyBack(int param, int callerVar) {
            Slot& p = in.slot(param);
            Slot& c = in.slot(callerVar);
            if (in.var(param).array) c.arr = p.arr;
            else c = p;
        }
    };

    void bind(Frame& f, const hir::Routine& r, const std::vector<hir::ExprPtr>& args) {
        std::vector<Value> values;
        values.reserve(args.size());
        for (std::size_t k = 0; k < args.size(); ++k)
            values.push_back(r.params[k].byRef ? Value() : eval(*args[k]));
        for (std::size_t k = 0; k < args.size(); ++k) {
            const int pv = r.params[k].var;
            if (r.params[k].byRef) {
                const int cv = args[k]->var;
                if (cv == pv) continue;  // the same variable: nothing to bind
                Slot copy = slot(cv);
                f.save(pv);
                slot(pv) = std::move(copy);
                f.byRef.emplace_back(pv, cv);
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
            return r.mode == 2 ? 2 + r.label : r.mode;
        } catch (...) {
            --handlerDepth_;
            throw;
        }
        --handlerDepth_;
        return 1;  // routine ended without RESUME: continue after the statement
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
                    if (truth(*br.cond)) return exec(br.body);
                return exec(s.elseBody);
            case SKind::For: return forLoop(s);
            case SKind::While:
                try {
                    while (truth(*s.cond)) loop(s.body);
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
                        if (s.pre == 1 && !truth(*s.cond)) break;
                        if (s.pre == 2 && truth(*s.cond)) break;
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

    void forLoop(const hir::Stmt& s) {
        // GFA semantics (probe1 T18-T21, probe2 T10-T14): end and step evaluated once; the
        // condition is checked before the first pass; the variable is increased (integers wrap)
        // and the new value is checked.
        const hir::Expr& t = *s.target;
        const Type ty = var(t.var).type;
        storeTarget(t, eval(*s.start));
        const f64 end = eval(*s.end).f;
        const f64 step = eval(*s.step).f;
        const bool down = fp_cmp(step, 0) < 0;
        auto inRange = [&](f64 v) { return down ? fp_cmp(v, end) >= 0 : fp_cmp(v, end) <= 0; };
        if (!inRange(num(loadTarget(t)))) return;
        try {
            for (;;) {
                loop(s.body);
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
                if (!inRange(next)) break;
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
        if (s.usingFmt) return printUsing(s);
        char last = 0;
        for (const hir::PrintItem& it : s.items) {
            if (it.expr) {
                if (it.kind == hir::PrintItem::Value) {
                    put(str(eval(*it.expr)));
                } else {
                    std::int32_t n = toI32(fp_trunc(eval(*it.expr).f));
                    if (it.kind == hir::PrintItem::Spc) {
                        put(std::string(static_cast<std::size_t>(std::max(n, 0)), ' '));
                    } else {
                        if (n < col_) put("\n");
                        if (n > col_) put(std::string(static_cast<std::size_t>(n - col_), ' '));
                    }
                }
            }
            last = it.sep;
            if (it.sep == ',') put(std::string(static_cast<std::size_t>(16 - col_ % 16), ' '));
            else if (it.sep == '\'') put(" ");
        }
        if (last != ';' && last != ',') put("\n");
    }

    void printUsing(const hir::Stmt& s) {
        const std::string fmt = eval(*s.usingFmt).s;
        rt_using u;
        rt_using_begin(&u, fmt.data(), static_cast<std::int32_t>(fmt.size()), mode_);
        char buf[4096];
        char last = 0;
        for (const hir::PrintItem& it : s.items) {
            if (it.expr) {
                const Value v = eval(*it.expr);
                const std::int32_t n = v.type == Type::Str
                                           ? rt_using_str(&u, buf, sizeof buf, v.s.data(), static_cast<std::int32_t>(v.s.size()))
                                           : rt_using_num(&u, buf, sizeof buf, num(v));
                put(std::string(buf, static_cast<std::size_t>(n)));
            }
            last = it.sep;
        }
        put(std::string(buf, static_cast<std::size_t>(rt_using_end(&u, buf, sizeof buf))));
        if (last != ';' && last != ',') put("\n");
    }

    void command(const hir::Stmt& s) {
        const std::string& n = s.name;
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
            defnum_ = std::min(13, std::max(2, toI32(fp_trunc(eval(*s.args[0]).f))));
            return;
        }
        if (n == "MODE") {
            mode_ = toI32(fp_trunc(eval(*s.args[0]).f)) & 3;
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
    // ERR$ texts of GFA-BASIC 3.5 (English), read on the original (probe1-probe9)
    static const char* const texts[] = {
        "divide by zero",                              // 0
        "overflow",                                    // 1
        "no integer|-2147483648 .. 2147483647",        // 2
        "no byte|0 .. 255",                            // 3
        "no word|-32768 .. 32767",                     // 4
        "no negative squareroot",                      // 5
        "logarithm only for|numbers greater zero",     // 6
        "unknown error ",                              // 7
        "out of memory",                               // 8
        "function or command|not implemented",         // 9
        "string too long|max. 32767 chars",            // 10
        "no GFA-BASIC 3.5 program",                    // 11
        "program too long|out of memory|new",          // 12
        "no GFA-BASIC program|eof - new",              // 13
        "redimensioned array",                         // 14
        "array undimensioned",                         // 15
        "array index out of range",                    // 16
        "dim out of range",                            // 17
        "wrong # of indices",                          // 18
        "procedure not found",                         // 19
        "label not found",                             // 20
        "open allows:|\"i\"nput  \"o\"utput \"r\"andom|\"a\"ppend \"u\"pdate",  // 21
        "file already open",                           // 22
        "file # wrong",                                // 23
        "file not open",                               // 24
        "wrong input, no number",                      // 25
        "end of file",                                 // 26
        "too many points for|polyline/polyfill|max. 128",  // 27
        "array not linear",                            // 28
        "# of points too large",                       // 29
        "merge - no ASCII",                            // 30
        "merge - line too long",                       // 31
        "==> syntax error|program aborted",            // 32
        "undefined label",                             // 33
        "out of data",                                 // 34
        "data not numeric",                            // 35
    };
    const int c = code & 0xFF;  // ERR$(-56) = "unknown error 200" (probe9 T6)
    if (c < static_cast<int>(sizeof texts / sizeof texts[0])) return texts[c];
    return "unknown error " + std::to_string(c);
}

RunResult interpret(const hir::Program& program, const RunOptions& options) {
    Interp in(program, options);
    return in.run();
}

}  // namespace basic
