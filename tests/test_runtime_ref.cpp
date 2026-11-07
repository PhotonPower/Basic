// Tests of the C reference runtime (runtime/ref).
//
// The reference uses integer arithmetic only. Here, in the test, the host's IEEE double arithmetic
// (round to nearest even) serves as an independent oracle for the basic operations, strtod and
// printf for the conversions. Expected values that come from the original GFA-BASIC 3.5 are marked
// with the probe test (tests/probes/results).

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>

#include "rt_ref.h"
#include "test_util.hpp"

namespace {

int g_error = -1;  // last error number reported through rt_error, -1 = none

f64 bits(double d) {
    f64 b;
    std::memcpy(&b, &d, sizeof b);
    return b;
}

double dbl(f64 b) {
    double d;
    std::memcpy(&d, &b, sizeof d);
    return d;
}

struct Rng {  // xorshift64, fixed seed: the tests are reproducible
    std::uint64_t s = 0x9E3779B97F4A7C15u;
    std::uint64_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<std::uint64_t>(hi - lo + 1)); }
};

// Random normal double with a binary exponent in [lo, hi].
double randomDouble(Rng& r, int lo, int hi) {
    const std::uint64_t frac = r.next() & ((std::uint64_t{1} << 52) - 1);
    const auto be = static_cast<std::uint64_t>(r.range(lo, hi) + 1023);
    const std::uint64_t sign = r.next() & 1;
    return dbl((sign << 63) | (be << 52) | frac);
}

// What the reference must return for a host result: the same bits, 0 for results below
// 2^-1022 (no denormals), and error 1 for infinity.
void checkAgainstHost(const char* op, double host, f64 got, int err, double a, double b) {
    ++g_checks;
    bool ok;
    if (std::isinf(host)) ok = err == RT_ERR_OVERFLOW && got == 0;
    else if (std::fabs(host) < DBL_MIN) ok = err == -1 && got == 0;
    else ok = err == -1 && got == bits(host);
    if (!ok) {
        ++g_failures;
        if (g_failures < 20)
            std::fprintf(stderr, "%s(%.17g, %.17g): host %.17g, ref %.17g (error %d)\n", op, a, b, host, dbl(got), err);
    }
}

std::string fmt(double x, int digits = 13) {
    char buf[RT_FMT_FLT_MAX];
    const int n = rt_fmt_flt(buf, bits(x), digits);
    CHECK_EQ(n, static_cast<int>(std::strlen(buf)));
    return buf;
}

// Significant digits (without trailing zeros) and decimal exponent of a formatted number.
std::string normalised(const std::string& s) {
    std::string t = s[0] == '-' ? s.substr(1) : s;
    int exp10 = 0;
    const auto e = t.find('E');
    if (e != std::string::npos) {
        exp10 = std::atoi(t.c_str() + e + 1);
        t = t.substr(0, e);
    }
    const auto dot = t.find('.');
    const int intDigits = static_cast<int>(dot == std::string::npos ? t.size() : dot);
    std::string digits;
    for (char c : t)
        if (c != '.') digits += c;
    std::size_t lead = 0;
    while (lead + 1 < digits.size() && digits[lead] == '0') ++lead;
    digits = digits.substr(lead);
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    return digits + "e" + std::to_string(exp10 + intDigits - 1 - static_cast<int>(lead));
}

f64 val(const char* s, int* used = nullptr) {
    std::int32_t u = 0;
    const f64 r = rt_val(s, static_cast<std::int32_t>(std::strlen(s)), &u);
    if (used != nullptr) *used = u;
    return r;
}

