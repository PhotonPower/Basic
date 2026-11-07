#include "rt_pc.hpp"

#include <algorithm>

namespace {

// runtime/ref reports errors through rt_error(); it only records the error here, and the caller
// raises it with brt::check() after the call (no C++ exception travels through C code).
thread_local int g_pending = -1;

}  // namespace

extern "C" void rt_error(int code) {
    if (g_pending < 0) g_pending = code;
}

namespace brt {

void check() {
    const int e = g_pending;
    if (e >= 0) {
        g_pending = -1;
        throw Error{e};
    }
}

std::string errorText(int code) {
    // ERR$ texts of GFA-BASIC 3.5 (English), read on the original (probe1-probe9)
    static const char* const texts[] = {
        "divide by zero",                                                   // 0
        "overflow",                                                         // 1
        "no integer|-2147483648 .. 2147483647",                             // 2
        "no byte|0 .. 255",                                                 // 3
        "no word|-32768 .. 32767",                                          // 4
        "no negative squareroot",                                           // 5
        "logarithm only for|numbers greater zero",                          // 6
        "unknown error ",                                                   // 7
        "out of memory",                                                    // 8
        "function or command|not implemented",                              // 9
        "string too long|max. 32767 chars",                                 // 10
        "no GFA-BASIC 3.5 program",                                         // 11
        "program too long|out of memory|new",                               // 12
        "no GFA-BASIC program|eof - new",                                   // 13
        "redimensioned array",                                              // 14
        "array undimensioned",                                              // 15
        "array index out of range",                                         // 16
        "dim out of range",                                                 // 17
        "wrong # of indices",                                               // 18
        "procedure not found",                                              // 19
        "label not found",                                                  // 20
        "open allows:|\"i\"nput  \"o\"utput \"r\"andom|\"a\"ppend \"u\"pdate",  // 21
        "file already open",                                                // 22
        "file # wrong",                                                     // 23
        "file not open",                                                    // 24
        "wrong input, no number",                                           // 25
        "end of file",                                                      // 26
        "too many points for|polyline/polyfill|max. 128",                   // 27
        "array not linear",                                                 // 28
        "# of points too large",                                            // 29
        "merge - no ASCII",                                                 // 30
        "merge - line too long",                                            // 31
        "==> syntax error|program aborted",                                 // 32
        "undefined label",                                                  // 33
        "out of data",                                                      // 34
        "data not numeric",                                                 // 35
    };
    const int c = code & 0xFF;  // ERR$(-56) = "unknown error 200" (probe9 T6)
    if (c < static_cast<int>(sizeof texts / sizeof texts[0])) return texts[c];
    return "unknown error " + std::to_string(c);
}

// ---- numbers and conversions -------------------------------------------------------------------
std::int32_t toInt(f64 x) {
    const std::int32_t r = fp_to_i32(x);
    check();
    return r;
}

std::int32_t convWord(f64 x) {
    const std::int16_t r = fp_to_i16(x);
    check();
    return r;
}

std::int32_t convByte(f64 x) {
    const std::uint8_t r = fp_to_u8(x);
    check();
    return r;
}

std::int32_t convBool(f64 x) {
    return fp_cmp(x, 0) != 0 ? -1 : 0;
}

std::int32_t convWord(std::int32_t x) {
    if (x < -32768 || x > 32767) throw Error{RT_ERR_NO_WORD};
    return x;
}

std::int32_t convByte(std::int32_t x) {
    if (x < 0 || x > 255) throw Error{RT_ERR_NO_BYTE};
    return x;
}

f64 add(f64 a, f64 b) {
    const f64 r = fp_add(a, b);
    check();
    return r;
}

f64 sub(f64 a, f64 b) {
    const f64 r = fp_sub(a, b);
    check();
    return r;
}

f64 mul(f64 a, f64 b) {
    const f64 r = fp_mul(a, b);
    check();
    return r;
}

f64 div(f64 a, f64 b) {
    const f64 r = fp_div(a, b);
    check();
    return r;
}

// x^y: integer exponents by repeated multiplication (exact for small powers such as 2^-26);
// other exponents need EXP/LOG, which the runtime does not provide yet.
f64 power(f64 x, f64 y) {
    const f64 t = fp_trunc(y);
    if (fp_cmp(t, y) != 0) throw Error{RT_ERR_OVERFLOW};  // TODO: EXP(y*LOG(x)) with EXP and LOG
    const f64 lim = 0x41DFFFFFFFC00000u;                    // 2^31-1
    if (fp_cmp(t & ~(std::uint64_t{1} << 63), lim) > 0) throw Error{RT_ERR_OVERFLOW};
    std::int64_t n = toInt(t);
    const bool negative = n < 0;
    if (negative) n = -n;
    f64 r = F64_ONE, b = x;
    while (n > 0) {
        if (n & 1) r = fp_mul(r, b);
        n >>= 1;
        if (n > 0) b = fp_mul(b, b);
        check();
    }
    if (negative) r = fp_div(F64_ONE, r);
    check();
    return r;
}

std::int32_t idiv(std::int32_t a, std::int32_t b) {
    if (b == 0) throw Error{RT_ERR_DIV_ZERO};
    if (b == -1) return static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(a));
    return a / b;
}

