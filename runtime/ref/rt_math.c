//! module    rt_math
//! provides  rt_exp rt_log rt_log10 rt_sin rt_cos rt_tan rt_atn rt_asin rt_acos rt_powf rt_deg rt_rad
//! requires  fp_add fp_mul fp_div fp_sqrt fp_cmp fp_int fp_to_i32 fp_from_i32 fp_finish rt_error
//! group     float
//! doc       Transcendental functions: deterministic, made only of correctly rounded basic operations (docs/numerics.md).

/* Not correctly rounded, but the same everywhere: every port performs exactly this sequence of
 * basic operations with exactly these constants (exact bit patterns, computed from the exact values
 * with rational arithmetic; tools: see docs/numerics.md). Polynomials are Taylor series, evaluated by
 * Horner's rule from the highest coefficient. Errors as on the original (probe13): LOG of x <= 0 ->
 * 6, ASIN/ACOS outside [-1,1] -> 67, EXP overflow -> 1, EXP underflow -> 0. */

#include "fp_internal.h"

#define C_HALF ((f64)0x3FE0000000000000u)
#define C_TWO ((f64)0x4000000000000000u)
#define C_FOUR ((f64)0x4010000000000000u)
#define C_QUARTER ((f64)0x3FD0000000000000u)
#define C_D180 ((f64)0x4066800000000000u)
#define C_PI ((f64)0x400921FB54442D18u)
#define C_PIO2 ((f64)0x3FF921FB54442D18u)
#define C_PIO4 ((f64)0x3FE921FB54442D18u)
#define C_PIO6 ((f64)0x3FE0C152382D7366u)
#define C_TWO_OVER_PI ((f64)0x3FE45F306DC9C883u)
#define C_PIO2_1 ((f64)0x3FF921FB50000000u) /* pi/2 in four parts, the first three with 26 bits: */
#define C_PIO2_2 ((f64)0x3E5110B460000000u) /* n * part is exact for |n| < 2^27 */
#define C_PIO2_3 ((f64)0x3C91A62630000000u)
#define C_PIO2_4 ((f64)0x3AE8A2E03707344Au)
#define C_LN2_HI ((f64)0x3FE62E42FEE00000u) /* low 32 bits zero: e * LN2_HI is exact */
#define C_LN2_LO ((f64)0x3DEA39EF35793C76u)
#define C_INV_LN2 ((f64)0x3FF71547652B82FEu)
#define C_INV_LN10 ((f64)0x3FDBCB7B1526E50Eu)
#define C_SQRT2 ((f64)0x3FF6A09E667F3BCDu)
#define C_SQRT3 ((f64)0x3FFBB67AE8584CAAu)
#define C_TAN_PI12 ((f64)0x3FD126145E9ECD56u) /* tan(pi/12) = 2 - sqrt(3) */
#define C_EXP_MAX ((f64)0x40862E42FEFA39EFu) /* 709.782712893384: above, EXP overflows */
#define C_EXP_MIN ((f64)0xC087C00000000000u) /* -760: below, EXP is 0 */

/* 1/13! .. 1/2! */
static const f64 rt_math__exp_c[12] = {
    0x3DE6124613A86D09u, 0x3E21EED8EFF8D898u, 0x3E5AE64567F544E4u,
    0x3E927E4FB7789F5Cu, 0x3EC71DE3A556C734u, 0x3EFA01A01A01A01Au,
    0x3F2A01A01A01A01Au, 0x3F56C16C16C16C17u, 0x3F81111111111111u,
    0x3FA5555555555555u, 0x3FC5555555555555u, 0x3FE0000000000000u};

/* sin: 1/17!, -1/15!, .. -1/3! */
static const f64 rt_math__sin_c[8] = {
    0x3CE952C77030AD4Au, 0xBD6AE7F3E733B81Fu, 0x3DE6124613A86D09u,
    0xBE5AE64567F544E4u, 0x3EC71DE3A556C734u, 0xBF2A01A01A01A01Au,
    0x3F81111111111111u, 0xBFC5555555555555u};

/* cos: 1/16!, -1/14!, .. -1/2! */
static const f64 rt_math__cos_c[8] = {
    0x3D2AE7F3E733B81Fu, 0xBDA93974A8C07C9Du, 0x3E21EED8EFF8D898u,
    0xBE927E4FB7789F5Cu, 0x3EFA01A01A01A01Au, 0xBF56C16C16C16C17u,
    0x3FA5555555555555u, 0xBFE0000000000000u};

/* log: 2/23, 2/21, .. 2/3 */
static const f64 rt_math__log_c[11] = {
    0x3FB642C8590B2164u, 0x3FB8618618618618u, 0x3FBAF286BCA1AF28u,
    0x3FBE1E1E1E1E1E1Eu, 0x3FC1111111111111u, 0x3FC3B13B13B13B14u,
    0x3FC745D1745D1746u, 0x3FCC71C71C71C71Cu, 0x3FD2492492492492u,
    0x3FD999999999999Au, 0x3FE5555555555555u};

