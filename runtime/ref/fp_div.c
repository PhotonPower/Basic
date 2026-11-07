//! module    fp_div
//! provides  fp_div
//! requires  fp_finish rt_error
//! group     float
//! doc       Division, correctly rounded (ties to even); division by 0 -> error 0.

#include "fp_internal.h"

f64 fp_div(f64 a, f64 b) {
    const fp_parts x = fp_unpack(a), y = fp_unpack(b);
    uint64_t q = 0, r;
    int i;
    if (y.zero) {
        rt_error(RT_ERR_DIV_ZERO);
        return F64_ZERO;
    }
    if (x.zero) return F64_ZERO;
    /* q = floor(x.m / y.m * 2^63), restoring division; r < 2 * y.m < 2^54 throughout */
    r = x.m;
    for (i = 0; i < 64; ++i) {
        q <<= 1;
        if (r >= y.m) {
            r -= y.m;
            q |= 1;
        }
        r <<= 1;
    }
    return fp_finish(x.neg != y.neg, x.e - y.e - 63, q | (r != 0 ? 1u : 0u));
}
