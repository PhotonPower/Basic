//! module    fp_round_int
//! provides  fp_trunc fp_int fp_frac fp_cint
//! requires  fp_round_pack fp_add
//! group     float
//! doc       TRUNC/FIX (towards 0), INT (floor), FRAC (x - TRUNC x), CINT (nearest, ties away from 0).

#include "fp_internal.h"

f64 fp_trunc(f64 a) {
    const fp_parts x = fp_unpack(a);
    int overflow;
    if (x.zero || x.e >= 0) return a;
    if (x.e <= -53) return F64_ZERO;
    return fp_round_pack(x.neg, x.e, x.m & ~(((uint64_t)1 << -x.e) - 1), &overflow);
}

f64 fp_int(f64 a) {
    const f64 t = fp_trunc(a);
    if ((a >> 63) != 0 && t != a) return fp_sub(t, F64_ONE);
    return t;
}

f64 fp_frac(f64 a) {
    return fp_sub(a, fp_trunc(a));
}

f64 fp_cint(f64 a) {
    const fp_parts x = fp_unpack(a);
    int overflow;
    int32_t sh;
    if (x.zero || x.e >= 0) return a;
    if (x.e < -53) return F64_ZERO; /* |x| < 0.5 */
    sh = -x.e;
    return fp_round_pack(x.neg, x.e, (x.m + ((uint64_t)1 << (sh - 1))) & ~(((uint64_t)1 << sh) - 1),
                         &overflow);
}
