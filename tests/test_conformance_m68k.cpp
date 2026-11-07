// Conformance of the MC68000 runtime (runtime/m68k) against the C reference (runtime/ref):
// every routine is called directly in the emulator with edge cases and random operands (fixed
// seed) and must return the same bits and raise the same error (docs/testing.md).
// BASIC_CONFORMANCE_CASES sets the number of random cases per routine (default 4000).

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "link/linker.hpp"
#include "m68k_routines.hpp"
#include "rt_ref.h"
#include "test_util.hpp"

using namespace basic;
using basic::harness::M68kRegs;
using basic::harness::M68kRoutines;

namespace {

int g_error = -1;
constexpr std::uint32_t kErrorCell = 0x7F000;

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

// Random binary64 with a biased exponent in [lo, hi] (0 = zero), any sign.
f64 randomF64(Rng& r, int lo, int hi) {
    const std::uint64_t frac = r.next() & ((std::uint64_t{1} << 52) - 1);
    const auto be = static_cast<std::uint64_t>(r.range(lo, hi));
    return ((r.next() & 1) << 63) | (be << 52) | (be == 0 ? 0 : frac);
}

// Interesting operands: zeros, ones, extremes, near-halfway and cancellation cases.
std::vector<f64> edgeCases() {
    return {0, 0x8000000000000000u, F64_ONE, 0xBFF0000000000000u, 0x0010000000000000u, 0x7FEFFFFFFFFFFFFFu,
            0xFFEFFFFFFFFFFFFFu, 0x3FF0000000000001u, 0x3FEFFFFFFFFFFFFFu, 0x4340000000000000u, 0x3CA0000000000000u,
            0x3FF8000000000000u, 0x000FFFFFFFFFFFFFu /* denormal: counts as 0 */};
}

class Runtime {
public:
    Runtime() {
        Diagnostics d;
        std::vector<link::Module> mods = link::loadModules(std::string(BASIC_SOURCE_DIR) + "/runtime/m68k", d);
        std::vector<link::Module> stub = link::loadModules(std::string(BASIC_SOURCE_DIR) + "/tests/conformance/m68k", d);
        mods.insert(mods.end(), stub.begin(), stub.end());
        for (const link::Module& m : mods) link::checkLabels(m, d);
        const link::Library lib = link::makeLibrary(std::move(mods), d);
        std::vector<std::string> all;
        for (const auto& [sym, idx] : lib.bySymbol) all.push_back(sym);
        const link::Result r = link::link(lib, {}, all, link::Layout::Plain, "        ORG $1000\n", d);
        for (const Diagnostic& x : d) std::cerr << "link: " << x.message << "\n";
        CHECK(d.empty());
        if (const char* dump = std::getenv("BASIC_DUMP_SOURCE")) {
            std::FILE* f = std::fopen(dump, "w");
            if (f != nullptr) {
                std::fputs(r.source.c_str(), f);
                std::fclose(f);
            }
        }
        rt_ = std::make_unique<M68kRoutines>(r.source);
        if (!rt_->error().empty()) {  // show the offending source lines
            std::cerr << "assembler:\n" << rt_->error();
            std::vector<std::string> lines;
            std::string l;
            for (char c : r.source) {
                if (c == '\n') {
                    lines.push_back(l);
                    l.clear();
                } else {
                    l += c;
                }
            }
            for (std::size_t pos = 0; (pos = rt_->error().find("line ", pos)) != std::string::npos; pos += 5) {
                const std::size_t n = std::stoul(rt_->error().substr(pos + 5));
                if (n >= 1 && n <= lines.size()) std::cerr << "  " << n << ": " << lines[n - 1] << "\n";
            }
        }
        CHECK(rt_->error().empty());
    }

    M68kRoutines& rt() { return *rt_; }

