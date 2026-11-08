// Conformance of the Z80 runtime (runtime/z80) against the C reference (runtime/ref): every routine
// is called directly in the emulator with edge cases and random operands (fixed seed) and must give
// the same bits and raise the same error (docs/testing.md). IX and IY must be preserved.
// BASIC_CONFORMANCE_CASES sets the number of random cases per routine (default 2000).

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "link/linker.hpp"
#include "rt_ref.h"
#include "test_util.hpp"
#include "z80_routines.hpp"

using namespace basic;
using basic::harness::Z80Regs;
using basic::harness::Z80Routines;

namespace {

int g_error = -1;
extern "C" void rt_error(int code) { g_error = code; }

constexpr std::uint16_t kA = 0xE000, kB = 0xE010, kDst = 0xE020, kErr = 0xF000;

struct Rng {
    std::uint64_t s = 0x9E3779B97F4A7C15u;
    std::uint64_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<std::uint64_t>(hi - lo + 1)); }
};

f64 randomF64(Rng& r, int lo, int hi) {
    const std::uint64_t frac = r.next() & ((std::uint64_t{1} << 52) - 1);
    const auto be = static_cast<std::uint64_t>(r.range(lo, hi));
    return ((r.next() & 1) << 63) | (be << 52) | (be == 0 ? 0 : frac);
}

std::vector<f64> edgeCases() {
    return {0,
            F64_ONE,
            0xBFF0000000000000u,
            0x0010000000000000u,
            0x7FEFFFFFFFFFFFFFu,
            0xFFEFFFFFFFFFFFFFu,
            0x3FF0000000000001u,
            0x3FEFFFFFFFFFFFFFu,
            0x4340000000000000u,
            0x3CA0000000000000u,
            0x3FF8000000000000u,
            0x41DFFFFFFFC00000u /* 2^31 - 1 */,
            0xC1E0000000000000u /* -2^31 */,
            0x41E0000000000000u /* 2^31 */,
            0x40DFFFC000000000u /* 32767 */,
            0xC0E0000000000000u /* -32768 */,
            0x406FE00000000000u /* 255 */,
            0xBFE0000000000000u /* -0.5 */};
}

class Runtime {
public:
    Runtime() {
        Diagnostics d;
        std::vector<link::Module> mods;
        for (link::Module& m : link::loadModules(std::string(BASIC_SOURCE_DIR) + "/runtime/z80", d))
            if (m.name != "rt_err" && m.name != "rt_onerr" && m.name != "rt_resume") mods.push_back(std::move(m));
        std::vector<std::string> all;
        for (const link::Module& m : mods) all.insert(all.end(), m.provides.begin(), m.provides.end());
        for (link::Module& m : link::loadModules(std::string(BASIC_SOURCE_DIR) + "/platform/z80-test", d))
            if (m.name != "plat_error" && m.name != "plat_start" && m.name != "plat_heap") mods.push_back(std::move(m));
        std::vector<link::Module> stub = link::loadModules(std::string(BASIC_SOURCE_DIR) + "/tests/conformance/z80", d);
        mods.insert(mods.end(), stub.begin(), stub.end());
        for (const link::Module& m : mods) link::checkLabels(m, d);
        const link::Library lib = link::makeLibrary(std::move(mods), d);
        link::Program prog;  // what a compiled program provides for the runtime (rt_data): no DATA items
        prog.data = "prg_data_count:\n        DW      0\nprg_data:\n";
        prog.provides = {"prg_data", "prg_data_count"};
        const link::Result r = link::link(lib, prog, all, link::Layout::PlainZ80, "        ORG &4000\n", d);
        for (const Diagnostic& x : d) std::cerr << "link: " << x.message << "\n";
        CHECK(d.empty());
        if (const char* dump = std::getenv("BASIC_DUMP_SOURCE")) {
            if (std::FILE* f = std::fopen(dump, "w")) {
                std::fputs(r.source.c_str(), f);
                std::fclose(f);
            }
        }
        rt_ = std::make_unique<Z80Routines>(r.source);
        if (!rt_->error().empty()) std::cerr << "assembler:\n" << rt_->error();
        CHECK(rt_->error().empty());
    }

    // HL = kDst, DE = kA, BC = kB; returns the 8 result bytes and the error number (-1: none)
    std::pair<std::uint64_t, int> call(const std::string& name, std::uint64_t a, std::uint64_t b = 0) {
        rt_->write64(kA, a);
        rt_->write64(kB, b);
        rt_->write64(kDst, 0xA5A5A5A5A5A5A5A5u);
        rt_->write8(kErr, 0xFF);
        Z80Regs in;
        in.hl = kDst;
        in.de = kA;
        in.bc = kB;
        in.ix = 0x1357;
        in.iy = 0x2468;
        const auto res = rt_->call(name, in);
        ++g_checks;
        if (!res.ok) {
            ++g_failures;
            std::cerr << res.error << "\n";
            return {0, -2};
        }
        if (res.regs.ix != in.ix || res.regs.iy != in.iy) {
            ++g_failures;
            std::cerr << name << ": IX/IY not preserved\n";
        }
        cycles_ += res.cycles;
        ++calls_;
        carry_ = (res.regs.af & 1) != 0;
        const std::uint8_t e = rt_->read8(kErr);
        return {rt_->read64(kDst), e == 0xFF ? -1 : e};
    }

    std::uint64_t cycles_ = 0, calls_ = 0;
    bool carry_ = false;
    void poke(const std::string& sym, std::uint8_t v) { rt_->write8(rt_->symbol(sym), v); }
    void poke64(std::uint16_t a, std::uint64_t v) { rt_->write64(a, v); }
    Z80Routines& raw() { return *rt_; }
    // the same with the result in place of the operand (HL = DE)
    std::pair<std::uint64_t, int> callInPlace(const std::string& name, std::uint64_t a) {
        rt_->write64(kA, a);
        rt_->write8(kErr, 0xFF);
        Z80Regs in;
        in.hl = kA;
        in.de = kA;
        in.bc = kB;
        in.ix = 0x1357;
        in.iy = 0x2468;
        const auto res = rt_->call(name, in);
        if (!res.ok) return {0, -2};
        const std::uint8_t e = rt_->read8(kErr);
        return {rt_->read64(kA), e == 0xFF ? -1 : e};
    }

private:
    std::unique_ptr<Z80Routines> rt_;
};

int g_shown = 0;
void report(const std::string& what, std::uint64_t a, std::uint64_t b, std::uint64_t got, int gotErr, std::uint64_t want, int wantErr) {
    ++g_checks;
    if (got == want && gotErr == wantErr) return;
    ++g_failures;
    if (g_shown++ < 20)
        std::fprintf(stderr, "%s(%016llX, %016llX): got %016llX err %d, want %016llX err %d\n", what.c_str(),
                     static_cast<unsigned long long>(a), static_cast<unsigned long long>(b), static_cast<unsigned long long>(got), gotErr,
                     static_cast<unsigned long long>(want), wantErr);
}

using Bin = f64 (*)(f64, f64);