/* atan: 1/29, -1/27, .. -1/3 */
static const f64 rt_math__atn_c[14] = {
    0x3FA1A7B9611A7B96u, 0xBFA2F684BDA12F68u, 0x3FA47AE147AE147Bu,
    0xBFA642C8590B2164u, 0x3FA8618618618618u, 0xBFAAF286BCA1AF28u,
    0x3FAE1E1E1E1E1E1Eu, 0xBFB1111111111111u, 0x3FB3B13B13B13B14u,
    0xBFB745D1745D1746u, 0x3FBC71C71C71C71Cu, 0xBFC2492492492492u,
    0x3FC999999999999Au, 0xBFD5555555555555u};

static f64 rt_math__abs(f64 x) {
    return x & ~FP_SIGN_BIT;
}

/* p = c[0]; p = p * z + c[k] for k = 1 .. n-1 */
static f64 rt_math__horner(const f64 *c, int n, f64 z) {
    f64 p = c[0];
    int k;
    for (k = 1; k < n; ++k) p = fp_add(fp_mul(p, z), c[k]);
    return p;
}

/* x * 2^k (overflow: error 1; below 2^-1022: 0) */
static f64 rt_math__scale(f64 x, int32_t k) {
    const fp_parts p = fp_unpack(x);
    if (p.zero) return F64_ZERO;
    return fp_finish(p.neg, p.e + k, p.m);
}

f64 rt_exp(f64 x) {
    f64 kf, r, p;
    if (fp_cmp(x, C_EXP_MAX) > 0) {
        rt_error(RT_ERR_OVERFLOW);
        return F64_ZERO;
    }
    if (fp_cmp(x, C_EXP_MIN) < 0) return F64_ZERO;
    /* x = k ln2 + r, |r| <= ln2/2 */
    kf = fp_int(fp_add(fp_mul(x, C_INV_LN2), C_HALF));
    r = fp_sub(fp_sub(x, fp_mul(kf, C_LN2_HI)), fp_mul(kf, C_LN2_LO));
    p = rt_math__horner(rt_math__exp_c, 12, r);
    p = fp_add(fp_mul(p, r), F64_ONE);
    p = fp_add(fp_mul(p, r), F64_ONE);
    return rt_math__scale(p, fp_to_i32(kf));
}

f64 rt_log(f64 x) {
    fp_parts p = fp_unpack(x);
    f64 m, f, s, z, r, l, ef;
    int32_t e;
    if (p.zero || p.neg) {
        rt_error(RT_ERR_LOG);
        return F64_ZERO;
    }
    /* x = 2^e * m, sqrt(1/2) < m <= sqrt(2) */
    e = p.e + 52;
    m = ((f64)1023 << 52) | (p.m & FP_FRAC_MASK);
    if (fp_cmp(m, C_SQRT2) > 0) {
        m = ((f64)1022 << 52) | (p.m & FP_FRAC_MASK);
        ++e;
    }
    /* log(1+f) = 2 atanh(s) with s = f/(2+f), = f - s (f - R), R = 2z/3 + 2z^2/5 + ..., z = s^2 */
    f = fp_sub(m, F64_ONE);
    s = fp_div(f, fp_add(C_TWO, f));
    z = fp_mul(s, s);
    r = fp_mul(rt_math__horner(rt_math__log_c, 11, z), z);
    l = fp_sub(f, fp_mul(s, fp_sub(f, r)));
    ef = fp_from_i32(e);
    return fp_add(fp_mul(ef, C_LN2_HI), fp_add(fp_mul(ef, C_LN2_LO), l));
}

f64 rt_log10(f64 x) {
    return fp_mul(rt_log(x), C_INV_LN10);
}

/* x = n pi/2 + r with |r| <= about pi/4; *q = n mod 4. One pass takes the top 26 bits of
 * n = INT(x 2/pi + 1/2), so that every n * part is exact; for |n| < 2^26 that is all of n and one pass
 * suffices (accurate). Larger arguments need more passes, each shrinking x by about 2^26; the result
 * then carries the rounding errors of the earlier passes (inaccurate, but in [-1,1] and the same
 * everywhere). */
static f64 rt_math__reduce(f64 x, int *q) {
    int quad = 0;
    for (;;) {
        f64 fn;
        if (fp_cmp(rt_math__abs(x), C_PIO4) <= 0) {
            *q = quad & 3;
            return x;
        }
        fn = fp_int(fp_add(fp_mul(x, C_TWO_OVER_PI), C_HALF)) & ~(((f64)1 << 27) - 1);
        quad += (int)fp_to_i32(fp_sub(fn, fp_mul(fp_int(fp_mul(fn, C_QUARTER)), C_FOUR)));
        x = fp_sub(x, fp_mul(fn, C_PIO2_1));
        x = fp_sub(x, fp_mul(fn, C_PIO2_2));
        x = fp_sub(x, fp_mul(fn, C_PIO2_3));
        x = fp_sub(x, fp_mul(fn, C_PIO2_4));
    }
}

