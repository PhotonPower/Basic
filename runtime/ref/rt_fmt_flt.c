//! module    rt_fmt_flt
//! provides  rt_fmt_flt
//! requires  ix_bn
//! group     float
//! doc       Float -> text as PRINT/STR$ (13 significant digits or DEFNUM), from the exact binary value.

#include "fp_internal.h"
#include "ix_bn.h"

/* floor(a / b) for b > 0 */
static int32_t rt_fmt_flt__floordiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b) != 0 && a < 0) --q;
    return (int32_t)q;
}

/* Rules determined on the original (docs/numerics.md, probe1/probe2):
 *  - the value is rounded to `digits` significant digits, ties away from 0;
 *  - without exponent if the decimal exponent d of the rounded value is -5 <= d <= 13,
 *    otherwise d.ddd E+dd with at least one decimal and at least two exponent digits;
 *  - trailing zeros of the fraction are dropped, "0.5" with a leading zero, no space before
 *    positive numbers. */
int rt_fmt_flt(char *buf, f64 a, int digits) {
    const fp_parts x = fp_unpack(a);
    char ds[16];
    char *o = buf;
    uint64_t pow_hi = 1, pow_lo, q = 0;
    int32_t d, s;
    int i, len;
    ix_bn num, den;
    if (digits < 2) digits = 2; /* DEFNUM 1 prints 2 digits on the original (probe6 T8) */
    if (digits > 13) digits = 13;
    if (x.zero) {
        buf[0] = '0';
        buf[1] = 0;
        return 1;
    }
    if (x.neg) *o++ = '-';
    for (i = 0; i < digits; ++i) pow_hi *= 10; /* 10^digits */
    pow_lo = pow_hi / 10;
    /* estimate of floor(log10 |x|) from the binary exponent; corrected below */
    d = rt_fmt_flt__floordiv((int64_t)(x.e + 52) * 78913, 262144);
    for (;;) {
        /* q = floor(|x| * 10^s) with s = digits-1-d, so that 10^(digits-1) <= q < 10^digits */
        s = digits - 1 - d;
        ix_bn_set_u64(&num, x.m);
        ix_bn_set_u64(&den, 1);
        if (s >= 0) ix_bn_mul_pow10(&num, s);
        else ix_bn_mul_pow10(&den, -s);
        if (x.e >= 0) ix_bn_shl(&num, x.e);
        else ix_bn_shl(&den, -x.e);
        q = ix_bn_divq(&num, &den, 50);
        if (q >= pow_hi) ++d;
        else if (q < pow_lo) --d;
        else break;
    }
    ix_bn_shl(&num, 1); /* remainder * 2 against the divisor: round half away from 0 */
    if (ix_bn_cmp(&num, &den) >= 0) {
        ++q;
        if (q == pow_hi) {
            q = pow_lo;
            ++d;
        }
    }
    for (i = digits - 1; i >= 0; --i) {
        ds[i] = (char)('0' + (int)(q % 10));
        q /= 10;
    }
    len = digits;
    while (len > 1 && ds[len - 1] == '0') --len;
    if (d >= -5 && d <= 13) {
        if (d >= 0) {
            for (i = 0; i <= d; ++i) *o++ = i < len ? ds[i] : '0';
            if (len > d + 1) {
                *o++ = '.';
                for (i = d + 1; i < len; ++i) *o++ = ds[i];
            }
        } else {
            *o++ = '0';
            *o++ = '.';
            for (i = -1; i > d; --i) *o++ = '0';
            for (i = 0; i < len; ++i) *o++ = ds[i];
        }
    } else {
        int32_t ex = d < 0 ? -d : d;
        char es[8];
        int n = 0;
        *o++ = ds[0];
        *o++ = '.';
        if (len == 1) *o++ = '0';
        for (i = 1; i < len; ++i) *o++ = ds[i];
        *o++ = 'E';
        *o++ = d < 0 ? '-' : '+';
        do {
            es[n++] = (char)('0' + ex % 10);
            ex /= 10;
        } while (ex != 0);
        if (n < 2) es[n++] = '0';
        while (n > 0) *o++ = es[--n];
    }
    *o = 0;
    return (int)(o - buf);
}