void testArithmetic() {
    Rng r;
    for (int i = 0; i < 300000; ++i) {
        // mostly moderate exponents, sometimes extreme ones (overflow, underflow)
        const bool wide = i % 4 == 0;
        const double a = randomDouble(r, wide ? -1022 : -60, wide ? 1023 : 60);
        double b = randomDouble(r, wide ? -1022 : -60, wide ? 1023 : 60);
        if (i % 7 == 0) b = dbl(bits(a) ^ (r.next() & 0xFFF));  // close values: cancellation
        if (i % 11 == 0) b = -b;
        g_error = -1;
        f64 got = fp_add(bits(a), bits(b));
        checkAgainstHost("add", a + b, got, g_error, a, b);
        g_error = -1;
        got = fp_sub(bits(a), bits(b));
        checkAgainstHost("sub", a - b, got, g_error, a, b);
        g_error = -1;
        got = fp_mul(bits(a), bits(b));
        checkAgainstHost("mul", a * b, got, g_error, a, b);
        g_error = -1;
        got = fp_div(bits(a), bits(b));
        checkAgainstHost("div", a / b, got, g_error, a, b);
        g_error = -1;
        got = fp_sqrt(bits(std::fabs(a)));
        checkAgainstHost("sqrt", std::sqrt(std::fabs(a)), got, g_error, std::fabs(a), 0);
        const int c = fp_cmp(bits(a), bits(b));
        CHECK_EQ(c, a < b ? -1 : (a > b ? 1 : 0));
    }
    // exact cases
    CHECK_EQ(fp_add(bits(1.5), bits(-1.5)), f64{0});  // no -0
    CHECK_EQ(fp_sub(bits(0.0), bits(0.0)), f64{0});
    CHECK_EQ(fp_neg(0), f64{0});
    CHECK_EQ(fp_mul(bits(-2.0), 0), f64{0});
    CHECK_EQ(dbl(fp_add(bits(0.1), bits(0.2))), 0.1 + 0.2);
    g_error = -1;
    CHECK_EQ(fp_div(F64_ONE, 0), f64{0});
    CHECK_EQ(g_error, int{RT_ERR_DIV_ZERO});  // probe1 T15
    g_error = -1;
    CHECK_EQ(fp_sqrt(bits(-1.0)), f64{0});
    CHECK_EQ(g_error, int{RT_ERR_SQR_NEG});  // probe1 T51
    g_error = -1;
    CHECK_EQ(fp_mul(bits(1e300), bits(1e300)), f64{0});
    CHECK_EQ(g_error, int{RT_ERR_OVERFLOW});  // error number as in probe3 T16
    g_error = -1;
    CHECK_EQ(fp_mul(bits(1e-300), bits(1e-300)), f64{0});  // underflow: 0 without error (probe1 T49)
    CHECK_EQ(g_error, -1);
    CHECK_EQ(fp_div(bits(DBL_MIN), bits(2.0)), f64{0});  // would be denormal
    // FOR x=0 TO 1 STEP 0.1 runs 11 times with binary64 (10 times on the original, D11)
    f64 x = 0;
    int n = 0;
    while (fp_cmp(x, F64_ONE) <= 0) {
        ++n;
        x = fp_add(x, bits(0.1));
    }
    CHECK_EQ(n, 11);
}

void testCanon() {
    g_error = -1;
    CHECK_EQ(fp_canon(bits(-0.0)), f64{0});
    CHECK_EQ(fp_canon(bits(DBL_MIN / 4)), f64{0});
    CHECK_EQ(fp_canon(bits(2.5)), bits(2.5));
    CHECK_EQ(g_error, -1);
    CHECK_EQ(fp_canon(bits(INFINITY)), f64{0});
    CHECK_EQ(g_error, int{RT_ERR_OVERFLOW});
}

void testIntegers() {
    Rng r;
    for (int i = 0; i < 100000; ++i) {
        const auto v = static_cast<std::int32_t>(r.next());
        CHECK_EQ(dbl(fp_from_i32(v)), static_cast<double>(v));
        CHECK_EQ(fp_to_i32(fp_from_i32(v)), v);
        const double d = randomDouble(r, -3, 40);
        g_error = -1;
        const std::int32_t t = fp_to_i32(bits(d));
        if (std::fabs(std::trunc(d)) <= 2147483647.0 || std::trunc(d) == -2147483648.0) {
            CHECK_EQ(t, static_cast<std::int32_t>(std::trunc(d)));
            CHECK_EQ(g_error, -1);
        } else {
            CHECK_EQ(g_error, int{RT_ERR_NO_INT});
        }
        CHECK_EQ(dbl(fp_trunc(bits(d))), std::trunc(d) == 0 ? 0.0 : std::trunc(d));
        CHECK_EQ(dbl(fp_int(bits(d))), std::floor(d) == 0 ? 0.0 : std::floor(d));
        CHECK_EQ(dbl(fp_cint(bits(d))), std::round(d) == 0 ? 0.0 : std::round(d));
        CHECK_EQ(dbl(fp_frac(bits(d))), d - std::trunc(d) == 0 ? 0.0 : d - std::trunc(d));
    }
    // assignment truncates towards 0 (probe1 T7-T12)
    CHECK_EQ(fp_to_i32(bits(2.5)), 2);
    CHECK_EQ(fp_to_i32(bits(3.5)), 3);
    CHECK_EQ(fp_to_i32(bits(-2.5)), -2);
    CHECK_EQ(fp_to_i32(bits(2.7)), 2);
    CHECK_EQ(fp_to_i32(bits(-2.7)), -2);
    CHECK_EQ(fp_to_i32(bits(-2147483648.0)), INT32_MIN);
    g_error = -1;
    fp_to_i32(bits(2147483648.0));  // probe1 T1
    CHECK_EQ(g_error, int{RT_ERR_NO_INT});
    g_error = -1;
    fp_to_i32(bits(3e9));  // probe1 T6
    CHECK_EQ(g_error, int{RT_ERR_NO_INT});
    g_error = -1;
    CHECK_EQ(fp_to_i16(bits(-32768.9)), std::int16_t{-32768});
    CHECK_EQ(g_error, -1);
    fp_to_i16(bits(32768.0));  // probe1 T4
    CHECK_EQ(g_error, int{RT_ERR_NO_WORD});
    g_error = -1;
    CHECK_EQ(fp_to_u8(bits(255.9)), std::uint8_t{255});
    CHECK_EQ(fp_to_u8(bits(-0.5)), std::uint8_t{0});
    CHECK_EQ(g_error, -1);
    fp_to_u8(bits(256.0));  // probe2 T6
    CHECK_EQ(g_error, int{RT_ERR_NO_BYTE});
    g_error = -1;
    fp_to_u8(bits(-1.0));
    CHECK_EQ(g_error, int{RT_ERR_NO_BYTE});
    // CINT: ties away from 0 (probe1 T16), INT floor (probe1 T17)
    CHECK_EQ(dbl(fp_cint(bits(0.5))), 1.0);
    CHECK_EQ(dbl(fp_cint(bits(1.5))), 2.0);
    CHECK_EQ(dbl(fp_cint(bits(2.5))), 3.0);
    CHECK_EQ(dbl(fp_cint(bits(-0.5))), -1.0);
    CHECK_EQ(dbl(fp_cint(bits(-1.5))), -2.0);
    CHECK_EQ(fp_cint(bits(0.49999999999999994)), f64{0});
    CHECK_EQ(dbl(fp_int(bits(-0.5))), -1.0);
    CHECK_EQ(fp_trunc(bits(-0.5)), f64{0});
    CHECK_EQ(fp_frac(bits(-3.0)), f64{0});
    // ROUND(x,n) = INT(x*10^n+0.5)/10^n: ties upwards (probe1 T17, probe6 T11)
    CHECK_EQ(dbl(fp_round(bits(2.5), 0)), 3.0);
    CHECK_EQ(dbl(fp_round(bits(-2.5), 0)), -2.0);
    CHECK_EQ(dbl(fp_round(bits(0.125), 2)), 0.13);
    CHECK_EQ(dbl(fp_round(bits(-0.125), 2)), -0.12);
    CHECK_EQ(dbl(fp_round(bits(1234.5), -2)), 1200.0);
    CHECK_EQ(dbl(fp_round(bits(1250), -2)), 1300.0);
    CHECK_EQ(dbl(fp_round(bits(0.35), 1)), 0.4);  // 0.3 on the original: 0.35*10 < 3.5 there (D11)
    CHECK_EQ(dbl(fp_round(bits(-0.4), 0)), 0.0);
    CHECK_EQ(fp_round(bits(-0.4), 0), f64{0});
    CHECK_EQ(dbl(fp_round(bits(1.5e300), 10)), 1.5e300);  // x*10^n overflows: x is already integral
    CHECK_EQ(dbl(fp_round(bits(1.25), 40)), 1.25);
}

