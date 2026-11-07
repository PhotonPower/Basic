//! module    fp_mul
//! provides  fp_mul
//! requires  fp_finish
//! group     float
//! doc       Multiplication, correctly rounded (ties to even).

#include "fp_internal.h"

f64 fp_mul(f64 a, f64 b) {
    const fp_parts x = fp_unpack(a), y = fp_unpack(b);
    uint64_t hi, lo;
    if (x.zero || y.zero) return F64_ZERO;
    /* both significands with the leading bit at 63: product leading bit at 126 or 127 */
    fp_mul64(x.m << 11, y.m << 11, &hi, &lo);
    return fp_finish(x.neg != y.neg, x.e + y.e - 22 + 64, hi | (lo != 0 ? 1u : 0u));
}
