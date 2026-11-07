//! module    fp_sqrt
//! provides  fp_sqrt
//! requires  fp_finish rt_error
//! group     float
//! doc       Square root, correctly rounded (ties to even); negative argument -> error 5.

#include "fp_internal.h"

f64 fp_sqrt(f64 a) {
    const fp_parts x = fp_unpack(a);
    uint64_t m, r = 0, hi, lo, rhi, rlo;
    int32_t e;
    int bit;
    if (x.zero) return F64_ZERO;
    if (x.neg) {
        rt_error(RT_ERR_SQR_NEG);
        return F64_ZERO;
    }
    m = x.m;
    e = x.e;
    if (e % 2 != 0) { /* make the exponent even */
        m <<= 1;
        --e;
    }
    /* radicand R = m * 2^72 (128 bit, 2^124 <= R < 2^126), root 2^62 <= r < 2^63 */
    rhi = m << 8;
    rlo = 0;
    for (bit = 62; bit >= 0; --bit) {
        const uint64_t t = r | ((uint64_t)1 << bit);
        fp_mul64(t, t, &hi, &lo);
        if (hi < rhi || (hi == rhi && lo <= rlo)) r = t;
    }
    fp_mul64(r, r, &hi, &lo);
    return fp_finish(0, (e - 72) / 2, r | ((hi != rhi || lo != rlo) ? 1u : 0u));
}