void testApproxEq() {
    // probe2 T34-T39, probe3 T5-T15: rounded to 28 significant bits, ties away from 0
    const double g = std::ldexp(1.0, -29), h = std::ldexp(1.0, -28);
    struct Case {
        double a, b;
        int eq;
    };
    const Case cases[] = {
        {1, 1 + g, 1},           {1, 1 + h, 0},          {1, 1 - g, 1},         {1, 1 - h, 0},
        {1, 1 - g / 2, 1},       {1 + g, 1 + h, 0},      {1 + h, 1 + h + g, 1}, {1 + h, 1 + 2 * h, 1},
        {0.5, 0.5 + g / 2, 1},   {0.5, 0.5 + h / 2, 0},  {-1, -1 - g, 1},       {-1, -1 - h, 0},
        {0, 0, 1},               {0, g * g * g * g, 0},  {1.5, 1.5 + g, 1},     {1.5, 1.5 + h, 0},
        {1000, 1000 + 1000 * g, 1}, {1000, 1000 + 1000 * h, 0}, {1, 1 + 3 * g / 2, 1},
        {1, 1 + (h - std::ldexp(1.0, -40)), 1},           {1e-20, 2e-20, 0},     {1, -1, 0},
    };
    for (const Case& c : cases) {
        CHECK_EQ(fp_approx_eq(bits(c.a), bits(c.b)), c.eq);
        CHECK_EQ(fp_approx_eq(bits(c.b), bits(c.a)), c.eq);
    }
}

