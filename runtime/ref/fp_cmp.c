//! module    fp_cmp
//! provides  fp_cmp
//! group     float
//! doc       Exact comparison of two values: -1, 0 or 1.

#include "fp_internal.h"

int fp_cmp(f64 a, f64 b) {
    const fp_parts x = fp_unpack(a), y = fp_unpack(b);
    int s;
    if (x.zero && y.zero) return 0;
    if (x.zero) return y.neg ? 1 : -1;
    if (y.zero) return x.neg ? -1 : 1;
    if (x.neg != y.neg) return x.neg ? -1 : 1;
    if (x.e != y.e) s = x.e < y.e ? -1 : 1;
    else if (x.m != y.m) s = x.m < y.m ? -1 : 1;
    else return 0;
    return x.neg ? -s : s;
}
