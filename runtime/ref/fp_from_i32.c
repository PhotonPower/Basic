//! module    fp_from_i32
//! provides  fp_from_i32
//! requires  fp_round_pack
//! group     float
//! doc       Exact conversion of a 32-bit integer.

#include "fp_internal.h"

f64 fp_from_i32(int32_t v) {
    int overflow;
    const uint64_t mag = v < 0 ? (uint64_t)(-(int64_t)v) : (uint64_t)v;
    return fp_round_pack(v < 0, 0, mag, &overflow);
}
