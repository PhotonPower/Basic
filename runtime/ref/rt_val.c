//! module    rt_val
//! provides  rt_val
//! requires  ix_bn fp_round_pack fp_from_i32
//! group     float
//! doc       VAL and VAL?: text -> float, correctly rounded; &H $ &O &X % prefixes.

#include "fp_internal.h"
#include "ix_bn.h"

/* At most this many significant digits are kept; further non-zero digits only set a sticky
 * flag. Every halfway point between two binary64 numbers has at most 767 significant digits,
 * so the rounding stays correct. */
#define RT_VAL_DIGITS 800

static int rt_val__digit(char c, int base) {
    int v;
    if (c >= '0' && c <= '9') v = c - '0';
    else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
    else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
    else return -1;
    return v < base ? v : -1;
}

/* Integer with a radix prefix: 32 bit, wrapping, read as a signed long (probe6 T1). */
static f64 rt_val__radix(const char *s, int32_t len, int32_t *i, int base) {
    uint32_t v = 0;
    int any = 0, dv;
    while (*i < len && (dv = rt_val__digit(s[*i], base)) >= 0) {
        v = v * (uint32_t)base + (uint32_t)dv;
        any = 1;
        ++*i;
    }
    if (!any) {
        *i = 0;
        return F64_ZERO;
    }
    return fp_from_i32(v >= 0x80000000u ? -(int32_t)(~v) - 1 : (int32_t)v);
}

f64 rt_val(const char *s, int32_t len, int32_t *used) {
    unsigned char dig[RT_VAL_DIGITS];
    int32_t i = 0, nd = 0, dexp = 0, ex = 0, k, top;
    int neg = 0, any = 0, tail = 0, overflow, sticky;
    uint64_t q;
    f64 r;
    ix_bn num, den;
    if (used != 0) *used = 0;
    while (i < len && s[i] == ' ') ++i;
    /* radix prefixes; no sign before them (VAL("-&H10") = 0, probe6 T2) */
    if (i < len && (s[i] == '$' || s[i] == '%' || s[i] == '&')) {
        int base = 0;
        if (s[i] == '$') base = 16, i += 1;
        else if (s[i] == '%') base = 2, i += 1;
        else if (i + 1 < len) {
            const char c = s[i + 1];
            if (c == 'H' || c == 'h') base = 16;
            else if (c == 'O' || c == 'o') base = 8;
            else if (c == 'X' || c == 'x') base = 2;
            if (base != 0) i += 2;
        }
        if (base == 0) return F64_ZERO;
        r = rt_val__radix(s, len, &i, base);
        if (used != 0) *used = i;
        return r;
    }
    if (i < len && (s[i] == '-' || s[i] == '+')) neg = s[i++] == '-';
    /* decimal: digits [. digits] [E [sign] digits] */
    for (; i < len && s[i] >= '0' && s[i] <= '9'; ++i) {
        any = 1;
        if (nd == 0 && s[i] == '0') continue;
        if (nd < RT_VAL_DIGITS) dig[nd++] = (unsigned char)(s[i] - '0');
        else {
            tail |= s[i] != '0';
            ++dexp;
        }
    }
    if (i < len && s[i] == '.') {
        for (++i; i < len && s[i] >= '0' && s[i] <= '9'; ++i) {
            any = 1;
            if (nd == 0 && s[i] == '0') --dexp;
            else if (nd < RT_VAL_DIGITS) {
                dig[nd++] = (unsigned char)(s[i] - '0');
                --dexp;
            } else tail |= s[i] != '0';
        }
    }
    if (!any) return F64_ZERO;
    if (i < len && (s[i] == 'E' || s[i] == 'e')) {
        int eneg = 0;
        ++i; /* consumed even without digits: VAL?("1E") = 2 on the original */
        if (i < len && (s[i] == '-' || s[i] == '+')) {
            const int32_t j = i + 1;
            if (j < len && s[j] >= '0' && s[j] <= '9') {
                eneg = s[i] == '-';
                i = j;
            }
        }
        for (; i < len && s[i] >= '0' && s[i] <= '9'; ++i)
            if (ex < 100000) ex = ex * 10 + (s[i] - '0');
        dexp += eneg ? -ex : ex;
    }
    if (used != 0) *used = i;
    while (nd > 0 && dig[nd - 1] == 0) { /* trailing zeros into the exponent */
        --nd;
        ++dexp;
    }
    if (nd == 0) return F64_ZERO;
    /* decimal exponent of the leading digit; out of range -> 0 without error (probe3 T20) */
    top = nd + dexp - 1;
    if (top > 309 || top < -330) return F64_ZERO;
    ix_bn_set_u64(&num, 0);
    for (k = 0; k < nd; ++k) ix_bn_mul_add(&num, 10, dig[k]);
    if (dexp >= 0) {
        int32_t bits;
        ix_bn_mul_pow10(&num, dexp);
        bits = ix_bn_bitlen(&num);
        if (bits >= 64) {
            q = ix_bn_top64(&num, &sticky);
            r = fp_round_pack(neg, bits - 64, q | ((sticky || tail) ? 1u : 0u), &overflow);
        } else {
            q = (uint64_t)num.w[0] | (num.n > 1 ? (uint64_t)num.w[1] << 32 : 0);
            r = fp_round_pack(neg, 0, q | (tail ? 1u : 0u), &overflow);
        }
    } else {
        /* q = floor(D * 2^k / 10^-dexp) with 2^62 <= q < 2^64 */
        ix_bn_set_u64(&den, 1);
        ix_bn_mul_pow10(&den, -dexp);
        k = 63 + ix_bn_bitlen(&den) - ix_bn_bitlen(&num);
        if (k >= 0) ix_bn_shl(&num, k);
        else ix_bn_shl(&den, -k);
        q = ix_bn_divq(&num, &den, 63);
        sticky = num.n != 0 || tail;
        r = fp_round_pack(neg, -k, q | (sticky ? 1u : 0u), &overflow);
    }
    return overflow ? F64_ZERO : r;
}