    // Calls a routine with up to two f64 operands; returns D0:D1 and the error number (-1: none).
    // D4-D7 and A2-A6 must be preserved.
    std::pair<f64, int> callF(const std::string& name, f64 a, f64 b = 0) {
        M68kRegs in;
        in.d = {static_cast<std::uint32_t>(a >> 32), static_cast<std::uint32_t>(a), static_cast<std::uint32_t>(b >> 32),
                static_cast<std::uint32_t>(b), 0x44444444, 0x55555555, 0x66666666, 0x77777777};
        in.a = {0xA0A0A0A0, 0xA1A1A1A1, 0xA2A2A2A2, 0xA3A3A3A3, 0xA4A4A4A4, 0xA5A5A5A5, 0xA6A6A6A6};
        rt_->write32(kErrorCell, 0xFFFFFFFF);
        const auto res = rt_->call(name, in);
        ++g_checks;
        if (!res.ok) {
            ++g_failures;
            std::cerr << res.error << "\n";
            return {0, -2};
        }
        for (int k = 4; k < 8; ++k)
            if (res.regs.d[static_cast<std::size_t>(k)] != in.d[static_cast<std::size_t>(k)]) {
                ++g_failures;
                std::cerr << name << ": D" << k << " not preserved\n";
            }
        for (int k = 2; k < 7; ++k)
            if (res.regs.a[static_cast<std::size_t>(k)] != in.a[static_cast<std::size_t>(k)]) {
                ++g_failures;
                std::cerr << name << ": A" << k << " not preserved\n";
            }
        instructions_ += res.instructions;
        const f64 r = (static_cast<f64>(res.regs.d[0]) << 32) | res.regs.d[1];
        return {r, static_cast<int>(rt_->read32(kErrorCell))};
    }

    std::uint64_t instructions() const { return instructions_; }

private:
    std::unique_ptr<M68kRoutines> rt_;
    std::uint64_t instructions_ = 0;
};

int casesFromEnv() {
    const char* e = std::getenv("BASIC_CONFORMANCE_CASES");
    return e != nullptr ? std::atoi(e) : 4000;
}

// Compares a binary operation with the reference over edge cases and random operands.
void binary(Runtime& rt, const char* name, f64 (*ref)(f64, f64), int cases) {
    Rng r;
    int failures = 0;
    auto one = [&](f64 a, f64 b) {
        g_error = -1;
        const f64 expect = ref(fp_canon(a), fp_canon(b));
        const int expectErr = g_error;
        const auto [got, err] = rt.callF(name, a, b);
        ++g_checks;
        if (got != expect || err != expectErr) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "%s(%016llX, %016llX): %016llX err %d, expected %016llX err %d\n", name,
                             static_cast<unsigned long long>(a), static_cast<unsigned long long>(b),
                             static_cast<unsigned long long>(got), err, static_cast<unsigned long long>(expect), expectErr);
        }
    };
    for (f64 a : edgeCases())
        for (f64 b : edgeCases()) one(a, b);
    for (int i = 0; i < cases; ++i) {
        const bool wide = i % 4 == 0;
        const f64 a = randomF64(r, wide ? 1 : 1000, wide ? 2046 : 1046);
        f64 b = randomF64(r, wide ? 1 : 1000, wide ? 2046 : 1046);
        if (i % 7 == 0) b = a ^ (r.next() & 0xFFF);  // close values: cancellation
        if (i % 11 == 0) b ^= std::uint64_t{1} << 63;
        if (i % 13 == 0) {  // exponents close together: alignment by 0..70 bits
            const int ea = static_cast<int>((a >> 52) & 0x7FF);
            const int eb = std::max(1, ea - r.range(0, 70));
            b = (b & 0x800FFFFFFFFFFFFFu) | (static_cast<std::uint64_t>(eb) << 52);
        }
        one(a, b);
    }
}

void compare(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    auto one = [&](f64 a, f64 b) {
        const int expect = fp_cmp(fp_canon(a), fp_canon(b));
        const auto [got, err] = rt.callF("fp_cmp", a, b);
        const int g = static_cast<std::int32_t>(got >> 32);
        ++g_checks;
        if (g != expect || err != -1) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "fp_cmp(%016llX, %016llX): %d, expected %d\n", static_cast<unsigned long long>(a),
                             static_cast<unsigned long long>(b), g, expect);
        }
    };
    for (f64 a : edgeCases())
        for (f64 b : edgeCases()) one(a, b);
    for (int i = 0; i < cases; ++i) {
        const f64 a = randomF64(r, 0, 2046);
        one(a, i % 3 == 0 ? a : (i % 3 == 1 ? a ^ (r.next() & 3) : randomF64(r, 0, 2046)));
    }
}

