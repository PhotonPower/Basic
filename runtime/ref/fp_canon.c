//! module    fp_canon
//! provides  fp_canon
//! requires  rt_error
//! group     float
//! doc       Normalises a binary64 read from memory: denormal and -0 -> 0, infinity/NaN -> error 1.

#include "fp_internal.h"

f64 fp_canon(f64 x) {
    const uint64_t be = (x >> 52) & 0x7FF;
    if (be == 0) return F64_ZERO;
    if (be == 0x7FF) {
        rt_error(RT_ERR_OVERFLOW);
        return F64_ZERO;
    }
    return x;
}
