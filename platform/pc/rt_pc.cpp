#include "rt_pc.hpp"

#include <cctype>

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
    switch (code & 0xFF) {  // further texts seen on the original
        case 45: return "too many parameters";  // probe10
        case 67: return "asin/acos error";      // probe13 T5
        case 202: return "object in use";       // probe14 T7 (AmigaDOS ERROR_OBJECT_IN_USE)
        case 205: return "object not found";    // probe14 T7 (AmigaDOS ERROR_OBJECT_NOT_FOUND)
        case 203: return "object already exists";  // probe16 T9
        case 219: return "seek error";            // probe16 T7
        case 50: return "fields greater than record";  // probe16 T6 (the original shows a garbled text)
        case 54: return "get/put field string|len changed";  // probe16 T3
        case 55: return "get/put: illegal recordnumber";     // probe16 T4
        case 68: return "var-type mismatch";    // probe10
        default: break;
    }
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
// other exponents through rt_powf.
f64 power(f64 x, f64 y) {
    const f64 t = fp_trunc(y);
    if (fp_cmp(t, y) != 0) {  // not an integer: EXP(y*LOG(x)) (probe13 T8-T10, T19)
        const f64 r = rt_powf(x, y);
        check();
        return r;
    }
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
f64 expf(f64 x) {
    const f64 r = rt_exp(x);
    check();
    return r;
}
f64 logf(f64 x) {
    const f64 r = rt_log(x);
    check();
    return r;
}
f64 log10f(f64 x) {
    const f64 r = rt_log10(x);
    check();
    return r;
}
f64 sinf(f64 x) {
    const f64 r = rt_sin(x);
    check();
    return r;
}
f64 cosf(f64 x) {
    const f64 r = rt_cos(x);
    check();
    return r;
}
f64 tanf(f64 x) {
    const f64 r = rt_tan(x);
    check();
    return r;
}
f64 atnf(f64 x) {
    const f64 r = rt_atn(x);
    check();
    return r;
}
f64 asinf(f64 x) {
    const f64 r = rt_asin(x);
    check();
    return r;
}
f64 acosf(f64 x) {
    const f64 r = rt_acos(x);
    check();
    return r;
}
f64 degf(f64 x) {
    const f64 r = rt_deg(x);
    check();
    return r;
}
f64 radf(f64 x) {
    const f64 r = rt_rad(x);
    check();
    return r;
}
f64 sinq(f64 x) {
    toInt(x);
    return sinf(radf(x));
}
f64 cosq(f64 x) {
    toInt(x);
    return cosf(radf(x));
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

// ---- files -------------------------------------------------------------------------------------
namespace {

std::string upperName(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

}  // namespace

Files::Channel& Files::get(f64 channel, bool open) {
    const std::int32_t n = toInt(channel);
    if (n < 0 || n > 99) throw Error{23};  // file # wrong
    Channel& c = ch_[n];
    if (open && c.f == nullptr) throw Error{RT_ERR_NOT_OPEN};
    return c;
}

bool Files::inUse(const std::string& name, const Channel* except) const {
    const std::string u = upperName(name);
    for (const Channel& c : ch_)
        if (&c != except && c.f != nullptr && upperName(c.name) == u) return true;
    return false;
}

void Files::open(const std::string& mode, f64 channel, const std::string& name, f64 recordLength) {
    const char m = mode.empty() ? ' ' : static_cast<char>(std::toupper(static_cast<unsigned char>(mode[0])));
    if (m != 'O' && m != 'I' && m != 'A' && m != 'U' && m != 'R') throw Error{21};  // open allows i o r a u
    Channel& c = get(channel, false);
    if (c.f != nullptr) throw Error{22};  // file already open
    if (inUse(name, &c)) throw Error{kErrObjectInUse};
    std::FILE* f = nullptr;
    if (m == 'U' || m == 'R') {  // read and write; a missing file is created (probe16 T7)
        f = std::fopen(name.c_str(), "r+b");
        if (f == nullptr) f = std::fopen(name.c_str(), "w+b");
    } else {
        f = std::fopen(name.c_str(), m == 'O' ? "wb" : m == 'A' ? "ab" : "rb");
    }
    if (f == nullptr) throw Error{m == 'I' ? kErrObjectNotFound : kErrObjectInUse};
    c.f = f;
    c.input = m == 'I' || m == 'U' || m == 'R';
    c.output = m != 'I';
    c.writing = false;
    c.reclen = 0;
    if (m == 'R') {  // without a length: 128 (probe16 T8)
        c.reclen = toInt(recordLength);
        if (c.reclen <= 0) c.reclen = 128;
    }
    c.next = 0;
    c.fields.clear();
    c.lengths.clear();
    c.name = name;
    c.out = Output();
}

void Files::toRead(Channel& c) {
    if (c.writing) std::fseek(c.f, 0, SEEK_CUR);
    c.writing = false;
}

void Files::toWrite(Channel& c) {
    if (!c.writing) std::fseek(c.f, 0, SEEK_CUR);
    c.writing = true;
}

std::int32_t Files::size(Channel& c) {
    std::fflush(c.f);
    const long pos = std::ftell(c.f);
    std::fseek(c.f, 0, SEEK_END);
    const long len = std::ftell(c.f);
    std::fseek(c.f, pos, SEEK_SET);
    return static_cast<std::int32_t>(len);
}

void Files::name(const std::string& from, const std::string& to) {
    if (!exist(from)) throw Error{kErrObjectNotFound};
    if (exist(to)) throw Error{kErrObjectExists};  // object already exists (probe16 T9)
    if (inUse(from, nullptr)) throw Error{kErrObjectInUse};
    std::rename(from.c_str(), to.c_str());
}

void Files::field(f64 channel, const std::vector<std::string*>& vars, const std::vector<f64>& lengths) {
    Channel& c = get(channel, true);
    std::vector<std::int32_t> lens;
    std::int64_t sum = 0;
    for (f64 l : lengths) {
        lens.push_back(toInt(l));
        sum += lens.back();
    }
    if (sum > c.reclen) throw Error{50};  // fields greater than record (also for other modes)
    c.fields = vars;
    c.lengths = lens;
    for (std::size_t k = 0; k < vars.size(); ++k) *vars[k] = std::string(static_cast<std::size_t>(std::max(0, lens[k])), ' ');
}

// Order of the checks as on the original (probe16 T4): record number (55), for GET the end of the
// file (26), then the lengths of the field strings (54).
std::size_t Files::recordIndex(Channel& c, bool numbered, f64 record) {
    if (c.reclen == 0) throw Error{50};
    if (!numbered) return static_cast<std::size_t>(std::max(0, c.next));
    const std::int32_t r = toInt(record);
    if (r < 1) throw Error{55};  // get/put: illegal recordnumber
    return static_cast<std::size_t>(r - 1);
}

namespace {

void checkFieldLengths(const std::vector<std::string*>& fields, const std::vector<std::int32_t>& lengths) {
    for (std::size_t k = 0; k < fields.size(); ++k)  // the field strings must keep their length
        if (static_cast<std::int32_t>(fields[k]->size()) != lengths[k]) throw Error{54};
}

}  // namespace

void Files::get(f64 channel, bool numbered, f64 record) {
    Channel& c = get(channel, true);
    const std::size_t i = recordIndex(c, numbered, record);
    if (static_cast<std::int64_t>(i + 1) * c.reclen > size(c)) throw Error{26};  // end of file
    checkFieldLengths(c.fields, c.lengths);
    toRead(c);
    std::fseek(c.f, static_cast<long>(i) * c.reclen, SEEK_SET);
    std::string rec(static_cast<std::size_t>(c.reclen), ' ');
    const std::size_t got = std::fread(rec.data(), 1, rec.size(), c.f);
    rec.resize(got);
    std::size_t pos = 0;
    for (std::size_t k = 0; k < c.fields.size(); ++k) {
        *c.fields[k] = rec.substr(std::min(pos, rec.size()), static_cast<std::size_t>(c.lengths[k]));
        pos += static_cast<std::size_t>(c.lengths[k]);
    }
    c.next = static_cast<std::int32_t>(i + 1);
}

void Files::put(f64 channel, bool numbered, f64 record) {
    Channel& c = get(channel, true);
    const std::size_t i = recordIndex(c, numbered, record);
    if (static_cast<std::int64_t>(i) * c.reclen > size(c)) throw Error{55};  // at most one record beyond the end
    checkFieldLengths(c.fields, c.lengths);
    std::string rec;
    for (const std::string* f : c.fields) rec += *f;
    rec.resize(static_cast<std::size_t>(c.reclen), ' ');
    toWrite(c);
    std::fseek(c.f, static_cast<long>(i) * c.reclen, SEEK_SET);
    std::fwrite(rec.data(), 1, rec.size(), c.f);
    c.next = static_cast<std::int32_t>(i + 1);
}

void Files::record(f64 channel, f64 n) { get(channel, true).next = toInt(n); }

void Files::seek(f64 channel, f64 posf) {
    Channel& c = get(channel, true);
    const std::int32_t pos = toInt(posf);
    const std::int32_t len = size(c);
    std::fflush(c.f);
    c.writing = false;
    if (pos < 0 || pos > len) {
        std::fseek(c.f, 0, SEEK_END);
        throw Error{kErrSeek};  // seek error; the position is the end (probe16 T7)
    }
    std::fseek(c.f, pos, SEEK_SET);
}

void Files::relseek(f64 channel, f64 n) {
    Channel& c = get(channel, true);
    std::fflush(c.f);
    seek(channel, fp_add(fp_from_i32(static_cast<std::int32_t>(std::ftell(c.f))), n));
}

std::int32_t Files::loc(f64 channel) {
    Channel& c = get(channel, true);
    if (c.reclen != 0) return 0;
    std::fflush(c.f);
    return static_cast<std::int32_t>(std::ftell(c.f));
}

std::int32_t Files::inp(f64 channel) {
    Channel& c = get(channel, true);
    if (!c.input) throw Error{RT_ERR_NOT_OPEN};
    toRead(c);
    const int b = std::fgetc(c.f);
    if (b == EOF) throw Error{26};
    return b;
}

void Files::out(f64 channel, f64 byte) {
    Channel& c = get(channel, true);
    if (!c.output) throw Error{RT_ERR_NOT_OPEN};
    toWrite(c);
    std::fputc(toInt(byte) & 0xFF, c.f);
}

void Files::close(f64 channel) {
    Channel& c = get(channel, false);
    if (c.f == nullptr) return;
    std::fclose(c.f);
    c.f = nullptr;
}

void Files::closeAll() {
    for (Channel& c : ch_)
        if (c.f != nullptr) {
            std::fclose(c.f);
            c.f = nullptr;
        }
}

Output& Files::output(f64 channel, const Output& screen) {
    Channel& c = get(channel, true);
    if (!c.output) throw Error{RT_ERR_NOT_OPEN};
    c.out.defnum = screen.defnum;
    c.out.mode = screen.mode;
    return c.out;
}

void Files::flush(f64 channel) {
    Channel& c = get(channel, true);
    if (c.out.buf.empty()) return;
    toWrite(c);
    std::fwrite(c.out.buf.data(), 1, c.out.buf.size(), c.f);
    c.out.buf.clear();
}

int Files::peekByte(Channel& c) {
    toRead(c);
    const int b = std::fgetc(c.f);
    if (b != EOF) std::ungetc(b, c.f);
    return b;
}

std::string Files::lineInput(f64 channel) {
    Channel& c = get(channel, true);
    if (!c.input) throw Error{RT_ERR_NOT_OPEN};
    if (peekByte(c) == EOF) throw Error{26};  // end of file
    std::string s;
    for (int b; (b = std::fgetc(c.f)) != EOF && b != '\n';) s += static_cast<char>(b);
    return s;
}

// One item: up to a comma or the end of the line; in quotes everything up to the closing quote
// (then on to the comma). Unquoted text keeps its spaces (probe14 T4).
std::string Files::item(Channel& c) {
    if (!c.input) throw Error{RT_ERR_NOT_OPEN};
    if (peekByte(c) == EOF) throw Error{26};  // end of file
    std::string s;
    int b = std::fgetc(c.f);
    std::string lead;
    while (b == ' ') {
        lead += ' ';
        b = std::fgetc(c.f);
    }
    if (b == '"') {
        while ((b = std::fgetc(c.f)) != EOF && b != '"' && b != '\n') s += static_cast<char>(b);
        if (b == '"')
            while ((b = std::fgetc(c.f)) != EOF && b != ',' && b != '\n') {
            }
        return s;
    }
    s = lead;
    for (; b != EOF && b != ',' && b != '\n'; b = std::fgetc(c.f)) s += static_cast<char>(b);
    return s;
}

std::string Files::inputString(f64 channel) { return item(get(channel, true)); }

f64 Files::inputNumber(f64 channel) {
    const std::string s = trim(item(get(channel, true)));
    std::int32_t used = 0;
    const f64 v = rt_val(s.data(), static_cast<std::int32_t>(s.size()), &used);
    if (s.empty() || used != static_cast<std::int32_t>(s.size())) throw Error{35};  // data not numeric
    return v;
}

std::string Files::inputChars(f64 nf, f64 channel) {
    Channel& c = get(channel, true);
    if (!c.input) throw Error{RT_ERR_NOT_OPEN};
    const std::int32_t n = toInt(nf);
    toRead(c);
    std::string s;
    for (std::int32_t k = 0; k < n; ++k) {
        const int b = std::fgetc(c.f);
        if (b == EOF) throw Error{26};
        s += static_cast<char>(b);
    }
    return s;
}

std::int32_t Files::eof(f64 channel) {
    Channel& c = get(channel, true);
    if (!c.input) return -1;
    if (std::ftell(c.f) >= size(c)) return -1;
    return peekByte(c) == EOF ? -1 : 0;
}

std::int32_t Files::lof(f64 channel) { return size(get(channel, true)); }

std::int32_t Files::exist(const std::string& name) {
    std::FILE* f = std::fopen(name.c_str(), "rb");
    if (f == nullptr) return 0;
    std::fclose(f);
    return -1;
}

void Files::kill(const std::string& name) {
    if (!exist(name)) throw Error{kErrObjectNotFound};
    if (inUse(name, nullptr)) throw Error{kErrObjectInUse};
    std::remove(name.c_str());
}

// ---- keyboard ----------------------------------------------------------------------------------
void Keyboard::read(Output& out, const std::string& prompt) {
    out.put(prompt);
    if (flush) flush();
    if (in == nullptr || !std::getline(*in, line_)) throw Error{26};  // end of file
    if (!line_.empty() && line_.back() == '\r') line_.pop_back();
    pos_ = 0;
    whole_ = true;
}

void Keyboard::begin(Output& out, const std::string& text, int mode) {
    prompt_ = mode == 0 ? "? " : mode == 1 ? text + "? " : text;
    read(out, prompt_);
}

std::string Keyboard::next(Output& out) {
    if (pos_ == std::string::npos) read(out, "? ");  // more values than on the line
    whole_ = false;
    const std::size_t comma = line_.find(',', pos_);
    std::string f;
    if (comma == std::string::npos) {
        f = line_.substr(pos_);
        pos_ = std::string::npos;
    } else {
        f = line_.substr(pos_, comma - pos_);
        pos_ = comma + 1;
    }
    return f;
}

std::string Keyboard::field(Output& out) { return next(out); }

f64 Keyboard::number(Output& out) {
    for (;;) {
        const std::string f = trim(next(out));
        std::int32_t used = 0;
        const f64 v = rt_val(f.data(), static_cast<std::int32_t>(f.size()), &used);
        if (!f.empty() && used == static_cast<std::int32_t>(f.size())) return v;
        read(out, prompt_);  // not a number: the prompt and the input again
    }
}

std::string Keyboard::line(Output& out) {
    if (!whole_) read(out, "? ");
    whole_ = false;
    pos_ = std::string::npos;
    return line_;
}

std::string Keyboard::inkey() {
    if (flush) flush();
    if (in == nullptr) return "";
    const int c = in->get();
    if (c == std::char_traits<char>::eof()) return "";
    return std::string(1, c == '\n' ? '\r' : static_cast<char>(c));
}

std::string Keyboard::chars(f64 nf) {
    if (flush) flush();
    const std::int32_t n = toInt(nf);
    std::string s;
    for (std::int32_t k = 0; k < n; ++k) {
        const int c = in == nullptr ? std::char_traits<char>::eof() : in->get();
        if (c == std::char_traits<char>::eof()) throw Error{26};
        s += c == '\n' ? '\r' : static_cast<char>(c);
    }
    return s;
}

void writeValue(Output& out, f64 x, bool first) {
    if (!first) out.put(",");
    out.value(x);
}

void writeValue(Output& out, const std::string& s, bool first) {
    if (!first) out.put(",");
    out.put("\"" + s + "\"");
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
