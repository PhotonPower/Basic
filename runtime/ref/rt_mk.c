//! module    rt_mk
//! provides  rt_mkd rt_cvd rt_mks rt_cvs
//! requires  fp_round_pack fp_finish
//! group     string
//! doc       MKD$/CVD (GFA 8-byte float format), MKS$/CVS (IEEE single, D11).

/* GFA format (probe1 T42-T45, docs/numerics.md): 6 bytes mantissa with the leading 1 (0.5 <= m < 1),
 * then 2 bytes exponent with bias $3FE, both big-endian; negative numbers have the exponent word
 * negated. 0 is eight zero bytes. A binary64 value is rounded to the 48 bits of the mantissa
 * (nearest, ties to even). */

#include "fp_internal.h"

void rt_mkd(f64 x, unsigned char out[8]) {
    const fp_parts p = fp_unpack(x);
    uint64_t m;
    int32_t e;
    int k;
    if (p.zero) {
        for (k = 0; k < 8; ++k) out[k] = 0;
        return;
    }
    /* x = m * 2^e with 2^52 <= m < 2^53: 48 bits are m >> 5, rounded */
    m = p.m >> 5;
    {
        const uint64_t rest = p.m & 31;
        if (rest > 16 || (rest == 16 && (m & 1))) ++m;
    }
    e = p.e + 5;
    if (m >> 48) { /* carried into a new bit */
        m >>= 1;
        ++e;
    }
    /* value = m * 2^e = (m / 2^48) * 2^(e + 48); exponent word = e + 48 + $3FE */
    {
        int32_t w = e + 48 + 0x3FE;
        if (p.neg) w = -w;
        for (k = 0; k < 6; ++k) out[k] = (unsigned char)(m >> (40 - 8 * k));
        out[6] = (unsigned char)((uint32_t)w >> 8);
        out[7] = (unsigned char)w;
    }
}

/* Out of the range of binary64: error 1 (overflow) or 0 (underflow), as for arithmetic. */
f64 rt_cvd(const unsigned char in[8]) {
    uint64_t m = 0;
    int32_t w;
    int k, neg = 0;
    for (k = 0; k < 6; ++k) m = (m << 8) | in[k];
    w = (int32_t)(int16_t)(uint16_t)((in[6] << 8) | in[7]);
    if (m == 0) return F64_ZERO;
    if (w < 0) {
        neg = 1;
        w = -w;
    }
    return fp_finish(neg, w - 0x3FE - 48, m);
}

/* IEEE single, big-endian; overflow error 1, results below the smallest normal single 0 */
void rt_mks(f64 x, unsigned char out[4]) {
    const fp_parts p = fp_unpack(x);
    uint32_t bits = 0;
    int k;
    if (!p.zero) {
        uint64_t m = p.m >> 29; /* 24 bits, rounded to nearest even */
        const uint64_t rest = p.m & ((1u << 29) - 1);
        int32_t e;
        if (rest > (1u << 28) || (rest == (1u << 28) && (m & 1))) ++m;
        e = p.e + 29;
        if (m >> 24) {
            m >>= 1;
            ++e;
        }
        /* value = m * 2^e, single: (1.f) * 2^(be - 127) with m = 1.f * 2^23 */
        {
            const int32_t be = e + 23 + 127;
            if (be >= 255) {
                rt_error(RT_ERR_OVERFLOW);
                bits = 0;
            } else if (be >= 1) {
                bits = ((uint32_t)p.neg << 31) | ((uint32_t)be << 23) | ((uint32_t)m & 0x7FFFFFu);
            }
        }
    }
    for (k = 0; k < 4; ++k) out[k] = (unsigned char)(bits >> (24 - 8 * k));
}

/* infinity and NaN: error 1; denormals and -0: 0 */
f64 rt_cvs(const unsigned char in[4]) {
    const uint32_t bits = ((uint32_t)in[0] << 24) | ((uint32_t)in[1] << 16) | ((uint32_t)in[2] << 8) | in[3];
    const int32_t be = (int32_t)((bits >> 23) & 0xFF);
    if (be == 255) {
        rt_error(RT_ERR_OVERFLOW);
        return F64_ZERO;
    }
    if (be == 0) return F64_ZERO;
    return ((f64)(bits >> 31) << 63) | ((f64)(be - 127 + 1023) << 52) | ((f64)(bits & 0x7FFFFFu) << 29);
}