void testFormat() {
    // probe1 T22-T40, probe2 T22-T33 (PI deliberately not as on the original, D11)
    const struct {
        double x;
        const char* s;
    } golden[] = {
        {0.5, "0.5"},
        {-0.5, "-0.5"},
        {1.0 / 3, "0.3333333333333"},
        {2.0 / 3, "0.6666666666667"},
        {100.0 / 3, "33.33333333333"},
        {0.1 + 0.2, "0.3"},
        {1e-5, "0.00001"},
        {123456789012.0, "123456789012"},
        {1234567890123.0, "1234567890123"},
        {1e15, "1.0E+15"},
        {1e100, "1.0E+100"},
        {-1e-100, "-1.0E-100"},
        {3.141592653589793, "3.14159265359"},
        {2147483648.0, "2147483648"},
        {1, "1"},
        {0, "0"},
        {-12, "-12"},
        {1.0 / 7, "0.1428571428571"},
        {1 + 1e-12, "1.000000000001"},
        {1 + 1e-13, "1"},
        {1e12, "1000000000000"},
        {1e13, "10000000000000"},
        {1e14, "1.0E+14"},
        {99999999999999.0, "1.0E+14"},
        {1.5e13, "15000000000000"},
        {1.5e14, "1.5E+14"},
        {0.001, "0.001"},
        {1e-4, "0.0001"},
        {1e-6, "1.0E-06"},
        {1e-7, "1.0E-07"},
        {1.5e-6, "1.5E-06"},
        {1.25e-8, "1.25E-08"},
        {1e-10, "1.0E-10"},
        {1.5e15, "1.5E+15"},
        {123.456, "123.456"},
        {-0.000123, "-0.000123"},
        {1500, "1500"},
        {DBL_MAX, "1.797693134862E+308"},
        {DBL_MIN, "2.225073858507E-308"},
    };
    for (const auto& c : golden) CHECK_EQ(fmt(c.x), std::string(c.s));
    // DEFNUM 4 (probe2 T33)
    CHECK_EQ(fmt(3.141592653589793, 4), std::string("3.142"));
    CHECK_EQ(fmt(123456, 4), std::string("123500"));
    CHECK_EQ(fmt(0.000123456, 4), std::string("0.0001235"));
    // rounding of an exact tie at 13 digits: away from 0
    CHECK_EQ(fmt(12345678901235.0), std::string("12345678901240"));
    CHECK_EQ(fmt(-12345678901235.0), std::string("-12345678901240"));
    CHECK_EQ(fmt(1234567890123.5), std::string("1234567890124"));  // probe6 T6, T7
    // DEFNUM: at least 2 digits (probe6 T8), thresholds unchanged (probe6 T9, T10)
    CHECK_EQ(fmt(2.5, 1), std::string("2.5"));
    CHECK_EQ(fmt(0.25, 1), std::string("0.25"));
    CHECK_EQ(fmt(9.96, 1), std::string("10"));
    CHECK_EQ(fmt(12345, 3), std::string("12300"));
    CHECK_EQ(fmt(1e13, 3), std::string("10000000000000"));
    CHECK_EQ(fmt(1e14, 3), std::string("1.0E+14"));
    CHECK_EQ(fmt(0.00001234, 3), std::string("0.0000123"));
    CHECK_EQ(fmt(0.000001234, 3), std::string("1.23E-06"));
    CHECK_EQ(fmt(99999, 3), std::string("100000"));
    CHECK_EQ(fmt(999.5, 3), std::string("1000"));
    CHECK_EQ(fmt(1234567, 3), std::string("1230000"));

    // random values: same significant digits as printf (skipping exact decimal ties)
    Rng r;
    char buf[64];
    int compared = 0;
    for (int i = 0; i < 20000; ++i) {
        const double d = randomDouble(r, -1022, 1023);
        std::snprintf(buf, sizeof buf, "%.40e", std::fabs(d));
        const std::string full = buf;  // d.dddd...e+xx
        const std::string after = full.substr(14, full.find('e') - 14);
        if (after.find_first_not_of('0', 1) == std::string::npos && after[0] == '5') continue;
        std::snprintf(buf, sizeof buf, "%.12E", std::fabs(d));
        CHECK_EQ(normalised(fmt(d)), normalised(buf));
        ++compared;
    }
    CHECK(compared > 19000);
}