// Unary f64 -> f64 routines (TRUNC, INT, ...).
void unary(Runtime& rt, const char* name, f64 (*ref)(f64), int cases) {
    Rng r;
    int failures = 0;
    auto one = [&](f64 a) {
        g_error = -1;
        const f64 expect = ref(fp_canon(a));
        const int expectErr = g_error;
        const auto [got, err] = rt.callF(name, a);
        ++g_checks;
        if (got != expect || err != expectErr) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "%s(%016llX): %016llX err %d, expected %016llX err %d\n", name, static_cast<unsigned long long>(a),
                             static_cast<unsigned long long>(got), err, static_cast<unsigned long long>(expect), expectErr);
        }
    };
    for (f64 a : edgeCases()) one(a);
    for (int k = -3; k <= 60; ++k)  // around integers and halves
        for (std::int32_t i : {0, 1, 2, 3, 7, -1, -2, -3})
            for (f64 half : {f64{0}, f64{0x3FE0000000000000u}}) {
                const f64 x = fp_add(fp_from_i32(i), half);
                one(fp_mul(x, k >= 0 ? fp_from_i32(1 << (k % 30)) : 0x3FE0000000000000u));
            }
    for (int i = 0; i < cases; ++i) one(randomF64(r, i % 3 == 0 ? 0 : 990, i % 3 == 0 ? 2046 : 1090));
}

// f64 -> int32 conversions (D0), with range errors.
void toInt(Runtime& rt, const char* name, std::int64_t (*ref)(f64), int cases) {
    Rng r;
    int failures = 0;
    auto one = [&](f64 a) {
        g_error = -1;
        const std::int64_t expect = ref(fp_canon(a));
        const int expectErr = g_error;
        const auto [got, err] = rt.callF(name, a);
        const std::int32_t g = static_cast<std::int32_t>(got >> 32);
        ++g_checks;
        if (g != expect || err != expectErr) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "%s(%016llX): %d err %d, expected %lld err %d\n", name, static_cast<unsigned long long>(a), g, err,
                             static_cast<long long>(expect), expectErr);
        }
    };
    for (f64 a : edgeCases()) one(a);
    for (std::int64_t v : {-2147483648LL, 2147483647LL, 2147483648LL, -2147483649LL, 32767LL, 32768LL, -32768LL, -32769LL, 255LL, 256LL, -1LL}) {
        const f64 x = fp_add(fp_mul(fp_from_i32(static_cast<std::int32_t>(v >> 16)), fp_from_i32(65536)), fp_from_i32(static_cast<std::int32_t>(v & 0xFFFF)));
        one(x);
        one(fp_add(x, 0x3FE0000000000000u));
        one(fp_sub(x, 0x3FE0000000000000u));
    }
    for (int i = 0; i < cases; ++i) one(randomF64(r, i % 3 == 0 ? 0 : 1000, i % 3 == 0 ? 2046 : 1060));
}

void fromInt(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    auto one = [&](std::int32_t v) {
        const f64 expect = fp_from_i32(v);
        const auto [got, err] = rt.callF("fp_from_i32", static_cast<f64>(static_cast<std::uint32_t>(v)) << 32);
        ++g_checks;
        if (got != expect || err != -1) {
            ++g_failures;
            if (++failures <= 10) std::fprintf(stderr, "fp_from_i32(%d): %016llX\n", v, static_cast<unsigned long long>(got));
        }
    };
    for (std::int32_t v : {0, 1, -1, 2, INT32_MIN, INT32_MAX, 65535, -65536}) one(v);
    for (int i = 0; i < cases; ++i) one(static_cast<std::int32_t>(r.next() >> (r.next() % 32)));
}

