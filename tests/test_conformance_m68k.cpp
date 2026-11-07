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
            if (m.name != "rt_err" && m.name != "rt_onerr" && m.name != "rt_resume" && m.name != "rt_file" && m.name != "rt_matinput") mods.push_back(std::move(m));
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

// MKD$/CVD (GFA 8-byte format), MKS$/CVS (IEEE single): the bytes and values of runtime/ref/rt_mk.c
void mkTest(Runtime& rt, int cases) {
    Rng r;
    int failures = 0;
    auto fail = [&](const char* what, f64 x, const std::string& got, const std::string& want) {
        ++g_failures;
        if (++failures <= 10) std::fprintf(stderr, "%s(%016llX): %s, expected %s\n", what, static_cast<unsigned long long>(x), got.c_str(), want.c_str());
    };
    auto hex = [](const unsigned char* b, int n) {
        std::string h;
        char t[4];
        for (int k = 0; k < n; ++k) {
            std::snprintf(t, sizeof t, "%02X", b[k]);
            h += t;
        }
        return h;
    };
    auto mk = [&](const char* name, f64 x, int n, unsigned char* got, int& err) {
        M68kRegs in;
        in.d = {static_cast<std::uint32_t>(x >> 32), static_cast<std::uint32_t>(x), 0, 0, 0, 0, 0, 0};
        rt.rt().write32(kErrorCell, 0xFFFFFFFF);
        const auto res = rt.rt().call(name, in, 100000);
        err = static_cast<int>(rt.rt().read32(kErrorCell));
        const std::uint32_t d = res.regs.a[0], at = rt.rt().read32(d);
        for (int k = 0; k < n; ++k) got[k] = rt.rt().read8(at + static_cast<std::uint32_t>(k));
        rt.rt().call("rt_str_release", M68kRegs{});
    };
    auto cv = [&](const char* name, const unsigned char* b, int n, int& err) {
        for (int k = 0; k < n; ++k) rt.rt().write8(0x77000 + static_cast<std::uint32_t>(k), b[k]);
        rt.rt().write32(0x77F20, 0x77000);
        rt.rt().write32(0x77F24, static_cast<std::uint32_t>(n) << 16);
        M68kRegs in;
        in.a[0] = 0x77F20;
        rt.rt().write32(kErrorCell, 0xFFFFFFFF);
        const auto res = rt.rt().call(name, in, 100000);
        err = static_cast<int>(rt.rt().read32(kErrorCell));
        return (static_cast<f64>(res.regs.d[0]) << 32) | res.regs.d[1];
    };
    auto one = [&](f64 x) {
        unsigned char want[8], got[8];
        int err = 0;
        rt_mkd(fp_canon(x), want);
        mk("rt_mkd", x, 8, got, err);
        ++g_checks;
        if (std::memcmp(want, got, 8) != 0) fail("rt_mkd", x, hex(got, 8), hex(want, 8));
        g_error = -1;
        const f64 backWant = rt_cvd(want);
        const int errWant = g_error;
        const f64 back = cv("rt_cvd", want, 8, err);
        ++g_checks;
        if (back != backWant || err != errWant) fail("rt_cvd", x, std::to_string(back), std::to_string(backWant));
        g_error = -1;
        rt_mks(fp_canon(x), want);
        const int errMks = g_error;
        mk("rt_mks", x, 4, got, err);
        ++g_checks;
        if (std::memcmp(want, got, 4) != 0 || err != errMks) fail("rt_mks", x, hex(got, 4), hex(want, 4));
        g_error = -1;
        const f64 sWant = rt_cvs(want);
        const f64 sBack = cv("rt_cvs", want, 4, err);
        ++g_checks;
        if (sBack != sWant || err != g_error) fail("rt_cvs", x, std::to_string(sBack), std::to_string(sWant));
    };
    for (f64 x : {f64{0}, F64_ONE, f64{0xC004000000000000u}, f64{0x3FB999999999999Au}, f64{0x7FEFFFFFFFFFFFFFu},
                  f64{0x0010000000000000u}, f64{0x47EFFFFFE0000000u}, f64{0x47EFFFFFF0000000u}, f64{0x3810000000000000u},
                  f64{0x380FFFFFF0000000u}, f64{0x3FF000001F000000u}, f64{0x3FF0000010000000u}, f64{0x3FF0000030000000u}})
        one(x);
    for (int i = 0; i < cases / 4; ++i) one(randomF64(r, i % 3 == 0 ? 0 : 800, i % 3 == 0 ? 2046 : 1250));
    for (int i = 0; i < cases / 8; ++i) {  // arbitrary bytes for CVD and CVS
        unsigned char b[8];
        for (unsigned char& c : b) c = static_cast<unsigned char>(r.next());
        int err = 0;
        g_error = -1;
        const f64 want = rt_cvd(b);
        const int ew = g_error;
        const f64 got = cv("rt_cvd", b, 8, err);
        ++g_checks;
        if (got != want || err != ew) fail("rt_cvd bytes", 0, std::to_string(got) + " err " + std::to_string(err), std::to_string(want) + " err " + std::to_string(ew));
        g_error = -1;
        const f64 sw = rt_cvs(b);
        const int es = g_error;
        const f64 sg = cv("rt_cvs", b, 4, err);
        ++g_checks;
        if (sg != sw || err != es) fail("rt_cvs bytes", 0, std::to_string(sg), std::to_string(sw));
    }
}