void testVal() {
    int used = 0;
    // probe1 T54, probe4 T47, probe3 T20
    CHECK_EQ(dbl(val("1.5E3")), 1500.0);
    CHECK_EQ(dbl(val("&HFF")), 255.0);
    CHECK_EQ(dbl(val("%101")), 5.0);
    CHECK_EQ(dbl(val(".5x", &used)), 0.5);
    CHECK_EQ(used, 2);
    CHECK_EQ(dbl(val("3.00 DM", &used)), 3.0);
    CHECK_EQ(used, 4);
    CHECK_EQ(val("x", &used), f64{0});
    CHECK_EQ(used, 0);
    CHECK_EQ(dbl(val(" 12", &used)), 12.0);
    CHECK_EQ(used, 3);
    CHECK_EQ(dbl(val("1E", &used)), 1.0);
    CHECK_EQ(used, 2);
    g_error = -1;
    CHECK_EQ(val("1E10000"), f64{0});
    CHECK_EQ(val("1E-10000"), f64{0});
    CHECK_EQ(val("1.8E308"), f64{0});
    CHECK_EQ(g_error, -1);  // no error, as on the original
    CHECK_EQ(dbl(val("-2.5")), -2.5);
    CHECK_EQ(val("-0"), f64{0});
    CHECK_EQ(dbl(val("$ff")), 255.0);
    CHECK_EQ(dbl(val("&O17")), 15.0);
    CHECK_EQ(dbl(val("&X101")), 5.0);
    // probe6 T1-T5
    CHECK_EQ(dbl(val("&HFFFFFFFF")), -1.0);
    CHECK_EQ(dbl(val("&H80000000")), -2147483648.0);
    CHECK_EQ(val("&H100000000"), f64{0});
    CHECK_EQ(val("-&H10", &used), f64{0});  // no sign before a prefix
    CHECK_EQ(used, 0);
    CHECK_EQ(dbl(val("&HFF.5", &used)), 255.0);
    CHECK_EQ(used, 4);
    CHECK_EQ(val("&H", &used), f64{0});
    CHECK_EQ(used, 0);
    CHECK_EQ(val("%2", &used), f64{0});
    CHECK_EQ(used, 0);
    CHECK_EQ(dbl(val("$10")), 16.0);
    CHECK_EQ(dbl(val("+5", &used)), 5.0);
    CHECK_EQ(used, 2);
    CHECK_EQ(dbl(val("1E+x", &used)), 1.0);
    CHECK_EQ(used, 2);
    CHECK_EQ(dbl(val("1e5")), 100000.0);
    CHECK_EQ(dbl(val("1E-5")), 1e-5);
    CHECK_EQ(dbl(val("1E+5")), 1e5);
    CHECK_EQ(dbl(val("1E-x", &used)), 1.0);
    CHECK_EQ(used, 2);

    // hard cases for correct rounding
    const char* hard[] = {
        "0.1", "0.3", "9007199254740993", "9007199254740995", "2.2250738585072014e-308",
        "1.7976931348623157e308", "4.9406564584124654e-300", "123456789012345678901234567890",
        "0.000000000000000000000000000000000000000000001",
        "2.4703282292062327208828439643411068618252990130716238221279284125033775364e-300",
        "1.00000000000000011102230246251565404236316680908203125",  // halfway: ties to even -> 1
        "1.00000000000000011102230246251565404236316680908203126",  // just above halfway
        "1.00000000000000033306690738754696212708950042724609375",  // halfway: ties to even up
    };
    for (const char* s : hard) CHECK_EQ(val(s), bits(std::strtod(s, nullptr)));
    // the halfway case with more than 800 digits: the sticky tail decides
    std::string longer = "1.00000000000000011102230246251565404236316680908203125";
    longer += std::string(900, '0') + "1";
    CHECK_EQ(val(longer.c_str()), bits(std::strtod(longer.c_str(), nullptr)));

    // random decimal strings against strtod
    Rng r;
    char buf[64];
    for (int i = 0; i < 20000; ++i) {
        const int nd = r.range(1, 20);
        std::string s = (r.next() & 1) ? "-" : "";
        for (int k = 0; k < nd; ++k) {
            if (k == 1) s += '.';
            s += static_cast<char>('0' + r.range(k == 0 ? 1 : 0, 9));
        }
        std::snprintf(buf, sizeof buf, "E%d", r.range(-320, 310));
        s += buf;
        const double expect = std::strtod(s.c_str(), nullptr);
        ++g_checks;
        const f64 got = val(s.c_str());
        const bool ok = (std::isinf(expect) || std::fabs(expect) < DBL_MIN) ? got == 0 : got == bits(expect);
        if (!ok) {
            ++g_failures;
            std::fprintf(stderr, "VAL(%s): %.17g, expected %.17g\n", s.c_str(), dbl(got), expect);
        }
    }
    // text -> number -> text is stable
    for (int i = 0; i < 2000; ++i) {
        const double d = randomDouble(r, -1000, 1000);
        const std::string s = fmt(d);
        CHECK_EQ(fmt(dbl(val(s.c_str()))), s);
    }
}

std::string usingNum(const char* f, std::initializer_list<double> xs, int mode = 0) {
    rt_using u;
    rt_using_begin(&u, f, static_cast<std::int32_t>(std::strlen(f)), mode);
    std::string r;
    char buf[512];
    for (double x : xs) r.append(buf, static_cast<std::size_t>(rt_using_num(&u, buf, sizeof buf, bits(x))));
    r.append(buf, static_cast<std::size_t>(rt_using_end(&u, buf, sizeof buf)));
    return r;
}

std::string usingStr(const char* f, const char* s) {
    rt_using u;
    rt_using_begin(&u, f, static_cast<std::int32_t>(std::strlen(f)), 0);
    char buf[512];
    std::string r(buf, static_cast<std::size_t>(rt_using_str(&u, buf, sizeof buf, s, static_cast<std::int32_t>(std::strlen(s)))));
    r.append(buf, static_cast<std::size_t>(rt_using_end(&u, buf, sizeof buf)));
    return r;
}

std::string strFmt(double x, int y, int z, int mode = 0) {
    char buf[300];
    return std::string(buf, static_cast<std::size_t>(rt_str_fmt(buf, sizeof buf, bits(x), y, z, mode)));
}

std::string strN(double x, int n) {
    char buf[300];
    return std::string(buf, static_cast<std::size_t>(rt_str_n(buf, sizeof buf, bits(x), n)));
}

