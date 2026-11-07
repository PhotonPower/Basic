//! module    fp_approx_eq
//! provides  fp_approx_eq
//! group     float
//! doc       GFA "==": both values rounded to 28 significant bits (ties away from 0), then compared.

#include "fp_internal.h"

/* Rounds |x| to 28 significant bits, ties away from zero (verified on the original, probe2/probe3). */
static void fp_approx_eq__key(fp_parts *p) {
    uint64_t m = (p->m + ((uint64_t)1 << 24)) >> 25;
    if (m == ((uint64_t)1 << 28)) {
        m >>= 1;
        ++p->e;
    }
    p->m = m;
}

int fp_approx_eq(f64 a, f64 b) {
    fp_parts x = fp_unpack(a), y = fp_unpack(b);
    if (x.zero || y.zero) return x.zero && y.zero; /* 0 only equals 0 */
    if (x.neg != y.neg) return 0;
    fp_approx_eq__key(&x);
    fp_approx_eq__key(&y);
    return x.e == y.e && x.m == y.m;
}
