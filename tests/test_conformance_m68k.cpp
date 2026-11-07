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
    std::cout << "68000 instructions executed: " << rt.instructions() << "\n";
    return report("conformance_m68k");
}
