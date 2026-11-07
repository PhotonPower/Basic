//! module    fp_round_pack
//! provides  fp_round_pack fp_finish
//! requires  rt_error
//! group     float
//! doc       Rounds m * 2^e to binary64 (ties to even); overflow -> error 1, below 2^-1022 -> 0.

#include "fp_internal.h"

f64 fp_round_pack(int neg, int32_t e, uint64_t m, int *overflow) {
    uint64_t rem;
    int32_t be;
    *overflow = 0;
    if (m == 0) return F64_ZERO;
    while ((m >> 63) == 0) { /* normalise: leading bit to bit 63 (exact) */
        m <<= 1;
        --e;
    }
    /* keep the top 53 bits, round on the remaining 11 */
    rem = m & 0x7FF;
    m >>= 11;
    e += 11;
    if (rem > 0x400 || (rem == 0x400 && (m & 1))) {
        ++m;
        if (m == ((uint64_t)1 << 53)) {
            m >>= 1;
            ++e;
        }
    }
    be = e + 1075; /* value = m * 2^e with 2^52 <= m < 2^53 */
    if (be >= 2047) {
        *overflow = 1;
        return F64_ZERO;
    }
    if (be <= 0) return F64_ZERO; /* no denormals */
    return ((uint64_t)(neg ? 1 : 0) << 63) | ((uint64_t)be << 52) | (m & FP_FRAC_MASK);
}

f64 fp_finish(int neg, int32_t e, uint64_t m) {
    int overflow;
    const f64 r = fp_round_pack(neg, e, m, &overflow);
    if (overflow) rt_error(RT_ERR_OVERFLOW);
    return r;
}
