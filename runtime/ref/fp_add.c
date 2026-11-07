//! module    fp_add
//! provides  fp_add fp_sub fp_neg
//! requires  fp_finish
//! group     float
//! doc       Addition and subtraction, correctly rounded (ties to even); negation.

#include "fp_internal.h"

f64 fp_neg(f64 x) {
    return ((x >> 52) & 0x7FF) == 0 ? F64_ZERO : x ^ FP_SIGN_BIT;
}

f64 fp_add(f64 a, f64 b) {
    fp_parts x = fp_unpack(a), y = fp_unpack(b);
    uint64_t mx, my, m;
    int32_t d;
    if (y.zero) return x.zero ? F64_ZERO : a;
    if (x.zero) return b;
    if (y.e > x.e || (y.e == x.e && y.m > x.m)) { /* |x| >= |y| */
        const fp_parts t = x;
        x = y;
        y = t;
    }
    /* significands with the leading bit at 62: one bit headroom, 10 guard bits */
    mx = x.m << 10;
    my = y.m << 10;
    d = x.e - y.e;
    if (d >= 63) {
        my = 1; /* only the sticky bit is left */
    } else if (d > 0) {
        const uint64_t lost = my & (((uint64_t)1 << d) - 1);
        my = (my >> d) | (lost != 0 ? 1u : 0u);
    }
    if (x.neg == y.neg) {
        m = mx + my;
    } else {
        m = mx - my;
        if (m == 0) return F64_ZERO; /* x - x is +0 */
    }
    return fp_finish(x.neg, x.e - 10, m);
}

f64 fp_sub(f64 a, f64 b) {
    return fp_add(a, fp_neg(b));
}
