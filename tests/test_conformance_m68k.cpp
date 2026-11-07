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
        // all runtime routines; the platform modules resolve plat_putc/plat_exit, the stub replaces
        // the platform's rt_error (records the error number and returns)
        // (error handling with ON ERROR is left out: the stub replaces rt_error; tested by programs)
        std::vector<link::Module> mods;
        for (link::Module& m : link::loadModules(std::string(BASIC_SOURCE_DIR) + "/runtime/m68k", d))
            if (m.name != "rt_err" && m.name != "rt_onerr" && m.name != "rt_resume") mods.push_back(std::move(m));
        std::vector<std::string> all;
        for (const link::Module& m : mods) all.insert(all.end(), m.provides.begin(), m.provides.end());
        for (link::Module& m : link::loadModules(std::string(BASIC_SOURCE_DIR) + "/platform/m68k-test", d))
            if (m.name != "plat_error" && m.name != "plat_start") mods.push_back(std::move(m));
        std::vector<link::Module> stub = link::loadModules(std::string(BASIC_SOURCE_DIR) + "/tests/conformance/m68k", d);
        mods.insert(mods.end(), stub.begin(), stub.end());
        for (const link::Module& m : mods) link::checkLabels(m, d);
        const link::Library lib = link::makeLibrary(std::move(mods), d);
        link::Program prog;  // what a compiled program provides for the runtime (rt_data): no DATA items
        prog.data = "prg_data_count:\n        DC.L    0\nprg_data:\n";
        prog.provides = {"prg_data", "prg_data_count"};
        const link::Result r = link::link(lib, prog, all, link::Layout::Plain, "        ORG $1000\n", d);
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

    // Calls a routine with integer arguments in D0-D3; returns D0 (or -2 on a failed call).
    std::uint32_t callI(const std::string& name, std::uint32_t d0, std::uint32_t d1 = 0, std::uint32_t d2 = 0,
                        std::uint32_t d3 = 0) {
        M68kRegs in;
        in.d = {d0, d1, d2, d3, 0x44444444, 0x55555555, 0x66666666, 0x77777777};
        in.a = {0xA0A0A0A0, 0xA1A1A1A1, 0xA2A2A2A2, 0xA3A3A3A3, 0xA4A4A4A4, 0xA5A5A5A5, 0xA6A6A6A6};
        const auto res = rt_->call(name, in);
        ++g_checks;
        bool preserved = true;
        for (int k = 4; k < 8; ++k) preserved = preserved && res.regs.d[static_cast<std::size_t>(k)] == in.d[static_cast<std::size_t>(k)];
        for (int k = 2; k < 7; ++k) preserved = preserved && res.regs.a[static_cast<std::size_t>(k)] == in.a[static_cast<std::size_t>(k)];
        if (!res.ok || !preserved) {
            ++g_failures;
            std::cerr << name << ": " << (res.ok ? "registers not preserved" : res.error) << "\n";
            return 0xFFFFFFFEu;
        }
        instructions_ += res.instructions;
        return res.regs.d[0];
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

// VAL: text at $78000; value and number of characters read against the reference.
void valTest(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    std::uint64_t instr = 0, maxInstr = 0;
    int calls = 0;
    auto one = [&](const std::string& text) {
        std::int32_t used = 0;
        const f64 expect = rt_val(text.data(), static_cast<std::int32_t>(text.size()), &used);
        for (std::size_t k = 0; k < text.size(); ++k) rt.rt().write8(0x78000 + static_cast<std::uint32_t>(k), static_cast<std::uint8_t>(text[k]));
        M68kRegs in;
        in.d = {static_cast<std::uint32_t>(text.size()), 0, 0, 0, 0x44444444, 0x55555555, 0x66666666, 0x77777777};
        in.a = {0x78000, 0, 0xA2A2A2A2, 0xA3A3A3A3, 0xA4A4A4A4, 0xA5A5A5A5, 0xA6A6A6A6};
        const auto res = rt.rt().call("rt_val", in, 50'000'000);
        ++g_checks;
        const f64 got = (static_cast<f64>(res.regs.d[0]) << 32) | res.regs.d[1];
        bool ok = res.ok && got == expect && static_cast<std::int32_t>(res.regs.d[2]) == used;
        for (int k = 4; k < 8; ++k) ok = ok && res.regs.d[static_cast<std::size_t>(k)] == in.d[static_cast<std::size_t>(k)];
        for (int k = 2; k < 7; ++k) ok = ok && res.regs.a[static_cast<std::size_t>(k)] == in.a[static_cast<std::size_t>(k)];
        instr += res.instructions;
        maxInstr = std::max<std::uint64_t>(maxInstr, res.instructions);
        ++calls;
        if (!ok) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "rt_val(\"%.60s\" len %zu): %016llX used %d, expected %016llX used %d %s\n", text.c_str(), text.size(),
                             static_cast<unsigned long long>(got), static_cast<int>(res.regs.d[2]), static_cast<unsigned long long>(expect),
                             used, res.error.c_str());
        }
    };
    for (const char* t : {"", " ", "-", "+", ".", "-.", "1E", "1e+", "1E-", "1E-x", "-&H10", "&H", "&", "&Z1", "&HFFFFFFFF", "&h7fffffff",
                          "$ff", "$", "%101", "%2", "&O17", "&o8", "&X11", "&x", "  12", "0.0", "000", "-0", "1E309", "1E-330", ".5x",
                          "1.7976931348623157E308", "1.7976931348623158E308", "1.7976931348623159E308", "4.9E-324", "2.4703282292062327E-324",
                          "2.4703282292062328E-324", "1E-400", "00001.5000", "1e5000000", "1e-5000000", "3.00 DM", "x", "123456789012345678901234567890",
                          "0.000000000000000000000000000001", "9007199254740993", "9007199254740992.5", "1.5E3", "-2.5", "  -  1", "1.e5", "1.2.3", "+.5e-3"})
        one(t);
    // random decimals
    for (int i = 0; i < cases / 4; ++i) {
        std::string t;
        if (r.range(0, 3) == 0) t += ' ';
        if (r.range(0, 2) == 0) t += r.range(0, 1) ? '-' : '+';
        const int nd = r.range(1, i % 10 == 0 ? 60 : 20);
        const int point = r.range(-1, nd);
        for (int k = 0; k < nd; ++k) {
            if (k == point) t += '.';
            t += static_cast<char>('0' + r.range(0, 9));
        }
        if (r.range(0, 1)) {
            t += r.range(0, 1) ? 'E' : 'e';
            if (r.range(0, 1)) t += r.range(0, 1) ? '-' : '+';
            t += std::to_string(r.range(0, 340));
        }
        if (r.range(0, 4) == 0) t += "x1";
        one(t);
    }
    // exact halfway points between neighbouring doubles (and just below/above), fixed notation
    for (int i = 0; i < cases / 40; ++i) {
        const f64 x = randomF64(r, 1023 - 60, 1023 + 60) & ~(std::uint64_t{1} << 63);
        double xd, ud;
        std::memcpy(&xd, &x, 8);
        const f64 next = x + 1;
        double nd;
        std::memcpy(&nd, &next, 8);
        ud = (nd - xd) / 2;  // half an ulp: exact
        char a[1400], b[1400];
        std::snprintf(a, sizeof a, "%.1100f", xd);
        std::snprintf(b, sizeof b, "%.1100f", ud);
        std::string sa = a, sb = b;
        while (sb.size() < sa.size()) sb.insert(0, 1, '0');
        while (sa.size() < sb.size()) sa.insert(0, 1, '0');
        std::string sum(sa.size(), '0');
        int carry = 0;
        for (std::size_t k = sa.size(); k-- > 0;) {
            if (sa[k] == '.') {
                sum[k] = '.';
                continue;
            }
            const int d = (sa[k] - '0') + (sb[k] - '0') + carry;
            sum[k] = static_cast<char>('0' + d % 10);
            carry = d / 10;
        }
        if (carry) sum.insert(0, 1, '1');
        while (!sum.empty() && sum.back() == '0') sum.pop_back();
        one(sum);
        one(sum + "0000001");
        std::string below = sum;  // drop the last digit: just below the midpoint
        below.pop_back();
        one(below);
        if (i % 4 == 0) {  // more than 800 significant digits: the tail decides
            std::string longer = sum;
            while (longer.size() < 900) longer += '0';
            one(longer + "1");
            one(longer);
        }
    }
    // long digit strings around the 800 limit and extreme exponents
    for (int i = 0; i < 12; ++i) {
        std::string t = std::to_string(r.range(1, 9));
        const int len = 780 + r.range(0, 60);
        for (int k = 1; k < len; ++k) t += static_cast<char>('0' + r.range(0, 9));
        t += "E" + std::to_string(r.range(-1130, -700));
        one(t);
        one("0." + t);
    }
    std::cout << "rt_val: " << instr / std::max(calls, 1) << " instructions per call on average, at most " << maxInstr << "\n";
}