void testUsing() {
    const double pi = 3.141592653589793;
    // probe4 T10-T30 (handbook examples included)
    CHECK_EQ(usingNum("#.####", {pi}), std::string("3.1416"));
    CHECK_EQ(usingNum("###.##^^^^", {16}), std::string(" 16.00E+00"));
    CHECK_EQ(usingNum("###.##^^^^", {4100}), std::string("  4.10E+03"));
    CHECK_EQ(usingNum("#.##^^^^", {0.000123}), std::string("1.23E-04"));
    CHECK_EQ(usingNum("#", {0.5}), std::string("1"));
    CHECK_EQ(usingNum("#", {1.5}), std::string("2"));
    CHECK_EQ(usingNum("#", {2.5}), std::string("3"));
    CHECK_EQ(usingNum("#.#", {0.25}), std::string("0.3"));
    CHECK_EQ(usingNum("#.#", {0.35}), std::string("0.4"));  // rounded from the 13-digit decimal
    CHECK_EQ(usingNum("#", {-2.5}), std::string("3"));     // no room for the sign
    CHECK_EQ(usingNum("##", {1234}), std::string("34"));   // leading digits dropped
    CHECK_EQ(usingNum("-##.#", {-1.25}), std::string("- 1.3"));
    CHECK_EQ(usingNum("-##.#", {1.25}), std::string("  1.3"));
    CHECK_EQ(usingNum("+##", {5}), std::string("+ 5"));
    CHECK_EQ(usingNum("##.#-", {-3}), std::string(" 3.0-"));
    CHECK_EQ(usingNum("***#", {12}), std::string("**12"));
    CHECK_EQ(usingNum("$##.##", {3.5}), std::string(" $3.50"));
    CHECK_EQ(usingNum("###.##", {-0.5}), std::string(" -0.50"));
    CHECK_EQ(usingNum("#,###.##", {1234.5}), std::string("1,234.50"));
    CHECK_EQ(usingNum("PI_._._. #.###", {pi}), std::string("PI... 3.142"));
    CHECK_EQ(usingStr("&", "abc"), std::string("abc"));
    CHECK_EQ(usingStr("!", "abc"), std::string("a"));
    CHECK_EQ(usingStr("\\  \\", "abcdef"), std::string("abcd"));
    CHECK_EQ(usingStr("\\  \\", "ab"), std::string("ab  "));
    // probe5 T21-T29; formats without a digit before the point as decided in D12
    CHECK_EQ(usingNum(".##", {0.5}), std::string(".50"));
    CHECK_EQ(usingNum(".##", {0.25}), std::string(".25"));
    CHECK_EQ(usingNum(".#", {0.25}), std::string(".3"));
    CHECK_EQ(usingNum("#.##", {0.5}), std::string("0.50"));
    CHECK_EQ(usingNum("##", {-5}), std::string("-5"));
    CHECK_EQ(usingNum("##", {-15}), std::string("15"));
    CHECK_EQ(usingNum("#.#", {99.96}), std::string("0.0"));
    CHECK_EQ(usingNum("##.##", {0.004999}), std::string(" 0.00"));
    CHECK_EQ(usingNum("##.##", {0.005}), std::string(" 0.01"));
    // several values: the format starts over (D12)
    CHECK_EQ(usingNum("##.#", {1.25, 2.75}), std::string(" 1.3 2.8"));
    CHECK_EQ(usingNum("##.# ", {1.25, 2.75}), std::string(" 1.3  2.8 "));
    CHECK_EQ(usingNum("A## B## C", {1, 2, 3}), std::string("A 1 B 2 CA 3 B## C"));
    // probe7: the rest of the format after the last value, '?' for a value without a field
    CHECK_EQ(usingNum("A## B## C", {1}), std::string("A 1 B## C"));
    CHECK_EQ(usingNum("Summe:", {5}), std::string("Summe:?5"));
    CHECK_EQ(usingNum("Summe:", {5, 7}), std::string("Summe:?5?7"));
    CHECK_EQ(usingNum("&", {1.5}), std::string("&?1.5"));
    CHECK_EQ(usingStr("##", "x"), std::string("##?x"));
    CHECK_EQ(usingStr("!", ""), std::string(" "));
    CHECK_EQ(usingNum("##.##", {-0.001}), std::string("-0.00"));
    CHECK_EQ(usingNum("-#.##", {-0.001}), std::string("-0.00"));
    CHECK_EQ(usingNum("#.##-", {-0.001}), std::string("0.00-"));
    CHECK_EQ(usingNum("#.##^^^^", {1e100}), std::string("1.00E+00"));  // exponent digits dropped
    CHECK_EQ(usingNum("##.##^^^", {5}), std::string(" 5.00E+0"));
    CHECK_EQ(usingNum("$####", {12}), std::string("  $12"));
    CHECK_EQ(usingNum("#$##", {12}), std::string(" $12"));
    CHECK_EQ(usingNum("+#.#", {-0.25}), std::string("-0.3"));
    CHECK_EQ(usingNum("##.##-", {5}), std::string(" 5.00 "));
    // probe8: exponent = multiple of the digit positions before the point; minus only with a
    // sign position
    struct Exp {
        const char* f;
        double x;
        const char* s;
    };
    const Exp exps[] = {
        {"###.##^^^^", 1, "  1.00E+00"},
        {"###.##^^^^", 9.999, " 10.00E+00"},    {"###.##^^^^", 123, "123.00E+00"},
        {"###.##^^^^", 999.9, "999.90E+00"},    {"###.##^^^^", 1234, "  1.23E+03"},
        {"###.##^^^^", 12345, " 12.35E+03"},    {"###.##^^^^", 123456, "123.46E+03"},
        {"###.##^^^^", 1234567, "  1.23E+06"},  {"###.##^^^^", 1e10, " 10.00E+09"},
        {"###.##^^^^", -16, " 16.00E+00"},      {"##.##^^^^", 0.5, "50.00E-02"},
        {"##.##^^^^", 0.05, " 5.00E-02"},       {"##.##^^^^", 0.00123, "12.30E-04"},
        {"##.##^^^^", 0.000123, " 1.23E-04"},   {"##.##^^^^", 999.9, "10.00E+02"},
        {"##.##^^^^", 1234, "12.34E+02"},       {"##.##^^^^", 1e10, " 1.00E+10"},
        {"##.##^^^^", -16, "16.00E+00"},        {"#.##^^^^", 0.00123, "1.23E-03"},
        {"#.##^^^^", 9.999, "1.00E+01"},        {"#.##^^^^", 999.9, "1.00E+03"},
        {"#.##^^^^", -0.05, "5.00E-02"},        {"####.#^^^^", 0.5, "5000.0E-04"},
        {"####.#^^^^", 0.000123, "   1.2E-04"}, {"####.#^^^^", 1234, "1234.0E+00"},
        {"####.#^^^^", 1234567, " 123.5E+04"},  {"####.#^^^^", 1e10, " 100.0E+08"},
        {"####.#^^^^", -16, "  16.0E+00"},
        // the original differs for 3 digits and |x| < 1 (D14): 0.5 -> "  5.00E-01",
        // 0.05 -> "500.00E-04", 0.000123 -> "  1.23E-04"
        {"###.##^^^^", 0.5, "500.00E-03"},      {"###.##^^^^", 0.05, " 50.00E-03"},
        {"###.##^^^^", 0.000123, "123.00E-06"},
        // and prints wrong values for formats without a digit before the point
        {".##^^^^", 16, ".16E+02"},             {".##^^^^", 0.05, ".50E-01"},
    };
    for (const Exp& c : exps) CHECK_EQ(usingNum(c.f, {c.x}), std::string(c.s));
    // MODE (probe4 T32)
    CHECK_EQ(usingNum("#,###.##", {1234.5}, 1), std::string("1,234.50"));
    CHECK_EQ(usingNum("#.###,##", {1234.5}, 2), std::string("1.234,50"));
    CHECK_EQ(usingNum("#,###.##", {1234.5}, 3).substr(0, 5), std::string("4,500"));
    // further cases
    CHECK_EQ(usingNum("#,###", {12}), std::string("   12"));
    CHECK_EQ(usingNum("#,###,###", {1234567}), std::string("1,234,567"));
    CHECK_EQ(usingNum("-#.##^^^^", {-1234}), std::string("-1.23E+03"));
    CHECK_EQ(usingNum("#.##^^^^", {0}), std::string("0.00E+00"));
    CHECK_EQ(usingNum("#.#^^^^", {9.96}), std::string("1.0E+01"));
    CHECK_EQ(usingNum("###.#^^^^", {0.000123}), std::string("123.0E-06"));
    CHECK_EQ(usingNum("**$##.##", {3.5}), std::string("***$3.50"));
    CHECK_EQ(usingNum("###.##", {1e300}).size(), std::size_t{6});
    // STR$(x,y,z) and STR$(x,n): probe4 T33/T34/T32, probe5 T19/T20
    CHECK_EQ(strFmt(123.4567, 6, 2), std::string("123.46"));
    CHECK_EQ(strFmt(pi, 5, 3), std::string("3.142"));
    CHECK_EQ(strFmt(-1.5, 4, 0), std::string("  -2"));
    CHECK_EQ(strFmt(2.5, 3, 0), std::string("  3"));
    CHECK_EQ(strFmt(0.05, 4, 1), std::string(" 0.1"));
    CHECK_EQ(strFmt(12345.6, 4, 1), std::string("45.6"));
    CHECK_EQ(strFmt(12345.6, 3, 0), std::string("346"));
    CHECK_EQ(strFmt(-1.25, 3, 1), std::string("1.3"));
    CHECK_EQ(strFmt(1, 0, 0), std::string(""));
    CHECK_EQ(strFmt(1234.5, 8, 2, 2), std::string(" 1234,50"));
    CHECK_EQ(strN(42, 5), std::string("   42"));
    CHECK_EQ(strN(123456, 3), std::string("123"));
    CHECK_EQ(strN(-12, 2), std::string("-1"));
    CHECK_EQ(strN(1.5, 2), std::string("1."));
    CHECK_EQ(strN(12, 5), std::string("   12"));
    CHECK_EQ(strN(-0.5, 3), std::string("-0."));
    CHECK_EQ(strN(1, 0), std::string(""));
}