void approxEq(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    auto one = [&](f64 a, f64 b) {
        const int expect = fp_approx_eq(fp_canon(a), fp_canon(b));
        const auto [got, err] = rt.callF("fp_approx_eq", a, b);
        const int g = static_cast<std::int32_t>(got >> 32);
        ++g_checks;
        if (g != expect || err != -1) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "fp_approx_eq(%016llX, %016llX): %d, expected %d\n", static_cast<unsigned long long>(a),
                             static_cast<unsigned long long>(b), g, expect);
        }
    };
    const f64 g29 = 0x3E20000000000000u, h28 = 0x3E30000000000000u;  // 2^-29, 2^-28 (probe3)
    for (f64 base : {F64_ONE, f64{0x3FF8000000000000u}, f64{0xBFF0000000000000u}, f64{0x408F400000000000u}})
        for (f64 d : {g29, h28, fp_neg(g29), fp_neg(h28), fp_add(g29, h28), fp_mul(g29, 0x3FF8000000000000u)}) {
            one(base, fp_add(base, fp_mul(base, d)));
            one(fp_add(base, fp_mul(base, d)), base);
        }
    for (f64 a : edgeCases())
        for (f64 b : edgeCases()) one(a, b);
    for (int i = 0; i < cases; ++i) {
        const f64 a = randomF64(r, 1, 2046);
        one(a, i % 2 == 0 ? a ^ (r.next() & ((std::uint64_t{1} << (20 + r.range(0, 12))) - 1)) : randomF64(r, 1, 2046));
    }
}

void roundTest(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    auto one = [&](f64 x, std::int32_t n) {
        g_error = -1;
        const f64 expect = fp_round(fp_canon(x), n);
        const int expectErr = g_error;
        const auto [got, err] = rt.callF("fp_round", x, static_cast<f64>(static_cast<std::uint32_t>(n)) << 32);
        ++g_checks;
        if (got != expect || err != expectErr) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "fp_round(%016llX, %d): %016llX, expected %016llX\n", static_cast<unsigned long long>(x), n,
                             static_cast<unsigned long long>(got), static_cast<unsigned long long>(expect));
        }
    };
    for (std::int32_t n : {0, 1, 2, -1, -2, 10, 40, 400, -400})
        for (f64 x : edgeCases()) one(x, n);
    one(0x4004000000000000u, 0);   // 2.5
    one(0xC004000000000000u, 0);   // -2.5
    one(0x3FC0000000000000u, 2);   // 0.125
    one(0x4093480000000000u, -2);  // 1234.5 (approximately)
    for (int i = 0; i < cases / 10; ++i) one(randomF64(r, 1000, 1080), r.range(-12, 12));
}

void randomTest(Runtime& rt) {
    for (std::uint32_t seed : {1u, 0u, 12345u, 0xDEADBEEFu}) {
        rt_randomize(seed);
        M68kRegs in;
        in.d[0] = seed;
        CHECK(rt.rt().call("rt_randomize", in).ok);
        for (int i = 0; i < 300; ++i) {
            const int kind = i % 3;
            in.d[0] = static_cast<std::uint32_t>(i * 977 + 13);
            const auto res = rt.rt().call(kind == 0 ? "rt_rnd_next" : kind == 1 ? "rt_rnd" : "rt_rand", in);
            CHECK(res.ok);
            if (kind == 0) CHECK_EQ(res.regs.d[0], rt_rnd_next());
            if (kind == 1) CHECK_EQ((static_cast<f64>(res.regs.d[0]) << 32) | res.regs.d[1], rt_rnd());
            if (kind == 2) CHECK_EQ(static_cast<std::int32_t>(res.regs.d[0]), rt_rand(static_cast<std::int32_t>(in.d[0])));
        }
    }
}

