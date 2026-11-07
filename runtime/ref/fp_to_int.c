//! module    fp_to_int
//! provides  fp_to_i32 fp_to_i16 fp_to_u8
//! requires  rt_error
//! group     core
//! doc       Assignment of a float to %, & or |: truncation towards 0, range error 2, 4 or 3.

#include "fp_internal.h"

/* Truncated value if |x| < 2^62, otherwise 0 with *ok = 0. */
static int64_t fp_to_int__trunc(f64 a, int *ok) {
    const fp_parts x = fp_unpack(a);
    uint64_t mag;
    *ok = 1;
    if (x.zero || x.e <= -53) return 0;
    if (x.e >= 0) {
        if (x.e > 9) {
            *ok = 0;
            return 0;
        }
        mag = x.m << x.e;
    } else {
        mag = x.m >> -x.e;
    }
    return x.neg ? -(int64_t)mag : (int64_t)mag;
}

int32_t fp_to_i32(f64 x) {
    int ok;
    const int64_t v = fp_to_int__trunc(x, &ok);
    if (!ok || v < INT32_MIN || v > INT32_MAX) {
        rt_error(RT_ERR_NO_INT);
        return 0;
    }
    return (int32_t)v;
}

int16_t fp_to_i16(f64 x) {
    int ok;
    const int64_t v = fp_to_int__trunc(x, &ok);
    if (!ok || v < INT16_MIN || v > INT16_MAX) {
        rt_error(RT_ERR_NO_WORD);
        return 0;
    }
    return (int16_t)v;
}

uint8_t fp_to_u8(f64 x) {
    int ok;
    const int64_t v = fp_to_int__trunc(x, &ok);
    if (!ok || v < 0 || v > 255) {
        rt_error(RT_ERR_NO_BYTE);
        return 0;
    }
    return (uint8_t)v;
}