void testRandom() {
    rt_randomize(1);
    std::uint32_t x = 1;
    for (int i = 0; i < 1000; ++i) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        CHECK_EQ(rt_rnd_next(), x);
    }
    rt_randomize(0);  // 0 cannot be a state
    CHECK(rt_rnd_next() != 0);
    rt_randomize(12345);
    for (int i = 0; i < 1000; ++i) {
        const double d = dbl(rt_rnd());
        CHECK(d >= 0 && d < 1);
    }
    rt_randomize(7);
    const std::uint32_t s = rt_rnd_next();
    rt_randomize(7);
    CHECK_EQ(rt_rand(100), static_cast<std::int32_t>(((s >> 16) * 100u) >> 16));
}

}  // namespace

extern "C" void rt_error(int code) {
    g_error = code;
}

// Transcendental functions (rt_math.c): not correctly rounded, but within a few units of the last
// place of the host libm (measured: at most 3); errors as on the original (probe13).
void testMath() {
    struct Fn {
        const char* name;
        f64 (*ref)(f64);
        double (*host)(double);
        double lo, hi;
    };
    const Fn fns[] = {{"exp", rt_exp, [](double x) { return std::exp(x); }, -700, 700},
                      {"log", rt_log, [](double x) { return std::log(x); }, 1e-300, 1e300},
                      {"log10", rt_log10, [](double x) { return std::log10(x); }, 1e-30, 1e30},
                      {"sin", rt_sin, [](double x) { return std::sin(x); }, -1e8, 1e8},
                      {"cos", rt_cos, [](double x) { return std::cos(x); }, -100, 100},
                      {"tan", rt_tan, [](double x) { return std::tan(x); }, -1.5, 1.5},
                      {"atn", rt_atn, [](double x) { return std::atan(x); }, -1e3, 1e3},
                      {"asin", rt_asin, [](double x) { return std::asin(x); }, -1, 1},
                      {"acos", rt_acos, [](double x) { return std::acos(x); }, -1, 1}};
    Rng r;
    for (const Fn& f : fns) {
        double worst = 0;
        for (int i = 0; i < 20000; ++i) {
            const double u = static_cast<double>(r.next() >> 11) / 9007199254740992.0;
            double x = f.lo + (f.hi - f.lo) * u;
            if (f.lo > 0) x = std::exp(std::log(f.lo) + (std::log(f.hi) - std::log(f.lo)) * u);
            const double want = f.host(x), got = dbl(f.ref(bits(x)));
            if (want == 0) continue;
            const double ulp = std::nextafter(std::fabs(want), INFINITY) - std::fabs(want);
            worst = std::max(worst, std::fabs(got - want) / ulp);
        }
        ++g_checks;
        if (worst > 4) {
            ++g_failures;
            std::fprintf(stderr, "rt_%s: %.1f ulp from the host libm\n", f.name, worst);
        }
    }
    auto err = [](f64 (*fn)(f64), double x) {
        g_error = -1;
        fn(bits(x));
        return g_error;
    };
    CHECK_EQ(err(rt_log, -1), 6);
    CHECK_EQ(err(rt_log10, 0), 6);
    CHECK_EQ(err(rt_asin, 2), 67);
    CHECK_EQ(err(rt_acos, -1.5), 67);
    CHECK_EQ(err(rt_exp, 710), 1);
    CHECK_EQ(err(rt_exp, -30000), -1);
    CHECK_EQ(rt_exp(bits(-30000)), F64_ZERO);
    CHECK_EQ(rt_exp(F64_ZERO), F64_ONE);
    CHECK_EQ(rt_log(F64_ONE), F64_ZERO);
    CHECK_EQ(rt_sin(F64_ZERO), F64_ZERO);
    CHECK_EQ(rt_cos(F64_ZERO), F64_ONE);
    CHECK(std::fabs(dbl(rt_sin(bits(1e300)))) <= 1);  // huge arguments: inaccurate, but a sine
    CHECK(std::fabs(dbl(rt_sin(bits(1e10))) - std::sin(1e10)) < 1e-12);
    g_error = -1;
    rt_powf(F64_ZERO, bits(-0.5));
    CHECK_EQ(g_error, 0);  // 0^-0.5: divide by zero
    CHECK_EQ(rt_powf(F64_ZERO, bits(0.5)), F64_ZERO);
    g_error = -1;
    rt_powf(bits(-8), bits(1.0 / 3));
    CHECK_EQ(g_error, 6);  // (-8)^(1/3): LOG of a negative number (probe13 T8)
}

int main() {
    testMath();
    testArithmetic();
    testCanon();
    testIntegers();
    testApproxEq();
    testFormat();
    testVal();
    testUsing();
    testRandom();
    return report("runtime_ref");
}