// rt_fmt_flt writes the text to memory at $70000.
void formatTest(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    std::uint64_t instr = 0;
    int calls = 0;
    auto one = [&](f64 x, int digits) {
        char expect[RT_FMT_FLT_MAX];
        rt_fmt_flt(expect, fp_canon(x), digits);
        M68kRegs in;
        in.d = {static_cast<std::uint32_t>(x >> 32), static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(digits), 0, 0x44444444,
                0x55555555, 0x66666666, 0x77777777};
        in.a = {0x70000, 0, 0xA2A2A2A2, 0xA3A3A3A3, 0xA4A4A4A4, 0xA5A5A5A5, 0xA6A6A6A6};
        const auto res = rt.rt().call("rt_fmt_flt", in, 10'000'000);
        ++g_checks;
        std::string got;
        for (std::uint32_t k = 0; k < 40 && rt.rt().read8(0x70000 + k) != 0; ++k) got += static_cast<char>(rt.rt().read8(0x70000 + k));
        bool ok = res.ok && got == expect && res.regs.d[0] == got.size();
        for (int k = 4; k < 8; ++k) ok = ok && res.regs.d[static_cast<std::size_t>(k)] == in.d[static_cast<std::size_t>(k)];
        for (int k = 2; k < 7; ++k) ok = ok && res.regs.a[static_cast<std::size_t>(k)] == in.a[static_cast<std::size_t>(k)];
        instr += res.instructions;
        ++calls;
        if (!ok) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "rt_fmt_flt(%016llX, %d): \"%s\" (%u), expected \"%s\" %s\n", static_cast<unsigned long long>(x), digits,
                             got.c_str(), res.regs.d[0], expect, res.error.c_str());
        }
    };
    const f64 golden[] = {0x3FE0000000000000u, 0xBFE0000000000000u, 0x3FD5555555555555u, 0x4040AAAAAAAAAAABu,
                          0x3EE4F8B588E368F1u, 0x4341C37937E08000u, 0x54B249AD2594C37Du, 0x8B2C3EC03E1F0D2Au,
                          0x400921FB54442D18u, 0x41E0000000000000u, F64_ONE,  0x42D6BCC41E900000u, 0x42F2309CE5400000u,
                          0x7FEFFFFFFFFFFFFFu, 0x0010000000000000u, 0x3EB0C6F7A0B5ED8Du, 0x3E7AD7F29ABCAF48u};
    for (f64 x : golden)
        for (int digits : {13, 4, 3, 2}) one(x, digits);
    for (int i = 0; i < cases; ++i) one(randomF64(r, i % 2 == 0 ? 1 : 1000, i % 2 == 0 ? 2046 : 1090), i % 5 == 0 ? r.range(1, 14) : 13);
    for (std::int32_t v = -2000; v <= 2000; v += 7) one(fp_from_i32(v), 13);  // integers
    std::cout << "rt_fmt_flt: " << (calls ? instr / static_cast<std::uint64_t>(calls) : 0) << " instructions per call on average\n";
}

std::int64_t refToI32(f64 x) { return fp_to_i32(x); }
std::int64_t refToI16(f64 x) { return fp_to_i16(x); }
std::int64_t refToU8(f64 x) { return fp_to_u8(x); }

}  // namespace

extern "C" void rt_error(int code) {
    if (g_error < 0) g_error = code;
}

int main() {
    Runtime rt;
    if (g_failures != 0) return report("conformance_m68k");
    const int cases = casesFromEnv();
    binary(rt, "fp_add", fp_add, cases);
    binary(rt, "fp_sub", fp_sub, cases);
    compare(rt, cases);
    binary(rt, "fp_mul", fp_mul, cases);
    binary(rt, "fp_div", fp_div, cases);
    unary(rt, "fp_neg", fp_neg, cases);
    unary(rt, "fp_trunc", fp_trunc, cases);
    unary(rt, "fp_int", fp_int, cases);
    unary(rt, "fp_frac", fp_frac, cases);
    unary(rt, "fp_cint", fp_cint, cases);
    unary(rt, "fp_canon", [](f64 x) { return x; }, cases);
    toInt(rt, "fp_to_i32", refToI32, cases);
    toInt(rt, "fp_to_i16", refToI16, cases);
    toInt(rt, "fp_to_u8", refToU8, cases);
    fromInt(rt, cases);
    approxEq(rt, cases);
    unary(rt, "fp_sqrt", fp_sqrt, cases);
    roundTest(rt, cases);
    randomTest(rt);
    formatTest(rt, cases / 4);
    std::cout << "68000 instructions executed: " << rt.instructions() << "\n";
    return report("conformance_m68k");
}