void checkBinary(Runtime& rt, const std::string& name, Bin ref, int cases) {
    Rng rng;
    std::vector<std::pair<f64, f64>> ops;
    for (f64 a : edgeCases())
        for (f64 b : edgeCases()) ops.emplace_back(a, b);
    for (int k = 0; k < cases; ++k) {
        const int kind = k % 4;
        if (kind == 0) ops.emplace_back(randomF64(rng, 1, 2046), randomF64(rng, 1, 2046));
        else if (kind == 1) {
            const int e = rng.range(900, 1150);
            ops.emplace_back(randomF64(rng, e, e + 2), randomF64(rng, e - 2, e));
        } else if (kind == 2) {
            const f64 a = randomF64(rng, 1000, 1100);
            ops.emplace_back(a, a ^ (rng.next() & 0x8000000000000007u));
        } else {
            ops.emplace_back(randomF64(rng, 1015, 1035), randomF64(rng, 1015, 1035));
        }
    }
    for (const auto& [a, b] : ops) {
        g_error = -1;
        const f64 want = ref(a, b);
        const int wantErr = g_error;
        const auto [got, err] = rt.call(name, a, b);
        report(name, a, b, got, err, want, wantErr);
    }
}

using Un = f64 (*)(f64);

void checkUnary(Runtime& rt, const std::string& name, Un ref, int cases, std::vector<f64> extra = {}) {
    Rng rng;
    std::vector<f64> vals = edgeCases();
    vals.insert(vals.end(), extra.begin(), extra.end());
    for (int k = 0; k < cases; ++k) {
        const int kind = k % 3;
        if (kind == 0) vals.push_back(randomF64(rng, 1, 2046));
        else if (kind == 1) vals.push_back(randomF64(rng, 960, 1090));
        else vals.push_back(randomF64(rng, 1010, 1040));
    }
    for (f64 x : vals) {
        g_error = -1;
        const f64 want = ref(x);
        const int wantErr = g_error;
        const auto [got, err] = rt.call(name, x);
        report(name, x, 0, got, err, want, wantErr);
        const auto [got2, err2] = rt.callInPlace(name, x);
        report(name + " in place", x, 0, got2, err2, want, wantErr);
    }
}

}  // namespace