// MAT (runtime/m68k/rt_mat.s) against runtime/ref/rt_mat.c: the same data as host matrix and in the
// emulator (column-major like an array: rs = 8, cs = 8 * rows), the same view, bit for bit.
struct MatCase {
    std::uint32_t addr;
    std::int32_t rows, cols;
    std::vector<f64> data;  // column-major
    rt_mat host() { return rt_mat{data.data(), rows, cols, 1, rows}; }
};

void matTest(Runtime& rt, int cases) {
    Rng r;
    const std::uint32_t views = rt.rt().symbol("rt_mat_v");
    int failures = 0;
    auto fail = [&](const std::string& what) {
        ++g_failures;
        if (++failures <= 10) std::fprintf(stderr, "rt_mat: %s\n", what.c_str());
    };
    auto value = [&](int kind) -> f64 {
        switch (kind) {
            case 0: return fp_from_i32(r.range(-3, 3));
            case 1: return randomF64(r, 1000, 1046) | (r.next() & 1 ? f64{1} << 63 : 0);
            case 2: return fp_from_i32(r.range(-2, 2) * (1 << r.range(0, 3)));
            default: return fp_div(fp_from_i32(r.range(-9, 9)), fp_from_i32(r.range(1, 7)));
        }
    };
    auto make = [&](std::uint32_t addr, std::int32_t rows, std::int32_t cols, int kind) {
        MatCase m{addr, rows, cols, std::vector<f64>(static_cast<std::size_t>(std::max(rows * cols, 1)))};
        for (f64& x : m.data) x = value(kind);
        return m;
    };
    auto put = [&](const MatCase& m) {
        for (std::size_t k = 0; k < m.data.size(); ++k) {
            rt.rt().write32(m.addr + 8 * static_cast<std::uint32_t>(k), static_cast<std::uint32_t>(m.data[k] >> 32));
            rt.rt().write32(m.addr + 8 * static_cast<std::uint32_t>(k) + 4, static_cast<std::uint32_t>(m.data[k]));
        }
    };
    auto view = [&](int k, const MatCase& m, int dims = 2) {
        const std::uint32_t v = views + 24 * static_cast<std::uint32_t>(k);
        rt.rt().write32(v, m.addr);
        rt.rt().write32(v + 4, static_cast<std::uint32_t>(m.rows));
        rt.rt().write32(v + 8, static_cast<std::uint32_t>(m.cols));
        rt.rt().write32(v + 12, 8);
        rt.rt().write32(v + 16, 8 * static_cast<std::uint32_t>(m.rows));
        rt.rt().write32(v + 20, static_cast<std::uint32_t>(dims));
    };
    // runs a routine; returns D0:D1, the error number in *err; D4-D7, A2-A6 must be kept
    auto call = [&](const char* name, std::uint32_t d0, std::uint32_t d1, std::uint32_t d2, int* err) {
        M68kRegs in;
        in.d = {d0, d1, d2, 0, 0x44444444, 0x55555555, 0x66666666, 0x77777777};
        in.a = {0xA0A0A0A0, 0xA1A1A1A1, 0xA2A2A2A2, 0xA3A3A3A3, 0xA4A4A4A4, 0xA5A5A5A5, 0xA6A6A6A6};
        rt.rt().write32(kErrorCell, 0xFFFFFFFF);
        const auto res = rt.rt().call(name, in, 50000000);
        if (!res.ok) fail(std::string(name) + ": " + res.error);
        for (int k = 4; k < 8; ++k)
            if (res.regs.d[static_cast<std::size_t>(k)] != in.d[static_cast<std::size_t>(k)]) fail(std::string(name) + ": D" + std::to_string(k) + " changed");
        for (int k = 2; k < 7; ++k)
            if (res.regs.a[static_cast<std::size_t>(k)] != in.a[static_cast<std::size_t>(k)]) fail(std::string(name) + ": A" + std::to_string(k) + " changed");
        *err = static_cast<int>(rt.rt().read32(kErrorCell));
        return (static_cast<f64>(res.regs.d[0]) << 32) | res.regs.d[1];
    };
    auto same = [&](const char* what, const MatCase& m) {  // emulator memory = host data
        ++g_checks;
        for (std::size_t k = 0; k < m.data.size(); ++k) {
            const f64 got = (static_cast<f64>(rt.rt().read32(m.addr + 8 * static_cast<std::uint32_t>(k))) << 32) |
                            rt.rt().read32(m.addr + 8 * static_cast<std::uint32_t>(k) + 4);
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
    const std::uint32_t A = 0x70000, B = 0x71000, C = 0x72000, T = 0x73000;
    for (int round = 0; round < cases / 40; ++round) {
        const int kind = r.range(0, 3);
        const std::int32_t n = r.range(0, 6), m = r.range(1, 5), q = r.range(1, 5);
        int err = 0;
        // DET, QDET, RANG; for some matrices the last row is the sum of the others (singular in exact
        // arithmetic, a tiny pivot in binary64)
        MatCase a = make(A, n, n, kind);
        if (n > 1 && r.range(0, 2) == 0)
            for (std::int32_t j = 0; j < n; ++j) {
                f64 sum = 0;
                for (std::int32_t i = 0; i + 1 < n; ++i) sum = fp_add(sum, a.data[static_cast<std::size_t>(i + j * n)]);
                a.data[static_cast<std::size_t>(n - 1 + j * n)] = sum;
            }
        if (round % 8 == 0)  // the zero matrix: tol = 0 and every pivot 0 (|p| <= tol, not <)
            for (f64& x : a.data) x = 0;
        for (int quick = 0; quick < 3; ++quick) {
            put(a);
            view(0, a);
            const rt_mat ha = a.host();
            g_error = -1;
            const f64 want = quick == 2 ? rt_mat_rank(&ha) : rt_mat_det(&ha, quick);
            const int werr = g_error;
            const f64 got = call(quick == 2 ? "rt_mat_rank" : "rt_mat_det", static_cast<std::uint32_t>(quick & 1), 0, 0, &err);
            scalar(quick == 2 ? "rank" : quick ? "qdet" : "det", got, err, want, werr);
            same("det input unchanged", a);
        }
        // INV into a target that is sometimes of the wrong size
        {
            MatCase t = make(T, r.range(0, 4) == 0 ? n + 1 : n, n, 1);
            put(a);
            put(t);
            view(0, t);
            view(1, a);
            const rt_mat ht = t.host(), ha = a.host();
            g_error = -1;
            rt_mat_inv(&ht, &ha);
            const int werr = g_error;
            call("rt_mat_inv", 0, 0, 0, &err);
            scalar("inv error", 0, err, 0, werr);
            same("inv", t);
        }
        // MUL a(n x m) = b(n x q) * c(q x m), sometimes of the wrong order; DOT, TRIPLE
        {
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
            const int werr = g_error;
            call("rt_mat_mul", 0, 0, 0, &err);
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
            int we = g_error;
            scalar("dot", call("rt_mat_dot", 0, 0, 0, &err), err, want, we);
            view(1, w);
            view(2, z, 1);
            g_error = -1;
            want = rt_mat_triple(&hu, &hw, &hz);
            we = g_error;
            scalar("triple", call("rt_mat_triple", 0, 0, 0, &err), err, want, we);
        }
        // NORM by rows and columns (sometimes with a zero line), TRANS, ADD/SUB, scalar operations
        for (int byCol = 0; byCol < 2; ++byCol) {
            MatCase t = make(A, n, m, kind);
            if (n > 0 && r.range(0, 3) == 0)
                for (std::int32_t j = 0; j < m; ++j) t.data[static_cast<std::size_t>(n - 1 + j * n)] = 0;
            put(t);
            view(0, t);
            const rt_mat ht = t.host();
            g_error = -1;
            rt_mat_norm(&ht, byCol);
            const int werr = g_error;
            call("rt_mat_norm", static_cast<std::uint32_t>(byCol), 0, 0, &err);
            scalar("norm error", 0, err, 0, werr);
            same("norm", t);
        }
        {
            MatCase s = make(A, n, m, kind), t = make(T, r.range(0, 4) == 0 ? m + 1 : m, n, 1);
            put(s);
            put(t);
            view(0, t);
            view(1, s);
            const rt_mat ht = t.host(), hs = s.host();
            g_error = -1;
            rt_mat_trans(&ht, &hs);
            const int werr = g_error;
            call("rt_mat_trans", 0, 0, 0, &err);
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
            const int we = g_error;
            call("rt_mat_addsub", static_cast<std::uint32_t>(sub), 0, 0, &err);
            scalar("addsub error", 0, err, 0, we);
            same("addsub", z);
            const int op = r.range(0, 2);
            const f64 v = value(kind);
            view(0, x);
            rt_mat_scalar(&hx, v, op);
            call("rt_mat_scalar", static_cast<std::uint32_t>(v >> 32), static_cast<std::uint32_t>(v), static_cast<std::uint32_t>(op), &err);
            same("scalar", x);
        }
        // CPY/XCPY: source and destination at random offsets (the emulator views moved like rt_mat_at)
        {
            MatCase s = make(A, r.range(0, 5), r.range(0, 5), kind), d = make(T, r.range(0, 5), r.range(0, 5), 1);
            const std::int32_t si = r.range(0, 2), sj = r.range(0, 2), di = r.range(0, 2), dj = r.range(0, 2);
            const std::int32_t h = r.range(-1, 4), w = r.range(-1, 4);
            const int tr = r.range(0, 1);
            put(s);
            put(d);
            auto moved = [&](int k, const MatCase& c, std::int32_t i, std::int32_t j) {
                MatCase v = c;
                view(k, v);
                const std::uint32_t at = views + 24 * static_cast<std::uint32_t>(k);
                rt.rt().write32(at, c.addr + 8 * static_cast<std::uint32_t>(i + j * c.rows));
                rt.rt().write32(at + 4, static_cast<std::uint32_t>(c.rows - i));
                rt.rt().write32(at + 8, static_cast<std::uint32_t>(c.cols - j));
            };
            moved(0, d, di, dj);
            moved(1, s, si, sj);
            const rt_mat hd = d.host(), hs = s.host();
            rt_mat_cpy(&hd, di, dj, &hs, si, sj, h, w, tr);
            call("rt_mat_cpy", static_cast<std::uint32_t>(h), static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(tr), &err);
            same(tr ? "xcpy" : "cpy", d);
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
        // overflow-checked add/sub/mul (integer fast paths): result or error 2 (D0 = 0)
        for (int op = 0; op < 3; ++op) {
            const std::int64_t xa = static_cast<std::int32_t>(x), xb = static_cast<std::int32_t>(n);
            const std::int64_t exact = op == 0 ? xa + xb : op == 1 ? xa - xb : xa * xb;
            const bool fits = exact >= INT32_MIN && exact <= INT32_MAX;
            rt.rt().write32(kErrorCell, 0xFFFFFFFF);
            const char* name = op == 0 ? "rt_iadd_chk" : op == 1 ? "rt_isub_chk" : "rt_imul_chk";
            const std::uint32_t got = rt.callI(name, x, n, 2);
            const std::uint32_t err = rt.rt().read32(kErrorCell);
            check(name, got, fits ? static_cast<std::uint32_t>(exact) : 0, x, n, 32);
            check(name, err, fits ? 0xFFFFFFFFu : 2u, x, n, 32);
        }
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
    // transcendental functions (runtime/m68k/rt_math.s, generated): the same bits as rt_math.c
    unary(rt, "rt_exp", rt_exp, cases);
    unary(rt, "rt_log", rt_log, cases);
    unary(rt, "rt_log10", rt_log10, cases);
    unary(rt, "rt_sin", rt_sin, cases);
    unary(rt, "rt_cos", rt_cos, cases);
    unary(rt, "rt_tan", rt_tan, cases);
    unary(rt, "rt_atn", rt_atn, cases);
    unary(rt, "rt_asin", rt_asin, cases);
    unary(rt, "rt_acos", rt_acos, cases);
    unary(rt, "rt_deg", rt_deg, cases);
    unary(rt, "rt_rad", rt_rad, cases);
    binary(rt, "rt_powf", rt_powf, cases / 4);
    valTest(rt, cases);
    usingTest(rt, cases);
    mkTest(rt, cases);
    matTest(rt, cases);
    formatTest(rt, cases / 4);
    std::cout << "68000 instructions executed: " << rt.instructions() << "\n";
    return report("conformance_m68k");
}