std::int32_t imod(std::int32_t a, std::int32_t b) {
    if (b == 0) throw Error{RT_ERR_DIV_ZERO};
    if (b == -1) return 0;
    return a % b;
}

int compare(f64 a, f64 b) {
    return fp_cmp(a, b);
}

int compare(const std::string& a, const std::string& b) {
    return a < b ? -1 : (a > b ? 1 : 0);
}

namespace {

void checkLen(const std::string& s) {
    if (s.size() > 32767) throw Error{RT_ERR_STR_LONG};
}

}  // namespace

std::string concat(std::string a, const std::string& b) {
    a += b;
    checkLen(a);
    return a;
}

// ---- built-in functions ------------------------------------------------------------------------
f64 abs(f64 x) { return x & ~(std::uint64_t{1} << 63); }
std::int32_t sgn(f64 x) { return fp_cmp(x, 0); }
f64 intf(f64 x) {
    const f64 r = fp_int(x);
    check();
    return r;
}
f64 truncf(f64 x) { return fp_trunc(x); }
f64 frac(f64 x) { return fp_frac(x); }
std::int32_t cint(f64 x) { return toInt(fp_cint(x)); }
f64 round(f64 x, std::int32_t n) {
    const f64 r = fp_round(x, n);
    check();
    return r;
}
f64 sqr(f64 x) {
    const f64 r = fp_sqrt(x);
    check();
    return r;
}
std::int32_t odd(f64 x) { return (toInt(x) & 1) != 0 ? -1 : 0; }
std::int32_t even(f64 x) { return (toInt(x) & 1) == 0 ? -1 : 0; }
f64 rnd() { return rt_rnd(); }
std::int32_t random(f64 x) { return toInt(intf(mul(rt_rnd(), x))); }
std::int32_t rand(f64 y) { return rt_rand(toInt(y)); }
void randomize(std::uint32_t seed) { rt_randomize(seed); }

std::int32_t fnAdd(f64 a, f64 b) { return wrapInt(static_cast<std::int64_t>(toInt(a)) + toInt(b)); }
std::int32_t fnSub(f64 a, f64 b) { return wrapInt(static_cast<std::int64_t>(toInt(a)) - toInt(b)); }
std::int32_t fnMul(f64 a, f64 b) { return wrapInt(static_cast<std::int64_t>(toInt(a)) * toInt(b)); }
std::int32_t fnDiv(f64 a, f64 b) { return idiv(toInt(a), toInt(b)); }
std::int32_t fnMod(f64 a, f64 b) { return imod(toInt(a), toInt(b)); }

std::int32_t pred(f64 x) { return wrapInt(static_cast<std::int64_t>(toInt(x)) - 1); }
std::int32_t succ(f64 x) { return wrapInt(static_cast<std::int64_t>(toInt(x)) + 1); }
std::string pred(const std::string& s) {
    return std::string(1, static_cast<char>((s.empty() ? 0 : static_cast<unsigned char>(s[0])) - 1));
}
std::string succ(const std::string& s) {
    return std::string(1, static_cast<char>((s.empty() ? 0 : static_cast<unsigned char>(s[0])) + 1));
}

std::int32_t bitOp(char op, f64 x, f64 bitNumber) {
    const std::uint32_t v = static_cast<std::uint32_t>(toInt(x));
    const std::uint32_t bit = 1u << (toInt(bitNumber) & 31);
    switch (op) {
        case 'T': return (v & bit) != 0 ? -1 : 0;
        case 'C': return static_cast<std::int32_t>(v & ~bit);
        case 'S': return static_cast<std::int32_t>(v | bit);
        default: return static_cast<std::int32_t>(v ^ bit);
    }
}