// PRINT USING: random formats from building blocks, numbers and strings, all four modes; the
// buffer variants (rt_using_numb ...) against runtime/ref/rt_using.c. Also STR$(x,y,z), STR$(x,n).
void usingTest(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    auto put = [&](std::uint32_t at, const std::string& t) {
        for (std::size_t k = 0; k < t.size(); ++k) rt.rt().write8(at + static_cast<std::uint32_t>(k), static_cast<std::uint8_t>(t[k]));
    };
    auto desc = [&](std::uint32_t d, std::uint32_t at, const std::string& t) {
        put(at, t);
        rt.rt().write32(d, at);
        rt.rt().write32(d + 4, static_cast<std::uint32_t>(t.size()) << 16);
    };
    auto call = [&](const char* name, M68kRegs in, std::string* text) {
        in.d[4] = 0x44444444;
        in.d[5] = 0x55555555;
        in.d[6] = 0x66666666;
        in.d[7] = 0x77777777;
        in.a[2] = 0xA2A2A2A2;
        in.a[3] = 0xA3A3A3A3;
        in.a[4] = 0xA4A4A4A4;
        in.a[5] = 0xA5A5A5A5;
        in.a[6] = 0xA6A6A6A6;
        const auto res = rt.rt().call(name, in, 20'000'000);
        bool ok = res.ok;
        for (int k = 4; k < 8; ++k) ok = ok && res.regs.d[static_cast<std::size_t>(k)] == in.d[static_cast<std::size_t>(k)];
        for (int k = 2; k < 7; ++k) ok = ok && res.regs.a[static_cast<std::size_t>(k)] == in.a[static_cast<std::size_t>(k)];
        if (text != nullptr && ok) {
            text->clear();
            for (std::uint32_t k = 0; k < res.regs.d[0] && k < 5000; ++k) *text += static_cast<char>(rt.rt().read8(res.regs.a[0] + k));
        }
        if (!ok) std::fprintf(stderr, "%s: %s\n", name, res.ok ? "registers not preserved" : res.error.c_str());
        return ok;
    };
    static const char* blocks[] = {"#", "##", "###", "####", "#,###", ",", ".", ".#", ".##", "*", "**", "$", "+", "-", "^^^^", "^^^", "^^^^^",
                                   "^^", "&", "!", "\\\\", "\\  \\", "_", "_#", "x", " ", "abc", "#.#", "+##.##", "##.##-", "**$#,##.##",
                                   "#.##^^^^", "###.##^^^^", "-#.#", "$##", "#.###,##", "##,##", "+.##", "*$#", "_!", "%"};
    const int nblocks = static_cast<int>(sizeof blocks / sizeof blocks[0]);
    static const f64 special[] = {0, 0x8000000000000000u, F64_ONE, 0xBFF0000000000000u, 0x3FC0000000000000u /*0.125*/, 0x4004000000000000u /*2.5*/,
                                  0x4058FFAE147AE148u /*99.995*/, 0x4415AF1D78B58C40u /*1e20*/, 0x3DDB7CDFD9D7BDBBu /*1e-10*/,
                                  0x7E37E43C8800759Cu /*1e300*/, 0x408F400000000000u /*1000*/, 0xC0C3880000000000u /*-10000*/,
                                  0x3FE0000000000000u, 0x3FEFFFFFFFFFFFFFu, 0x4023FFFFFFFFFFFFu /*9.999..*/};
    const int nspecial = static_cast<int>(sizeof special / sizeof special[0]);
    for (int i = 0; i < cases / 2; ++i) {
        std::string f;
        const int nb = r.range(1, 4);
        for (int k = 0; k < nb; ++k) f += blocks[r.range(0, nblocks - 1)];
        const int mode = r.range(0, 3);
        rt_using u;
        rt_using_begin(&u, f.data(), static_cast<std::int32_t>(f.size()), mode);
        M68kRegs in;
        in.d[0] = static_cast<std::uint32_t>(mode);
        if (!call("rt_set_mode", in, nullptr)) break;
        desc(0x77F00, 0x78000, f);
        in.a[0] = 0x77F00;
        if (!call("rt_using_begin", in, nullptr)) break;
        std::string expect, got, part;
        static char buf[4096];
        const int nv = r.range(1, 3);
        for (int k = 0; k < nv; ++k) {
            M68kRegs v;
            if (r.range(0, 4) == 0) {
                static const char* strs[] = {"", "a", "hello world", "xy"};
                const std::string sv = strs[r.range(0, 3)];
                expect += std::string(buf, static_cast<std::size_t>(rt_using_str(&u, buf, sizeof buf, sv.data(), static_cast<std::int32_t>(sv.size()))));
                desc(0x77F10, 0x77000, sv);
                v.a[0] = 0x77F10;
                if (!call("rt_using_strb", v, &part)) break;
            } else {
                const f64 x = r.range(0, 2) == 0 ? special[r.range(0, nspecial - 1)] : randomF64(r, 1023 - 40, 1023 + 70);
                expect += std::string(buf, static_cast<std::size_t>(rt_using_num(&u, buf, sizeof buf, x)));
                v.d[0] = static_cast<std::uint32_t>(x >> 32);
                v.d[1] = static_cast<std::uint32_t>(x);
                if (!call("rt_using_numb", v, &part)) break;
            }
            got += part;
        }
        expect += std::string(buf, static_cast<std::size_t>(rt_using_end(&u, buf, sizeof buf)));
        M68kRegs e;
        if (call("rt_using_endb", e, &part)) got += part;
        ++g_checks;
        if (got != expect) {
            ++g_failures;
            if (++failures <= 10) std::fprintf(stderr, "PRINT USING \"%s\" mode %d: \"%s\", expected \"%s\"\n", f.c_str(), mode, got.c_str(), expect.c_str());
        }
    }
    // STR$(x,y,z) and STR$(x,n)
    for (int i = 0; i < cases / 4; ++i) {
        const f64 x = r.range(0, 2) == 0 ? special[r.range(0, nspecial - 1)] : randomF64(r, 1023 - 20, 1023 + 40);
        const int mode = r.range(0, 3);
        const std::int32_t y = r.range(-2, 30), z = r.range(-2, 12), n = r.range(-2, 30);
        char buf[512];
        M68kRegs in;
        in.d[0] = static_cast<std::uint32_t>(mode);
        call("rt_set_mode", in, nullptr);
        const std::string expect(buf, static_cast<std::size_t>(rt_str_fmt(buf, sizeof buf, x, y, z, mode)));
        in.d = {static_cast<std::uint32_t>(x >> 32), static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(z), 0, 0, 0, 0};
        std::string got;
        call("rt_using_fmt", in, &got);
        const std::string expectN(buf, static_cast<std::size_t>(rt_str_n(buf, sizeof buf, x, n)));
        in.d[2] = static_cast<std::uint32_t>(n);
        std::string gotN;
        const auto res = rt.rt().call("rt_str_n", in, 2'000'000);
        if (res.ok) {
            const std::uint32_t d = res.regs.a[0], at = rt.rt().read32(d), len = rt.rt().read32(d + 4) >> 16;
            for (std::uint32_t k = 0; k < len; ++k) gotN += static_cast<char>(rt.rt().read8(at + k));
        }
        rt.rt().call("rt_str_release", M68kRegs{});  // D0 = 0: free the temporaries
        g_checks += 2;
        if (got != expect || gotN != expectN) {
            ++g_failures;
            if (++failures <= 10)
                std::fprintf(stderr, "STR$(%016llX,%d,%d) mode %d: \"%s\", expected \"%s\"; STR$(x,%d): \"%s\", expected \"%s\"\n",
                             static_cast<unsigned long long>(x), y, z, mode, got.c_str(), expect.c_str(), n, gotN.c_str(), expectN.c_str());
        }
    }
}

