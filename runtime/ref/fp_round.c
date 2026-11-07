//! module    fp_round
//! provides  fp_round
//! requires  fp_add fp_mul fp_div fp_cmp fp_round_int
//! group     float
//! doc       ROUND(x,n) = INT(x*10^n+0.5)/10^n (ties upwards, as on the original, probe6 T11).

#include "fp_internal.h"

#define FP_ROUND__HALF ((f64)0x3FE0000000000000u)
#define FP_ROUND__TEN ((f64)0x4024000000000000u)
#define FP_ROUND__2P52 ((f64)0x4330000000000000u)

/* For |x * 10^n| >= 2^52 the value has no digits left to round: x is returned unchanged (this
 * also avoids an overflow of x * 10^n). n < 0 rounds to tens, hundreds, ... */
f64 fp_round(f64 x, int32_t n) {
    const int32_t k = n < 0 ? -n : n;
    f64 p = F64_ONE, y;
    int32_t i;
    if (((x >> 52) & 0x7FF) == 0) return F64_ZERO;
    if (k > 308) return n > 0 ? x : F64_ZERO;
    for (i = 0; i < k; ++i) p = fp_mul(p, FP_ROUND__TEN); /* exact up to 10^22 */
    if (n >= 0) {
        if (fp_cmp(x & ~FP_SIGN_BIT, fp_div(FP_ROUND__2P52, p)) >= 0) return x;
        y = fp_mul(x, p);
    } else {
        y = fp_div(x, p);
        if (fp_cmp(y & ~FP_SIGN_BIT, FP_ROUND__2P52) >= 0) return x;
    }
    y = fp_int(fp_add(y, FP_ROUND__HALF));
    return n >= 0 ? fp_div(y, p) : fp_mul(y, p);
}