std::int32_t shift(f64 xf, f64 nf, int width, bool left, bool rotate) {
    const std::uint32_t x = static_cast<std::uint32_t>(toInt(xf));
    std::int32_t n = toInt(nf) & 31;
    const std::uint32_t mask = width == 32 ? 0xFFFFFFFFu : (1u << width) - 1;
    std::uint32_t v = x & mask;
    if (rotate) {
        n %= width;
        if (n != 0) v = left ? ((v << n) | (v >> (width - n))) & mask : ((v >> n) | (v << (width - n))) & mask;
    } else {
        v = n >= width ? 0 : (left ? (v << n) & mask : v >> n);
    }
    if (width == 16) return static_cast<std::int16_t>(static_cast<std::uint16_t>(v));  // sign-extended
    return static_cast<std::int32_t>(v);
}

std::int32_t swapWords(f64 xf) {
    const std::uint32_t x = static_cast<std::uint32_t>(toInt(xf));
    return static_cast<std::int32_t>((x << 16) | (x >> 16));
}
std::int32_t byteOf(f64 x) { return toInt(x) & 0xFF; }
std::int32_t cardOf(f64 x) { return toInt(x) & 0xFFFF; }
std::int32_t wordOf(f64 x) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(toInt(x) & 0xFFFF)); }

std::string left(const std::string& s, f64 nf) {
    const std::int32_t n = toInt(nf);
    if (n < 0) throw Error{RT_ERR_STR_LONG};  // probe4 T45
    return s.substr(0, std::min(static_cast<std::size_t>(n), s.size()));
}

std::string right(const std::string& s, f64 nf) {
    const std::int32_t n = toInt(nf);
    if (n < 0) throw Error{RT_ERR_STR_LONG};
    const std::size_t m = std::min(static_cast<std::size_t>(n), s.size());
    return s.substr(s.size() - m);
}

std::string mid(const std::string& s, f64 fromf, f64 lenf) {
    std::int32_t from = toInt(fromf);
    const std::int32_t n = toInt(lenf);
    if (from < 0 || n < 0) throw Error{RT_ERR_STR_LONG};
    if (from == 0) from = 1;  // probe4 T44
    if (static_cast<std::size_t>(from) > s.size()) return "";
    return s.substr(static_cast<std::size_t>(from - 1), static_cast<std::size_t>(n));
}

std::string mid(const std::string& s, f64 from) {
    return mid(s, from, flt(32767));
}

// Empty search string as decided in D12.
std::int32_t instr(const std::string& a, const std::string& b, f64 fromf) {
    std::int32_t from = toInt(fromf);
    if (from < 0) throw Error{RT_ERR_STR_LONG};
    if (from == 0) from = 1;
    if (b.empty()) return from <= static_cast<std::int32_t>(a.size()) + 1 ? (a.empty() ? 1 : from) : 0;
    if (static_cast<std::size_t>(from) > a.size()) return 0;
    const std::size_t p = a.find(b, static_cast<std::size_t>(from - 1));
    return p == std::string::npos ? 0 : static_cast<std::int32_t>(p + 1);
}

std::string stringOf(f64 nf, const std::string& unit) {
    const std::int32_t n = toInt(nf);
    if (n < 0 || n > 32767) throw Error{RT_ERR_STR_LONG};
    std::string r;
    for (std::int32_t k = 0; k < n; ++k) r += unit;
    checkLen(r);
    return r;
}

std::string stringOf(f64 n, f64 code) {
    return stringOf(n, chr(code));
}

std::string space(f64 nf) {
    const std::int32_t n = toInt(nf);
    if (n < 0 || n > 32767) throw Error{RT_ERR_STR_LONG};
    return std::string(static_cast<std::size_t>(n), ' ');
}

std::string chr(f64 code) { return std::string(1, static_cast<char>(toInt(code) & 0xFF)); }
std::int32_t asc(const std::string& s) { return s.empty() ? 0 : static_cast<unsigned char>(s[0]); }
std::int32_t len(const std::string& s) { return static_cast<std::int32_t>(s.size()); }

std::string str(f64 x, int digits) {
    char buf[RT_FMT_FLT_MAX];
    rt_fmt_flt(buf, x, digits);
    return buf;
}