// Bit functions against the semantics of platform/pc (brt::shift, brt::bitOp; that library cannot
// be linked here because it defines rt_error itself).
std::uint32_t refShift(std::uint32_t x, std::uint32_t count, int width, int kind) {
    std::uint32_t n = count & 31;
    const std::uint32_t mask = width == 32 ? 0xFFFFFFFFu : (1u << width) - 1;
    std::uint32_t v = x & mask;
    if (kind >= 2) {
        n %= static_cast<std::uint32_t>(width);
        const bool left = kind == 2;
        if (n != 0) v = left ? ((v << n) | (v >> (width - static_cast<int>(n)))) & mask : ((v >> n) | (v << (width - static_cast<int>(n)))) & mask;
    } else {
        v = n >= static_cast<std::uint32_t>(width) ? 0 : (kind == 0 ? (v << n) & mask : v >> n);
    }
    if (width == 16) return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(static_cast<std::uint16_t>(v))));
    return v;
}

void bitsTest(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    auto check = [&](const char* name, std::uint32_t got, std::uint32_t expect, std::uint32_t x, std::uint32_t n, int w) {
        if (got == expect) return;
        ++g_failures;
        if (++failures <= 10) std::fprintf(stderr, "%s(%08X, %u) width %d: %08X, expected %08X\n", name, x, n, w, got, expect);
    };
    const char* kinds[] = {"SHL", "SHR", "ROL", "ROR"};
    auto one = [&](std::uint32_t x, std::uint32_t n) {
        for (int w : {8, 16, 32})
            for (int k = 0; k < 4; ++k)
                check(kinds[k], rt.callI("rt_shift", x, n, static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(k)), refShift(x, n, w, k), x, n, w);
        const std::uint32_t bit = 1u << (n & 31);
        check("BTST", rt.callI("rt_btst", x, n), (x & bit) != 0 ? 0xFFFFFFFFu : 0, x, n, 32);
        check("BCLR", rt.callI("rt_bclr", x, n), x & ~bit, x, n, 32);
        check("BSET", rt.callI("rt_bset", x, n), x | bit, x, n, 32);
        check("BCHG", rt.callI("rt_bchg", x, n), x ^ bit, x, n, 32);
        check("SWAP", rt.callI("rt_swap", x), (x << 16) | (x >> 16), x, 0, 32);
        check("WORD", rt.callI("rt_word", x), static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(x & 0xFFFF))), x, 0, 32);
        check("CARD", rt.callI("rt_card", x), x & 0xFFFF, x, 0, 32);
        check("BYTE", rt.callI("rt_byte", x), x & 0xFF, x, 0, 32);
        check("MUL", rt.callI("rt_imul", x, n), x * n, x, n, 32);
    };
    for (std::uint32_t x : {0u, 1u, 0x80u, 0x81u, 0x8000u, 0xFFFFu, 0x80000000u, 0xFFFFFFFFu, 0x12345678u})
        for (std::uint32_t n : {0u, 1u, 7u, 8u, 9u, 15u, 16u, 17u, 31u, 32u, 33u, 0xFFFFFFFFu, 0x80000000u}) one(x, n);
    for (int i = 0; i < cases / 8; ++i) {
        const auto x = static_cast<std::uint32_t>(r.next());
        one(x, i % 2 == 0 ? static_cast<std::uint32_t>(r.range(0, 40)) : static_cast<std::uint32_t>(r.next() >> 16));
    }
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
    bitsTest(rt, cases);
    valTest(rt, cases);
    usingTest(rt, cases);
    formatTest(rt, cases / 4);
    std::cout << "68000 instructions executed: " << rt.instructions() << "\n";
    return report("conformance_m68k");
}
