//! module    rt_flt_dec
//! provides  rt_flt_dec
//! requires  ix_bn
//! group     float
//! doc       Decimal digits of a float: exact binary value rounded to n significant digits, ties away from 0.

#include "fp_internal.h"
#include "ix_bn.h"

/* floor(a / b) for b > 0 */
static int32_t rt_flt_dec__floordiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b) != 0 && a < 0) --q;
    return (int32_t)q;
}

/* Writes `digits` (1..13) significant decimal digits of |x| to ds (no terminator) and the decimal
 * exponent of the first digit to *d, so that |x| ~ 0.d1d2... * 10^(*d+1). Rounds the exact
 * binary value, ties away from 0. For 0: all digits '0', *d = 0. Returns 1 if x is negative. */
int rt_flt_dec(f64 a, int digits, char *ds, int32_t *dexp) {
    const fp_parts x = fp_unpack(a);
    uint64_t pow_hi = 1, pow_lo, q = 0;
    int32_t d, s;
    int i;
    ix_bn num, den;
    if (x.zero) {
        for (i = 0; i < digits; ++i) ds[i] = '0';
        *dexp = 0;
        return 0;
    }
    for (i = 0; i < digits; ++i) pow_hi *= 10; /* 10^digits */
    pow_lo = pow_hi / 10;
    /* estimate of floor(log10 |x|) from the binary exponent; corrected below */
    d = rt_flt_dec__floordiv((int64_t)(x.e + 52) * 78913, 262144);
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
    *dexp = d;
    return x.neg;
}