std::string str(f64 x, f64 n) {
    char buf[512];
    return std::string(buf, static_cast<std::size_t>(rt_str_n(buf, sizeof buf, x, toInt(n))));
}

std::string str(f64 x, f64 y, f64 z, int mode) {
    char buf[512];
    return std::string(buf, static_cast<std::size_t>(rt_str_fmt(buf, sizeof buf, x, toInt(y), toInt(z), mode)));
}

f64 val(const std::string& s) {
    return rt_val(s.data(), static_cast<std::int32_t>(s.size()), nullptr);
}

std::int32_t valCount(const std::string& s) {
    std::int32_t used = 0;
    rt_val(s.data(), static_cast<std::int32_t>(s.size()), &used);
    return used;
}

std::string upper(const std::string& s) {
    std::string r = s;
    for (char& c : r) {
        const unsigned char u = static_cast<unsigned char>(c);
        if ((u >= 'a' && u <= 'z') || (u >= 0xE0 && u <= 0xFE && u != 0xF7)) c = static_cast<char>(u - 32);
    }
    return r;
}

std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(' ');
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(' ') - a + 1);
}

std::string radix(f64 xf, unsigned base, f64 widthf) {
    static const char* digits = "0123456789ABCDEF";
    std::uint32_t v = static_cast<std::uint32_t>(toInt(xf));
    const std::int32_t width = toInt(widthf);
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

std::int32_t pack(const std::string& s) {
    std::uint32_t v = 0;
    for (std::size_t k = 0; k < s.size() && k < 4; ++k) v = (v << 8) | static_cast<unsigned char>(s[k]);
    return static_cast<std::int32_t>(v);
}

// MID$(a$,x[,y])=b$: overwrites, the length of a$ stays (probe4 T46).
void midAssign(std::string& a, f64 fromf, f64 lenf, const std::string& b) {
    std::int32_t from = toInt(fromf);
    const std::int32_t n = toInt(lenf);
    if (from < 1) from = 1;
    for (std::int32_t k = 0; k < n && k < static_cast<std::int32_t>(b.size()); ++k) {
        const std::size_t pos = static_cast<std::size_t>(from - 1 + k);
        if (pos >= a.size()) break;
        a[pos] = b[static_cast<std::size_t>(k)];
    }
}

std::string lset(const std::string& a, const std::string& b) {
    std::string r = b.substr(0, std::min(a.size(), b.size()));
    r.append(a.size() - r.size(), ' ');
    return r;
}

std::string rset(const std::string& a, const std::string& b) {
    const std::string t = b.substr(0, std::min(a.size(), b.size()));
    return std::string(a.size() - t.size(), ' ') + t;
}

f64 dataNumber(const std::string& text, bool quoted) {
    std::int32_t used = 0;
    const f64 v = rt_val(text.data(), static_cast<std::int32_t>(text.size()), &used);
    if (quoted || text.empty() || used != static_cast<std::int32_t>(text.size())) throw Error{35};
    return v;
}

// ---- output ------------------------------------------------------------------------------------
void Output::put(const std::string& s) {
    for (char c : s) {
        buf += c;
        col = c == '\n' ? 0 : col + 1;
    }
}

// TAB(n): spaces up to column n; if the cursor is already beyond it, a new line first.
void Output::tab(f64 nf) {
    const std::int32_t n = toInt(fp_trunc(nf));
    if (n < col) newline();
    if (n > col) put(std::string(static_cast<std::size_t>(n - col), ' '));
}

void Output::spc(f64 nf) {
    const std::int32_t n = toInt(fp_trunc(nf));
    if (n > 0) put(std::string(static_cast<std::size_t>(n), ' '));
}

Using::Using(Output& out, std::string format) : out_(out), format_(std::move(format)) {
    rt_using_begin(&u_, format_.data(), static_cast<std::int32_t>(format_.size()), out.mode);
}

void Using::value(f64 x) {
    char buf[4096];
    out_.put(std::string(buf, static_cast<std::size_t>(rt_using_num(&u_, buf, sizeof buf, x))));
}

void Using::value(const std::string& s) {
    char buf[4096];
    out_.put(std::string(buf, static_cast<std::size_t>(rt_using_str(&u_, buf, sizeof buf, s.data(), static_cast<std::int32_t>(s.size())))));
}

void Using::end() {
    char buf[4096];
    out_.put(std::string(buf, static_cast<std::size_t>(rt_using_end(&u_, buf, sizeof buf))));
}

}  // namespace brt