int main() {
    int cases = 2000;
    if (const char* c = std::getenv("BASIC_CONFORMANCE_CASES")) cases = std::atoi(c);
    Runtime rt;
    checkBinary(rt, "fp_add", fp_add, cases);
    checkBinary(rt, "fp_sub", fp_sub, cases);
    checkBinary(rt, "fp_mul", fp_mul, cases);
    checkBinary(rt, "fp_div", fp_div, cases);
    {  // fp_cmp -> 32 bits
        Rng rng;
        std::vector<std::pair<f64, f64>> ops;
        for (f64 a : edgeCases())
            for (f64 b : edgeCases()) ops.emplace_back(a, b);
        for (int k = 0; k < cases; ++k) {
            const f64 a = randomF64(rng, 1000, 1050);
            ops.emplace_back(a, k % 3 == 0 ? a : k % 3 == 1 ? a ^ 1 : randomF64(rng, 1000, 1050));
        }
        for (const auto& [a, b] : ops) {
            const auto [got, err] = rt.call("fp_cmp", a, b);
            report("fp_cmp", a, b, got & 0xFFFFFFFFu, err, static_cast<std::uint32_t>(fp_cmp(a, b)), -1);
        }
    }
    {  // fp_from_i32
        Rng rng;
        std::vector<std::int32_t> vals = {0, 1, -1, 2, 7, -7, 255, 32767, -32768, INT32_MAX, INT32_MIN, 0x12345678, -0x12345678};
        for (int k = 0; k < cases; ++k) vals.push_back(static_cast<std::int32_t>(rng.next() >> (rng.next() % 32)));
        for (std::int32_t v : vals) {
            const auto [got, err] = rt.call("fp_from_i32", static_cast<std::uint32_t>(v));
            report("fp_from_i32", static_cast<std::uint32_t>(v), 0, got, err, fp_from_i32(v), -1);
        }
    }
    {  // fp_to_i32 / fp_to_i16 / fp_to_u8 -> 32 bits
        Rng rng;
        std::vector<f64> vals = edgeCases();
        for (int k = 0; k < cases; ++k) vals.push_back(randomF64(rng, 1000, 1060));
        for (f64 x : vals) {
            for (int t = 0; t < 3; ++t) {
                g_error = -1;
                std::uint32_t want = 0;
                const char* name = t == 0 ? "fp_to_i32" : t == 1 ? "fp_to_i16" : "fp_to_u8";
                if (t == 0) want = static_cast<std::uint32_t>(fp_to_i32(x));
                else if (t == 1) want = static_cast<std::uint32_t>(static_cast<std::int32_t>(fp_to_i16(x)));
                else want = fp_to_u8(x);
                const int wantErr = g_error;
                const auto [got, err] = rt.call(name, x);
                report(name, x, 0, got & 0xFFFFFFFFu, err, want, wantErr);
            }
        }
    }
    checkUnary(rt, "fp_sqrt", fp_sqrt, cases);
    checkUnary(rt, "fp_trunc", fp_trunc, cases, {0x3FE0000000000000u, 0xC00C000000000000u, 0x432FFFFFFFFFFFFFu});
    checkUnary(rt, "fp_int", fp_int, cases, {0x3FE0000000000000u, 0xC00C000000000000u, 0xBFE0000000000000u});
    checkUnary(rt, "fp_frac", fp_frac, cases, {0x3FE0000000000000u, 0xC00C000000000000u});
    checkUnary(rt, "fp_cint", fp_cint, cases,
               {0x3FE0000000000000u, 0xBFE0000000000000u, 0x3FDFFFFFFFFFFFFFu, 0x4004000000000000u, 0xC004000000000000u,
                0x3CA0000000000000u, 0x3C90000000000000u});
    checkUnary(rt, "fp_canon", fp_canon, cases,
               {0x8000000000000000u, 0x000FFFFFFFFFFFFFu, 0x7FF0000000000000u, 0xFFF8000000000000u, 0x7FF0000000000001u});
    checkUnary(rt, "fp_neg", fp_neg, cases);
    checkUnary(rt, "fp_abs", [](f64 x) -> f64 { return x & 0x7FFFFFFFFFFFFFFFu; }, cases);
    {  // fp_approx_eq -> 32-bit 1/0
        Rng rng;
        std::vector<std::pair<f64, f64>> ops;
        for (f64 a : edgeCases())
            for (f64 b : edgeCases()) ops.emplace_back(a, b);
        for (int k = 0; k < cases; ++k) {
            const f64 a = randomF64(rng, 1000, 1050);
            const std::uint64_t d = rng.next() & ((std::uint64_t{1} << rng.range(1, 30)) - 1);
            ops.emplace_back(a, k % 2 == 0 ? a + d : a - d);
        }
        for (const auto& [a, b] : ops) {
            const auto [got, err] = rt.call("fp_approx_eq", a, b);
            report("fp_approx_eq", a, b, got & 0xFFFFFFFFu, err, static_cast<std::uint32_t>(fp_approx_eq(a, b)), -1);
        }
    }
    {  // 32-bit integers of the generated code (zi): results, overflow in CF, comparisons
        Rng rng;
        std::vector<std::int32_t> vals = {0, 1, -1, 2, -2, 7, 46341, -46341, 65535, 65536, INT32_MAX, INT32_MIN, INT32_MAX - 1, INT32_MIN + 1};
        for (int k = 0; k < 60; ++k) vals.push_back(static_cast<std::int32_t>(rng.next() >> (rng.next() % 40)));
        const auto u = [](std::int32_t v) { return static_cast<std::uint32_t>(v); };
        for (std::int32_t a : vals)
            for (std::int32_t b : vals) {
                const std::int64_t sum = std::int64_t{a} + b, dif = std::int64_t{a} - b, pro = std::int64_t{a} * b;
                const auto fits = [](std::int64_t v) { return v >= INT32_MIN && v <= INT32_MAX; };
                auto r = rt.call("zi_add", u(a), u(b));
                report("zi_add", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, static_cast<std::uint32_t>(sum), -1);
                r = rt.call("zi_addv", u(a), u(b));
                report("zi_addv carry", u(a), u(b), rt.carry_, 0, !fits(sum), 0);
                r = rt.call("zi_sub", u(a), u(b));
                report("zi_sub", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, static_cast<std::uint32_t>(dif), -1);
                r = rt.call("zi_subv", u(a), u(b));
                report("zi_subv carry", u(a), u(b), rt.carry_, 0, !fits(dif), 0);
                r = rt.call("zi_mulv", u(a), u(b));
                report("zi_mulv carry", u(a), u(b), rt.carry_, 0, !fits(pro), 0);
                if (fits(pro)) report("zi_mulv", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, static_cast<std::uint32_t>(pro), -1);
                r = rt.call("rt_imul", u(a), u(b));
                report("rt_imul", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, u(a) * u(b), -1);
                if (b != 0) {
                    r = rt.call("rt_idiv", u(a), u(b));
                    const std::int64_t q = std::int64_t{a} / b, m = std::int64_t{a} % b;
                    report("rt_idiv", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, static_cast<std::uint32_t>(q), -1);
                    r = rt.call("rt_imod", u(a), u(b));
                    report("rt_imod", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, static_cast<std::uint32_t>(m), -1);
                } else {
                    r = rt.call("rt_idiv", u(a), u(b));
                    report("rt_idiv /0", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, 0, 0);
                }
                r = rt.call("zi_and", u(a), u(b));
                report("zi_and", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, u(a & b), -1);
                r = rt.call("zi_or", u(a), u(b));
                report("zi_or", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, u(a | b), -1);
                r = rt.call("zi_xor", u(a), u(b));
                report("zi_xor", u(a), u(b), r.first & 0xFFFFFFFFu, r.second, u(a ^ b), -1);
                const bool want[6] = {a == b, a != b, a < b, a <= b, a > b, a >= b};
                for (int c = 0; c < 6; ++c) {
                    rt.poke("zi_cmp_code", static_cast<std::uint8_t>(c));
                    r = rt.call("zi_cmp", u(a), u(b));
                    report("zi_cmp " + std::to_string(c), u(a), u(b), r.first & 0xFFFFFFFFu, r.second, want[c] ? 0xFFFFFFFFu : 0u, -1);
                }
            }
        for (std::int32_t a : vals) {
            auto r = rt.call("zi_neg", u(a));
            report("zi_neg", u(a), 0, r.first & 0xFFFFFFFFu, r.second, static_cast<std::uint32_t>(0u - u(a)), -1);
            r = rt.call("zi_not", u(a));
            report("zi_not", u(a), 0, r.first & 0xFFFFFFFFu, r.second, ~u(a), -1);
            r = rt.call("zi_mov", u(a));
            report("zi_mov", u(a), 0, r.first & 0xFFFFFFFFu, r.second, u(a), -1);
            const bool w = a >= -32768 && a <= 32767, by = a >= 0 && a <= 255;
            r = rt.call("rt_conv_word", u(a));
            report("rt_conv_word", u(a), 0, r.first & 0xFFFFFFFFu, r.second, w ? u(a) : 0u, w ? -1 : 4);
            r = rt.call("rt_conv_byte", u(a));
            report("rt_conv_byte", u(a), 0, r.first & 0xFFFFFFFFu, r.second, by ? u(a) : 0u, by ? -1 : 3);
        }
    }
    {  // rt_fmt_flt: the text of PRINT/STR$ for 2..13 digits
        Rng rng;
        std::vector<f64> vals = edgeCases();
        const f64 more[] = {0x3FB999999999999Au /* 0.1 */, 0x4059000000000000u /* 100 */, 0x42D6BCC41E900000u /* 1e14 */,
                            0x42C6BCC41E900000u /* 5e13 */, 0x3EE4F8B588E368F1u /* 1e-5 */, 0x3EB0C6F7A0B5ED8Du /* 1e-6 */,
                            0x7FE1CCF385EBC8A0u /* 1e308 */, 0x0010000000000000u, 0x3FE0000000000000u, 0x4024000000000000u};
        vals.insert(vals.end(), std::begin(more), std::end(more));
        for (int k = 0; k < cases / 4; ++k) vals.push_back(randomF64(rng, k % 2 == 0 ? 1 : 980, k % 2 == 0 ? 2046 : 1080));
        for (f64 x : vals)
            for (int digits : {13, 2, 7}) {
                char want[64];
                const int n = rt_fmt_flt(want, x, digits);
                rt.poke64(kA, x);
                Z80Regs in;
                in.hl = 0xE100;
                in.de = kA;
                in.af = static_cast<std::uint16_t>(digits << 8);
                in.ix = 0x1357;
                in.iy = 0x2468;
                const auto res = rt.raw().call("rt_fmt_flt", in, 50'000'000);
                std::string got;
                if (res.ok)
                    for (int i = 0; i < res.regs.bc && i < 60; ++i) got += static_cast<char>(rt.raw().read8(static_cast<std::uint16_t>(0xE100 + i)));
                ++g_checks;
                if (!res.ok || got != std::string(want, static_cast<std::size_t>(n))) {
                    ++g_failures;
                    if (g_shown++ < 20)
                        std::fprintf(stderr, "rt_fmt_flt(%016llX, %d): got \"%s\" want \"%s\" %s\n", static_cast<unsigned long long>(x), digits,
                                     got.c_str(), want, res.error.c_str());
                }
            }
    }
    {  // fp_round(x, n) and the wrap of FOR with an integer variable
        Rng rng;
        std::vector<f64> vals = edgeCases();
        for (int k = 0; k < cases / 8; ++k) vals.push_back(randomF64(rng, 990, 1080));
        for (f64 x : vals)
            for (std::int32_t n : {0, 1, 2, 3, 5, 12, 20, -1, -2, -5, -20, 400, -400}) {
                g_error = -1;
                const f64 want = fp_round(x, n);
                const auto [got, err] = rt.call("fp_round", x, static_cast<std::uint32_t>(n));
                report("fp_round " + std::to_string(n), x, 0, got, err, want, g_error);
            }
        for (int k = 0; k < cases / 4; ++k) {  // |x| < 2^32
            const f64 x = randomF64(rng, 1023, 1053);
            g_error = -1;
            f64 t = fp_trunc(x);
            if (fp_cmp(t, 0x41E0000000000000u) >= 0) t = fp_sub(t, 0x41F0000000000000u);
            else if (fp_cmp(t, 0xC1E0000000000000u) < 0) t = fp_add(t, 0x41F0000000000000u);
            const std::uint32_t want = static_cast<std::uint32_t>(fp_to_i32(t));
            const auto [got, err] = rt.call("rt_for_wrap_int", x);
            report("rt_for_wrap_int", x, 0, got & 0xFFFFFFFFu, err, want, g_error);
        }
    }
    {  // rt_bits: BTST/BCLR/BSET/BCHG, SHL/SHR/ROL/ROR in 8, 16, 32 bits, SWAP, WORD, CARD, BYTE (brt)
        const std::uint16_t kW = 0xE030, kK = 0xE038, argp = rt.raw().symbol("rt_argp");
        rt.raw().write8(argp, kW & 0xFF);
        rt.raw().write8(argp + 1, kW >> 8);
        rt.raw().write8(argp + 2, kK & 0xFF);
        rt.raw().write8(argp + 3, kK >> 8);
        const std::uint32_t vals[] = {0u, 1u, 0x80u, 0xFFu, 0x8000u, 0xFFFFu, 0x12345678u, 0x80000001u, 0xFFFFFFFFu, 0xA5C3E10Fu};
        for (std::uint32_t v : vals) {
            for (std::uint32_t n : {0u, 1u, 3u, 7u, 8u, 15u, 16u, 17u, 31u, 32u, 33u, 0xFFFFFFFFu}) {
                const std::uint32_t bit = 1u << (n & 31);
                report("rt_btst", v, n, rt.call("rt_btst", v, n).first & 0xFFFFFFFFu, -1, (v & bit) != 0 ? 0xFFFFFFFFu : 0u, -1);
                report("rt_bclr", v, n, rt.call("rt_bclr", v, n).first & 0xFFFFFFFFu, -1, v & ~bit, -1);
                report("rt_bset", v, n, rt.call("rt_bset", v, n).first & 0xFFFFFFFFu, -1, v | bit, -1);
                report("rt_bchg", v, n, rt.call("rt_bchg", v, n).first & 0xFFFFFFFFu, -1, v ^ bit, -1);
                for (int width : {8, 16, 32})
                    for (int kind = 0; kind < 4; ++kind) {
                        rt.raw().write32(kW, static_cast<std::uint32_t>(width));
                        rt.raw().write32(kK, static_cast<std::uint32_t>(kind));
                        const std::uint32_t mask = width == 32 ? 0xFFFFFFFFu : (1u << width) - 1;
                        std::uint32_t w = v & mask;
                        std::uint32_t c = n & 31;
                        if (kind >= 2) {
                            c %= static_cast<std::uint32_t>(width);
                            if (c != 0) w = kind == 2 ? ((w << c) | (w >> (width - c))) & mask : ((w >> c) | (w << (width - c))) & mask;
                        } else {
                            w = c >= static_cast<std::uint32_t>(width) ? 0 : (kind == 0 ? (w << c) & mask : w >> c);
                        }
                        if (width == 16) w = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(w)));
                        report("rt_shift " + std::to_string(width) + "/" + std::to_string(kind), v, n,
                               rt.call("rt_shift", v, n).first & 0xFFFFFFFFu, -1, w, -1);
                    }
            }
            report("rt_swap", v, 0, rt.call("rt_swap", v).first & 0xFFFFFFFFu, -1, (v << 16) | (v >> 16), -1);
            report("rt_word", v, 0, rt.call("rt_word", v).first & 0xFFFFFFFFu, -1,
                   static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))), -1);
            report("rt_card", v, 0, rt.call("rt_card", v).first & 0xFFFFFFFFu, -1, v & 0xFFFF, -1);
            report("rt_byte", v, 0, rt.call("rt_byte", v).first & 0xFFFFFFFFu, -1, v & 0xFF, -1);
        }
    }
    {  // rt_str_radix: HEX$/BIN$/OCT$ with width (brt::radix; the Z80 caps the width at 32767)
        const std::uint16_t kW = 0xE030, argp = rt.raw().symbol("rt_argp");
        rt.raw().write8(argp, kW & 0xFF);
        rt.raw().write8(argp + 1, kW >> 8);
        for (std::uint32_t v : {0u, 1u, 7u, 8u, 255u, 0x1234u, 0x80000000u, 0xFFFFFFFFu, 0xDEADBEEFu})
            for (unsigned base : {2u, 8u, 16u})
                for (std::int32_t width : {0, -1, -100000, 1, 3, 8, 32, 33, 40, 300, 100000}) {
                    std::string want;
                    std::uint32_t x = v;
                    do {
                        want.insert(want.begin(), "0123456789ABCDEF"[x % base]);
                        x /= base;
                    } while (x != 0);
                    const std::int32_t w = width > 32767 ? 32767 : width;
                    if (w > 0) {
                        if (static_cast<std::int32_t>(want.size()) < w) want.insert(0, static_cast<std::size_t>(w) - want.size(), '0');
                        else want = want.substr(want.size() - static_cast<std::size_t>(w));
                    }
                    if (w > 2000) continue;  // more than the heap of the test holds
                    rt.raw().write32(kA, v);
                    rt.raw().write32(kB, base);
                    rt.raw().write32(kW, static_cast<std::uint32_t>(width));
                    Z80Regs in;
                    in.de = kA;
                    in.bc = kB;
                    const auto res = rt.raw().call("rt_str_radix", in);
                    std::string got = "(no return)";
                    if (res.ok) {
                        const std::uint16_t d = res.regs.hl;
                        const std::uint16_t at = static_cast<std::uint16_t>(rt.raw().read8(d) | rt.raw().read8(d + 1) << 8);
                        const int len = rt.raw().read8(d + 2) | rt.raw().read8(d + 3) << 8;
                        got.clear();
                        for (int k = 0; k < len; ++k) got += static_cast<char>(rt.raw().read8(static_cast<std::uint16_t>(at + k)));
                    }
                    rt.raw().write32(kB, 0);
                    Z80Regs rel;
                    rel.de = kB;
                    (void)rt.raw().call("rt_str_release", rel);
                    ++g_checks;
                    if (got != want) {
                        ++g_failures;
                        std::cerr << "rt_str_radix(" << v << ", " << base << ", " << width << "): got " << got << ", want " << want << "\n";
                    }
                }
    }
    {  // PRINT USING: random formats from building blocks, numbers and strings, all four modes; the
       // buffer variants (rt_using_numb ...) against runtime/ref/rt_using.c; STR$(x,y,z), STR$(x,n)
        Rng r;
        int shown = 0;
        auto put = [&](std::uint16_t at, const std::string& t) {
            for (std::size_t k = 0; k < t.size(); ++k) rt.raw().write8(static_cast<std::uint16_t>(at + k), static_cast<std::uint8_t>(t[k]));
        };
        auto desc = [&](std::uint16_t d, std::uint16_t at, const std::string& t) {
            put(at, t);
            rt.raw().write8(d, at & 0xFF);
            rt.raw().write8(d + 1, at >> 8);
            rt.raw().write8(d + 2, t.size() & 0xFF);
            rt.raw().write8(d + 3, static_cast<std::uint8_t>(t.size() >> 8));
        };
        auto call = [&](const char* name, Z80Regs in, std::string* text) {
            in.ix = 0x1357;
            in.iy = 0x2468;
            const auto res = rt.raw().call(name, in, 50'000'000);
            const bool ok = res.ok && res.regs.ix == 0x1357 && res.regs.iy == 0x2468;
            if (text != nullptr && ok) {
                text->clear();
                for (std::uint16_t k = 0; k < res.regs.bc && k < 5000; ++k)
                    *text += static_cast<char>(rt.raw().read8(static_cast<std::uint16_t>(res.regs.hl + k)));
            }
            if (!ok) std::cerr << name << ": " << (res.ok ? "IX/IY not preserved" : res.error) << "\n";
            return ok;
        };
        auto strOf = [&](std::uint16_t d) {
            std::string t;
            const std::uint16_t at = static_cast<std::uint16_t>(rt.raw().read8(d) | rt.raw().read8(d + 1) << 8);
            const int len = rt.raw().read8(d + 2) | rt.raw().read8(d + 3) << 8;
            for (int k = 0; k < len; ++k) t += static_cast<char>(rt.raw().read8(static_cast<std::uint16_t>(at + k)));
            return t;
        };
        static const char* blocks[] = {"#", "##", "###", "####", "#,###", ",", ".", ".#", ".##", "*", "**", "$", "+", "-", "^^^^", "^^^", "^^^^^",
                                       "^^", "&", "!", "\\\\", "\\  \\", "_", "_#", "x", " ", "abc", "#.#", "+##.##", "##.##-", "**$#,##.##",
                                       "#.##^^^^", "###.##^^^^", "-#.#", "$##", "#.###,##", "##,##", "+.##", "*$#", "_!", "%",
                                       "#.##^^^^^^^^^^^^", "##########.##########", "^^^^^^^^^^^^"};
        const int nblocks = static_cast<int>(sizeof blocks / sizeof blocks[0]);
        static const f64 special[] = {0, 0x8000000000000000u, 0x3FF0000000000000u, 0xBFF0000000000000u, 0x3FC0000000000000u,
                                      0x4004000000000000u, 0x4058FFAE147AE148u, 0x4415AF1D78B58C40u, 0x3DDB7CDFD9D7BDBBu,
                                      0x7E37E43C8800759Cu, 0x408F400000000000u, 0xC0C3880000000000u, 0x3FE0000000000000u,
                                      0x3FEFFFFFFFFFFFFFu, 0x4023FFFFFFFFFFFFu, 0x7FEFFFFFFFFFFFFFu, 0x0010000000000000u};
        const int nspecial = static_cast<int>(sizeof special / sizeof special[0]);
        static char buf[4096];
        for (int i = 0; i < cases / 2; ++i) {
            std::string f;
            const int nb = r.range(1, 4);
            for (int k = 0; k < nb; ++k) f += blocks[r.range(0, nblocks - 1)];
            const int mode = r.range(0, 3);
            rt_using u;
            rt_using_begin(&u, f.data(), static_cast<std::int32_t>(f.size()), mode);
            rt.raw().write32(kA, static_cast<std::uint32_t>(mode));
            Z80Regs in;
            in.de = kA;
            if (!call("rt_set_mode", in, nullptr)) break;
            desc(0xE040, 0xE200, f);
            in.de = 0xE040;
            if (!call("rt_using_begin", in, nullptr)) break;
            std::string expect, got, part;
            const int nv = r.range(1, 3);
            for (int k = 0; k < nv; ++k) {
                Z80Regs v;
                if (r.range(0, 4) == 0) {
                    static const char* strs[] = {"", "a", "hello world", "xy"};
                    const std::string sv = strs[r.range(0, 3)];
                    expect += std::string(buf, static_cast<std::size_t>(rt_using_str(&u, buf, 512, sv.data(), static_cast<std::int32_t>(sv.size()))));
                    desc(0xE048, 0xE300, sv);
                    v.de = 0xE048;
                    if (!call("rt_using_strb", v, &part)) break;
                } else {
                    const f64 x = r.range(0, 2) == 0 ? special[r.range(0, nspecial - 1)] : randomF64(r, 1023 - 40, 1023 + 70);
                    expect += std::string(buf, static_cast<std::size_t>(rt_using_num(&u, buf, 512, x)));
                    rt.raw().write64(kA, x);
                    v.de = kA;
                    if (!call("rt_using_numb", v, &part)) break;
                }
                got += part;
            }
            expect += std::string(buf, static_cast<std::size_t>(rt_using_end(&u, buf, 512)));
            if (call("rt_using_endb", Z80Regs{}, &part)) got += part;
            ++g_checks;
            if (got != expect) {
                ++g_failures;
                if (++shown <= 10) std::cerr << "PRINT USING \"" << f << "\" mode " << mode << ": \"" << got << "\", expected \"" << expect << "\"\n";
            }
        }
        const std::uint16_t kZ = 0xE030, argp = rt.raw().symbol("rt_argp");
        for (int i = 0; i < cases / 4; ++i) {
            const f64 x = r.range(0, 2) == 0 ? special[r.range(0, nspecial - 1)] : randomF64(r, 1023 - 20, 1023 + 40);
            const int mode = r.range(0, 3);
            const std::int32_t y = i % 50 == 0 ? r.range(250, 300) : r.range(-2, 30), z = i % 40 == 0 ? r.range(250, 400) : r.range(-2, 12),
                               n = i % 50 == 0 ? r.range(100, 300) : r.range(-2, 30);
            rt.raw().write32(kA, static_cast<std::uint32_t>(mode));
            Z80Regs in;
            in.de = kA;
            call("rt_set_mode", in, nullptr);
            const std::string expect(buf, static_cast<std::size_t>(rt_str_fmt(buf, 512, x, y, z, mode)));
            rt.raw().write64(kA, x);
            rt.raw().write32(kB, static_cast<std::uint32_t>(y));
            rt.raw().write32(kZ, static_cast<std::uint32_t>(z));
            rt.raw().write8(argp, kZ & 0xFF);
            rt.raw().write8(argp + 1, kZ >> 8);
            in.de = kA;
            in.bc = kB;
            const auto res = rt.raw().call("rt_str_fmt", in, 50'000'000);
            const std::string got = res.ok ? strOf(res.regs.hl) : "(" + res.error + ")";
            const std::string expectN(buf, static_cast<std::size_t>(rt_str_n(buf, 512, x, n)));
            rt.raw().write32(kB, static_cast<std::uint32_t>(n));
            const auto resN = rt.raw().call("rt_str_n", in, 5'000'000);
            const std::string gotN = resN.ok ? strOf(resN.regs.hl) : "(" + resN.error + ")";
            rt.raw().write32(kB, 0);
            Z80Regs rel;
            rel.de = kB;
            (void)rt.raw().call("rt_str_release", rel);
            g_checks += 2;
            if (got != expect || gotN != expectN) {
                ++g_failures;
                if (++shown <= 10)
                    std::cerr << "STR$(" << std::hex << x << std::dec << "," << y << "," << z << ") mode " << mode << ": \"" << got << "\", expected \""
                              << expect << "\"; STR$(x," << n << "): \"" << gotN << "\", expected \"" << expectN << "\"\n";
            }
        }
    }
    {  // MAT (runtime/z80/rt_mat.s) against runtime/ref/rt_mat.c: the same data as host matrix and in
       // the emulator (column-major like an array: rs = 8, cs = 8 * rows), the same view, bit for bit
        Rng r;
        const std::uint16_t views = rt.raw().symbol("rt_mat_v");
        int shown = 0;
        struct MatCase {
            std::uint16_t addr;
            std::int32_t rows, cols;
            std::vector<f64> data;  // column-major
            rt_mat host() { return rt_mat{data.data(), rows, cols, 1, rows}; }
        };
        auto fail = [&](const std::string& what) {
            ++g_failures;
            if (++shown <= 10) std::cerr << "rt_mat: " << what << "\n";
        };
        auto value = [&](int kind) -> f64 {
            switch (kind) {
                case 0: return fp_from_i32(r.range(-3, 3));
                case 1: return randomF64(r, 1000, 1046) | (r.next() & 1 ? f64{1} << 63 : 0);
                case 2: return fp_from_i32(r.range(-2, 2) * (1 << r.range(0, 3)));
                default: return fp_div(fp_from_i32(r.range(-9, 9)), fp_from_i32(r.range(1, 7)));
            }
        };
        auto make = [&](std::uint16_t addr, std::int32_t rows, std::int32_t cols, int kind) {
            MatCase m{addr, rows, cols, std::vector<f64>(static_cast<std::size_t>(std::max(rows * cols, 1)))};
            for (f64& x : m.data) x = value(kind);
            return m;
        };
        auto put = [&](const MatCase& m) {
            for (std::size_t k = 0; k < m.data.size(); ++k) rt.raw().write64(static_cast<std::uint16_t>(m.addr + 8 * k), m.data[k]);
        };
        auto w16 = [&](std::uint16_t a, std::int32_t v) {
            rt.raw().write8(a, static_cast<std::uint8_t>(v));
            rt.raw().write8(static_cast<std::uint16_t>(a + 1), static_cast<std::uint8_t>(v >> 8));
        };
        auto view = [&](int k, const MatCase& m, int dims = 2) {
            const std::uint16_t v = static_cast<std::uint16_t>(views + 12 * k);
            w16(v, m.addr);
            w16(static_cast<std::uint16_t>(v + 2), m.rows);
            w16(static_cast<std::uint16_t>(v + 4), m.cols);
            w16(static_cast<std::uint16_t>(v + 6), 8);
            w16(static_cast<std::uint16_t>(v + 8), 8 * m.rows);
            rt.raw().write8(static_cast<std::uint16_t>(v + 10), static_cast<std::uint8_t>(dims));
        };
        auto same = [&](const char* what, const MatCase& m) {  // emulator memory = host data
            ++g_checks;
            for (std::size_t k = 0; k < m.data.size(); ++k) {
                const f64 got = rt.raw().read64(static_cast<std::uint16_t>(m.addr + 8 * k));
                if (got != m.data[k]) {
                    char b[160];
                    std::snprintf(b, sizeof b, "%s element %zu: %016llX, expected %016llX", what, k, static_cast<unsigned long long>(got),
                                  static_cast<unsigned long long>(m.data[k]));
                    fail(b);
                    return;
                }
            }
        };
        auto scalar = [&](const char* what, f64 got, int err, f64 want, int werr) {
            ++g_checks;
            if (got != want || err != werr) {
                char b[160];
                std::snprintf(b, sizeof b, "%s: %016llX err %d, expected %016llX err %d", what, static_cast<unsigned long long>(got), err,
                              static_cast<unsigned long long>(want), werr);
                fail(b);
            }
        };
        const std::uint16_t A = 0xE400, B = 0xE600, C = 0xE800, T = 0xEA00;
        for (int round = 0; round < cases / 20; ++round) {
            const int kind = r.range(0, 3);
            const std::int32_t n = r.range(0, 6), m = r.range(1, 5), q = r.range(1, 5);
            MatCase a = make(A, n, n, kind);
            if (n > 1 && r.range(0, 2) == 0)  // the last row the sum of the others
                for (std::int32_t j = 0; j < n; ++j) {
                    f64 sum = 0;
                    for (std::int32_t i = 0; i + 1 < n; ++i) sum = fp_add(sum, a.data[static_cast<std::size_t>(i + j * n)]);
                    a.data[static_cast<std::size_t>(n - 1 + j * n)] = sum;
                }
            if (round % 8 == 0)
                for (f64& x : a.data) x = 0;
            for (int quick = 0; quick < 3; ++quick) {
                put(a);
                view(0, a);
                const rt_mat ha = a.host();
                g_error = -1;
                const f64 want = quick == 2 ? rt_mat_rank(&ha) : rt_mat_det(&ha, quick);
                const int werr = g_error;
                const auto [got, err] = rt.call(quick == 2 ? "rt_mat_rank" : "rt_mat_det", static_cast<std::uint32_t>(quick & 1));
                scalar(quick == 2 ? "rank" : quick ? "qdet" : "det", got, err, want, werr);
                same("det input unchanged", a);
            }
            {  // INV into a target that is sometimes of the wrong size
                MatCase t = make(T, r.range(0, 4) == 0 ? n + 1 : n, n, 1);
                put(a);
                put(t);
                view(0, t);
                view(1, a);
                const rt_mat ht = t.host(), ha = a.host();
                g_error = -1;
                rt_mat_inv(&ht, &ha);
                const int werr = g_error;
                const int err = rt.call("rt_mat_inv", 0).second;
                scalar("inv error", 0, err, 0, werr);
                same("inv", t);
            }
            {  // MUL, DOT, TRIPLE
                MatCase b = make(B, n, r.range(0, 5) == 0 ? q + 1 : q, kind), c = make(C, q, m, kind), t = make(T, n, m, 1);
                put(b);
                put(c);
                put(t);
                view(0, t);
                view(1, b);
                view(2, c);
                const rt_mat ht = t.host(), hb = b.host(), hc = c.host();
                g_error = -1;
                rt_mat_mul(&ht, &hb, &hc);
                int werr = g_error;
                int err = rt.call("rt_mat_mul", 0).second;
                scalar("mul error", 0, err, 0, werr);
                same("mul", t);
                MatCase u = make(A, 1, q, kind), v = make(B, 1, r.range(0, 4) == 0 ? q + 1 : q, kind), w = make(C, q, m, kind),
                        z = make(T, 1, m, kind);
                put(u);
                put(v);
                put(w);
                put(z);
                view(0, u, 1);
                view(1, v, 1);
                const rt_mat hu = u.host(), hv = v.host(), hw = w.host(), hz = z.host();
                g_error = -1;
                f64 want = rt_mat_dot(&hu, &hv);
                werr = g_error;
                auto res = rt.call("rt_mat_dot", 0);
                scalar("dot", res.first, res.second, want, werr);
                view(1, w);
                view(2, z, 1);
                g_error = -1;
                want = rt_mat_triple(&hu, &hw, &hz);
                werr = g_error;
                res = rt.call("rt_mat_triple", 0);
                scalar("triple", res.first, res.second, want, werr);
            }
            for (int byCol = 0; byCol < 2; ++byCol) {  // NORM, sometimes with a zero line
                MatCase t = make(A, n, m, kind);
                if (n > 0 && r.range(0, 3) == 0)
                    for (std::int32_t j = 0; j < m; ++j) t.data[static_cast<std::size_t>(n - 1 + j * n)] = 0;
                put(t);
                view(0, t);
                const rt_mat ht = t.host();
                g_error = -1;
                rt_mat_norm(&ht, byCol);
                const int werr = g_error;
                const int err = rt.call("rt_mat_norm", static_cast<std::uint32_t>(byCol)).second;
                scalar("norm error", 0, err, 0, werr);
                same("norm", t);
            }
            {  // TRANS, ADD/SUB, scalar operations
                MatCase s2 = make(A, n, m, kind), t = make(T, r.range(0, 4) == 0 ? m + 1 : m, n, 1);
                put(s2);
                put(t);
                view(0, t);
                view(1, s2);
                const rt_mat ht = t.host(), hs = s2.host();
                g_error = -1;
                rt_mat_trans(&ht, &hs);
                int werr = g_error;
                int err = rt.call("rt_mat_trans", 0).second;
                scalar("trans error", 0, err, 0, werr);
                same("trans", t);
                MatCase x = make(B, n, m, kind), y = make(C, n, r.range(0, 4) == 0 ? m + 1 : m, kind), z = make(T, n, m, 1);
                put(x);
                put(y);
                put(z);
                view(0, z);
                view(1, x);
                view(2, y);
                const int sub = r.range(0, 1);
                const rt_mat hz = z.host(), hx = x.host(), hy = y.host();
                g_error = -1;
                rt_mat_addsub(&hz, &hx, &hy, sub);
                werr = g_error;
                err = rt.call("rt_mat_addsub", static_cast<std::uint32_t>(sub)).second;
                scalar("addsub error", 0, err, 0, werr);
                same("addsub", z);
                const int op = r.range(0, 2);
                const f64 v = value(kind);
                view(0, x);
                rt_mat_scalar(&hx, v, op);
                (void)rt.call("rt_mat_scalar", v, static_cast<std::uint32_t>(op));
                same("scalar", x);
            }
            {  // CPY/XCPY: source and destination at random offsets (views moved like rt_mat_at)
                MatCase s2 = make(A, r.range(0, 5), r.range(0, 5), kind), d = make(T, r.range(0, 5), r.range(0, 5), 1);
                const std::int32_t si = r.range(0, 2), sj = r.range(0, 2), di = r.range(0, 2), dj = r.range(0, 2);
                const std::int32_t h = r.range(-1, 4), w = r.range(-1, 4);
                const int tr = r.range(0, 1);
                put(s2);
                put(d);
                auto moved = [&](int k, const MatCase& c, std::int32_t i, std::int32_t j) {
                    view(k, c);
                    const std::uint16_t at = static_cast<std::uint16_t>(views + 12 * k);
                    w16(at, c.addr + 8 * (i + j * c.rows));
                    w16(static_cast<std::uint16_t>(at + 2), c.rows - i);
                    w16(static_cast<std::uint16_t>(at + 4), c.cols - j);
                };
                moved(0, d, di, dj);
                moved(1, s2, si, sj);
                const rt_mat hd = d.host(), hs = s2.host();
                rt_mat_cpy(&hd, di, dj, &hs, si, sj, h, w, tr);
                const std::uint16_t argp = rt.raw().symbol("rt_argp"), kTr = 0xE030;
                rt.raw().write32(kTr, static_cast<std::uint32_t>(tr));
                w16(argp, kTr);
                (void)rt.call("rt_mat_cpy", static_cast<std::uint32_t>(h), static_cast<std::uint32_t>(w));
                same(tr ? "xcpy" : "cpy", d);
            }
        }
    }
    {  // rt_mkstr: MKD$/CVD (GFA 8-byte float), MKS$/CVS (IEEE single), MKI$/MKL$/CVI/CVL
        auto mk = [&](const char* name, std::uint64_t x, int n, std::vector<std::uint8_t>& got, int& err) {
            rt.raw().write64(kA, x);
            rt.raw().write8(kErr, 0xFF);
            Z80Regs in;
            in.de = kA;
            const auto res = rt.raw().call(name, in);
            ++g_checks;
            err = rt.raw().read8(kErr) == 0xFF ? -1 : rt.raw().read8(kErr);
            got.assign(static_cast<std::size_t>(n) + 1, 0xEE);
            if (!res.ok) {
                std::cerr << name << ": " << res.error << "\n";
                return;
            }
            const std::uint16_t d = res.regs.hl;
            const std::uint16_t at = static_cast<std::uint16_t>(rt.raw().read8(d) | rt.raw().read8(d + 1) << 8);
            const int len = rt.raw().read8(d + 2) | rt.raw().read8(d + 3) << 8;
            got.resize(static_cast<std::size_t>(len));
            for (int k = 0; k < len; ++k) got[static_cast<std::size_t>(k)] = rt.raw().read8(static_cast<std::uint16_t>(at + k));
            rt.raw().write32(kB, 0);
            Z80Regs rel;
            rel.de = kB;
            (void)rt.raw().call("rt_str_release", rel);
        };
        auto cv = [&](const char* name, const std::uint8_t* b, int n) {
            for (int k = 0; k < n; ++k) rt.raw().write8(static_cast<std::uint16_t>(0xE100 + k), b[k]);
            return rt.call(name, 0xE100u | static_cast<std::uint64_t>(n) << 16);
        };
        auto hexs = [](const std::uint8_t* b, std::size_t n) {
            std::uint64_t v = 0;
            for (std::size_t k = 0; k < n && k < 8; ++k) v = v << 8 | b[k];
            return v;
        };
        auto one = [&](f64 x) {
            std::uint8_t want[8];
            std::vector<std::uint8_t> got;
            int err = 0;
            rt_mkd(fp_canon(x), want);
            mk("rt_mkd", x, 8, got, err);
            report("rt_mkd", x, 0, got.size() == 8 ? hexs(got.data(), 8) : 0xEEEEu, err, hexs(want, 8), -1);
            g_error = -1;
            const f64 back = rt_cvd(want);
            const int eb = g_error;
            const auto [gb, ge] = cv("rt_cvd", want, 8);
            report("rt_cvd", hexs(want, 8), 0, gb, ge, back, eb);
            g_error = -1;
            rt_mks(fp_canon(x), want);
            const int es = g_error;
            mk("rt_mks", x, 4, got, err);
            report("rt_mks", x, 0, got.size() == 4 ? hexs(got.data(), 4) : 0xEEEEu, err, hexs(want, 4), es);
            g_error = -1;
            const f64 sb = rt_cvs(want);
            const int esb = g_error;
            const auto [gs, gse] = cv("rt_cvs", want, 4);
            report("rt_cvs", hexs(want, 4), 0, gs, gse, sb, esb);
        };
        for (f64 x : {f64{0}, f64{0x3FF0000000000000u}, f64{0xC004000000000000u}, f64{0x3FB999999999999Au}, f64{0x7FEFFFFFFFFFFFFFu},
                      f64{0x0010000000000000u}, f64{0x47EFFFFFE0000000u}, f64{0x47EFFFFFF0000000u}, f64{0x3810000000000000u},
                      f64{0x380FFFFFF0000000u}, f64{0x3FF000001F000000u}, f64{0x3FF0000010000000u}, f64{0x3FF0000030000000u},
                      f64{0x3FF000000000001Fu}, f64{0x3FF0000000000010u}, f64{0x3FF0000000000030u}, f64{0x3FFFFFFFFFFFFFF0u}})
            one(x);
        Rng rng;
        for (int i = 0; i < cases / 4; ++i) one(randomF64(rng, i % 3 == 0 ? 0 : 800, i % 3 == 0 ? 2046 : 1250));
        for (int i = 0; i < cases / 8; ++i) {  // arbitrary bytes, also shorter strings (padded with zeros)
            std::uint8_t b[8];
            for (std::uint8_t& c : b) c = static_cast<std::uint8_t>(rng.next());
            const int n = i % 5 == 0 ? i % 8 : 8;
            std::uint8_t pad[8] = {};
            for (int k = 0; k < n; ++k) pad[k] = b[k];
            g_error = -1;
            const f64 w = rt_cvd(pad);
            const int ew = g_error;
            const auto [g, e] = cv("rt_cvd", b, n);
            report("rt_cvd bytes", hexs(pad, 8), static_cast<std::uint64_t>(n), g, e, w, ew);
            g_error = -1;
            const f64 sw = rt_cvs(pad);
            const int esw = g_error;
            const auto [sg, se] = cv("rt_cvs", b, n < 4 ? n : 4);
            report("rt_cvs bytes", hexs(pad, 4), static_cast<std::uint64_t>(n), sg, se, sw, esw);
            const std::uint32_t l = static_cast<std::uint32_t>(hexs(pad, 4));
            const auto [cl, cle] = cv("rt_cvl", b, n < 4 ? n : 4);
            report("rt_cvl", l, 0, cl & 0xFFFFFFFFu, cle, l, -1);
            const auto [ci, cie] = cv("rt_cvi", b, n < 2 ? n : 2);
            report("rt_cvi", l, 0, ci & 0xFFFFFFFFu, cie,
                   static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(l >> 16))), -1);
            std::vector<std::uint8_t> got;
            int err = 0;
            mk("rt_mkl", l, 4, got, err);
            report("rt_mkl", l, 0, got.size() == 4 ? hexs(got.data(), 4) : 0xEEEEu, err, l, -1);
            mk("rt_mki", l, 2, got, err);
            report("rt_mki", l, 0, got.size() == 2 ? hexs(got.data(), 2) : 0xEEEEu, err, l & 0xFFFF, -1);
        }
    }
    {  // rt_val: text -> binary64 and the number of characters read (inputs of up to 40 digits, D30)
        std::vector<std::string> texts = {"0", "1", "-1", "+2.5", "  3.25", "1E", "1E+", "1E-5", "1e300", "1E309", "1E-330",
                                          "2.2250738585072014E-308", "4.9E-324", "1.7976931348623157E308", "1.7976931348623159E308",
                                          "0.1", ".5", "5.", "-.", "", "abc", "12abc", "&H7FFFFFFF", "&HFFFFFFFF", "&h10",
                                          "$FF", "%1011", "&O777", "&X101", "&Q", "&", "-&H10", "9007199254740993",
                                          "9007199254740992.5", "123456789012345678901234567890", "0.000000000000000000001",
                                          "1.5E-7", "3.14159265358979323846", "1E-1000", "1E3000", "00012.3400", "1.0000000000000002",
                                          "4.4501477170144023E-308", "2.4703282292062327E-324", " -0", "1e5x", "1E2E3"};
        Rng rng;
        for (int k = 0; k < cases; ++k) {  // random values in the format of PRINT and with 17 digits
            const f64 x = randomF64(rng, k % 2 == 0 ? 1 : 900, k % 2 == 0 ? 2046 : 1150);
            char b[64];
            rt_fmt_flt(b, x, 13);
            texts.emplace_back(b);
            const std::uint64_t m = rng.next() % 100000000000000000ULL;
            std::snprintf(b, sizeof b, "%llu.%lluE%d", static_cast<unsigned long long>(m / 10000000000000000ULL),
                          static_cast<unsigned long long>(m % 10000000000000000ULL), rng.range(-320, 300));
            texts.emplace_back(b);
        }
        for (const std::string& t : texts) {
            std::int32_t used = 0;
            g_error = -1;
            const f64 want = rt_val(t.data(), static_cast<std::int32_t>(t.size()), &used);
            for (std::size_t i = 0; i < t.size(); ++i) rt.raw().write8(static_cast<std::uint16_t>(0xE200 + i), static_cast<std::uint8_t>(t[i]));
            rt.raw().write64(kDst, 0xA5A5A5A5A5A5A5A5u);
            Z80Regs in;
            in.hl = kDst;
            in.de = 0xE200;
            in.bc = static_cast<std::uint16_t>(t.size());
            in.ix = 0x1357;
            in.iy = 0x2468;
            const auto res = rt.raw().call("rt_val", in, 50'000'000);
            const std::uint64_t got = res.ok ? rt.raw().read64(kDst) : 0;
            const std::uint16_t usedAt = rt.raw().symbol("rt_val_used");
            const int gotUsed = rt.raw().read8(usedAt) | rt.raw().read8(static_cast<std::uint16_t>(usedAt + 1)) << 8;
            ++g_checks;
            if (!res.ok || got != want || gotUsed != used) {
                ++g_failures;
                if (g_shown++ < 20)
                    std::fprintf(stderr, "rt_val(\"%s\"): got %016llX used %d, want %016llX used %d %s\n", t.c_str(),
                                 static_cast<unsigned long long>(got), gotUsed, static_cast<unsigned long long>(want), used, res.error.c_str());
            }
        }
    }
    {  // transcendental functions (the same sequence of basic operations as runtime/ref)
        const int n = std::max(cases / 4, 20);
        checkUnary(rt, "rt_exp", rt_exp, n, {0x40862E42FEFA39F0u, 0xC087C00000000001u, 0x4086000000000000u});
        checkUnary(rt, "rt_log", rt_log, n, {0x3FF6A09E667F3BCDu, 0x3FF6A09E667F3BCEu, 0xBFF0000000000000u});
        checkUnary(rt, "rt_log10", rt_log10, n);
        checkUnary(rt, "rt_sin", rt_sin, n, {0x400921FB54442D18u, 0x4415AF1D78B58C40u /* 1e20 */});
        checkUnary(rt, "rt_cos", rt_cos, n, {0x3FF921FB54442D18u});
        checkUnary(rt, "rt_tan", rt_tan, n, {0x3FE921FB54442D18u});
        checkUnary(rt, "rt_atn", rt_atn, n, {0x3FD126145E9ECD56u, 0x3FD126145E9ECD57u});
        checkUnary(rt, "rt_asin", rt_asin, n, {0x3FEFFFFFFFFFFFFFu, 0xBFF0000000000000u, 0x3FF0000000000001u});
        checkUnary(rt, "rt_acos", rt_acos, n, {0x3FE0000000000000u, 0x3FE0000000000001u, 0xBFEFFFFFFFFFFFFFu});
        checkUnary(rt, "rt_deg", rt_deg, n);
        checkUnary(rt, "rt_rad", rt_rad, n);
        Rng rng;
        for (int k = 0; k < n; ++k) {
            const f64 x = k % 5 == 0 ? 0 : randomF64(rng, 1000, 1040) & (k % 3 == 0 ? ~0ULL : 0x7FFFFFFFFFFFFFFFULL);
            const f64 y = randomF64(rng, 1000, 1030);
            g_error = -1;
            const f64 want = rt_powf(x, y);
            const int wantErr = g_error;
            const auto [got, err] = rt.call("rt_powf", x, y);
            report("rt_powf", x, y, got, err, want, wantErr);
        }
        for (int k = 0; k < n; ++k) {  // x^y as platform/pc power: integer exponents by squaring
            const f64 x = randomF64(rng, 1015, 1032);
            f64 y;
            if (k % 3 == 0) y = fp_from_i32(rng.range(-40, 40));
            else if (k % 3 == 1) y = randomF64(rng, 1015, 1030);
            else y = fp_from_i32(rng.range(-1100, 1100));
            g_error = -1;
            f64 want = 0;
            {  // platform/pc power, with the error of the first failing operation
                const f64 t = fp_trunc(y);
                if (fp_cmp(t, y) != 0) {
                    want = rt_powf(x, y);
                } else if (fp_cmp(t & 0x7FFFFFFFFFFFFFFFULL, 0x41DFFFFFFFC00000ULL) > 0) {
                    rt_error(1);
                } else {
                    std::int64_t e = fp_to_i32(t);
                    const bool neg = e < 0;
                    if (neg) e = -e;
                    f64 r = F64_ONE, b = x;
                    while (e > 0 && g_error < 0) {
                        if (e & 1) r = fp_mul(r, b);
                        e >>= 1;
                        if (e > 0 && g_error < 0) b = fp_mul(b, b);
                    }
                    if (neg && g_error < 0) r = fp_div(F64_ONE, r);
                    want = g_error < 0 ? r : 0;
                }
            }
            const int wantErr = g_error;
            const auto [got, err] = rt.call("rt_power", x, y);
            if (wantErr < 0 || err == wantErr) report("rt_power", x, y, got, err, wantErr < 0 ? want : got, wantErr);
            else report("rt_power", x, y, got, err, want, wantErr);
        }
    }
    if (rt.calls_ > 0) std::fprintf(stderr, "average %llu T-states per call\n", static_cast<unsigned long long>(rt.cycles_ / rt.calls_));
    return report("conformance_z80");
}