static f64 rt_math__ksin(f64 r) {
    const f64 z = fp_mul(r, r);
    return fp_add(r, fp_mul(fp_mul(r, z), rt_math__horner(rt_math__sin_c, 8, z)));
}

static f64 rt_math__kcos(f64 r) {
    const f64 z = fp_mul(r, r);
    return fp_add(F64_ONE, fp_mul(z, rt_math__horner(rt_math__cos_c, 8, z)));
}

f64 rt_sin(f64 x) {
    int q;
    const f64 r = rt_math__reduce(x, &q);
    switch (q) {
        case 0: return rt_math__ksin(r);
        case 1: return rt_math__kcos(r);
        case 2: return fp_neg(rt_math__ksin(r));
        default: return fp_neg(rt_math__kcos(r));
    }
}

f64 rt_cos(f64 x) {
    int q;
    const f64 r = rt_math__reduce(x, &q);
    switch (q) {
        case 0: return rt_math__kcos(r);
        case 1: return fp_neg(rt_math__ksin(r));
        case 2: return fp_neg(rt_math__kcos(r));
        default: return rt_math__ksin(r);
    }
}

f64 rt_tan(f64 x) {
    int q;
    const f64 r = rt_math__reduce(x, &q);
    const f64 s = rt_math__ksin(r), c = rt_math__kcos(r);
    return (q & 1) == 0 ? fp_div(s, c) : fp_neg(fp_div(c, s));
}

f64 rt_atn(f64 x) {
    const int neg = (int)(x >> 63);
    f64 t = rt_math__abs(x), z, a;
    int inv = 0, sixth = 0;
    if (fp_cmp(t, F64_ONE) > 0) { /* atan(t) = pi/2 - atan(1/t) */
        t = fp_div(F64_ONE, t);
        inv = 1;
    }
    if (fp_cmp(t, C_TAN_PI12) > 0) { /* atan(t) = pi/6 + atan((t sqrt3 - 1) / (t + sqrt3)) */
        t = fp_div(fp_sub(fp_mul(t, C_SQRT3), F64_ONE), fp_add(t, C_SQRT3));
        sixth = 1;
    }
    z = fp_mul(t, t);
    a = fp_add(t, fp_mul(fp_mul(t, z), rt_math__horner(rt_math__atn_c, 14, z)));
    if (sixth) a = fp_add(C_PIO6, a);
    if (inv) a = fp_sub(C_PIO2, a);
    return neg ? fp_neg(a) : a;
}

f64 rt_asin(f64 x) {
    const int c = fp_cmp(rt_math__abs(x), F64_ONE);
    if (c > 0) {
        rt_error(RT_ERR_ASIN);
        return F64_ZERO;
    }
    if (c == 0) return (x >> 63) ? fp_neg(C_PIO2) : C_PIO2;
    return rt_atn(fp_div(x, fp_sqrt(fp_mul(fp_sub(F64_ONE, x), fp_add(F64_ONE, x)))));
}

/* |x| <= 1/2: pi/2 - asin(x); otherwise the half angle, which avoids the cancellation near +-1:
 * acos(x) = 2 asin(sqrt((1-x)/2)), acos(-x) = pi - acos(x) */
f64 rt_acos(f64 x) {
    const f64 a = rt_math__abs(x);
    f64 h;
    if (fp_cmp(a, F64_ONE) > 0) {
        rt_error(RT_ERR_ASIN);
        return F64_ZERO;
    }
    if (fp_cmp(a, C_HALF) <= 0) return fp_sub(C_PIO2, rt_asin(x));
    h = fp_mul(C_TWO, rt_asin(fp_sqrt(fp_mul(fp_sub(F64_ONE, a), C_HALF))));
    return (x >> 63) ? fp_sub(C_PI, h) : h;
}

/* x^y for an exponent that is not an integer: EXP(y LOG(x)); 0^y = 0 (y > 0), division by zero
 * for y < 0; LOG raises error 6 for x < 0 (probe13 T8-T10) */
f64 rt_powf(f64 x, f64 y) {
    if (x == F64_ZERO) {
        if (y >> 63) rt_error(RT_ERR_DIV_ZERO);
        return F64_ZERO;
    }
    return rt_exp(fp_mul(y, rt_log(x)));
}

/* DEG(x) = x*180/PI, RAD(x) = x*PI/180, in this order (docs/numerics.md) */
f64 rt_deg(f64 x) {
    return fp_div(fp_mul(x, C_D180), C_PI);
}

f64 rt_rad(f64 x) {
    return fp_div(fp_mul(x, C_PI), C_D180);
}
