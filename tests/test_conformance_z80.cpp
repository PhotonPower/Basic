// Conformance of the Z80 runtime (runtime/z80) against the C reference (runtime/ref): every routine
// is called directly in the emulator with edge cases and random operands (fixed seed) and must give
// the same bits and raise the same error (docs/testing.md). IX and IY must be preserved.
// BASIC_CONFORMANCE_CASES sets the number of random cases per routine (default 2000).

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
            if (m.name != "rt_err") mods.push_back(std::move(m));
        std::vector<std::string> all;
        for (const link::Module& m : mods) all.insert(all.end(), m.provides.begin(), m.provides.end());
        for (link::Module& m : link::loadModules(std::string(BASIC_SOURCE_DIR) + "/platform/z80-test", d))
            if (m.name != "plat_error" && m.name != "plat_start") mods.push_back(std::move(m));
        std::vector<link::Module> stub = link::loadModules(std::string(BASIC_SOURCE_DIR) + "/tests/conformance/z80", d);
        mods.insert(mods.end(), stub.begin(), stub.end());
        for (const link::Module& m : mods) link::checkLabels(m, d);
        const link::Library lib = link::makeLibrary(std::move(mods), d);
        const link::Result r = link::link(lib, link::Program{}, all, link::Layout::PlainZ80, "        ORG &4000\n", d);
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
    if (rt.calls_ > 0) std::fprintf(stderr, "average %llu T-states per call\n", static_cast<unsigned long long>(rt.cycles_ / rt.calls_));
    return report("conformance_z80");
}
