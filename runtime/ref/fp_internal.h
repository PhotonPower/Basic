/* Helpers shared by the fp_* modules (not part of the public interface). */
#ifndef FP_INTERNAL_H
#define FP_INTERNAL_H

#include "rt_ref.h"

#define FP_SIGN_BIT ((uint64_t)1 << 63)
#define FP_HIDDEN ((uint64_t)1 << 52)
#define FP_FRAC_MASK (FP_HIDDEN - 1)

/* Unpacked value: zero, or (-1)^neg * m * 2^e with 2^52 <= m < 2^53. */
typedef struct {
    int neg;
    int zero;
    int32_t e;
    uint64_t m;
} fp_parts;

static inline fp_parts fp_unpack(f64 x) {
    fp_parts p;
    const int32_t be = (int32_t)((x >> 52) & 0x7FF);
    p.zero = be == 0;
    p.neg = p.zero ? 0 : (int)(x >> 63);
    p.m = p.zero ? 0 : (x & FP_FRAC_MASK) | FP_HIDDEN;
    p.e = p.zero ? 0 : be - 1075;
    return p;
}

/* 64 x 64 -> 128 bit product. */
static inline void fp_mul64(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo) {
    const uint64_t a0 = a & 0xFFFFFFFFu, a1 = a >> 32;
    const uint64_t b0 = b & 0xFFFFFFFFu, b1 = b >> 32;
    const uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    const uint64_t mid = (p00 >> 32) + (p01 & 0xFFFFFFFFu) + (p10 & 0xFFFFFFFFu);
    *lo = (mid << 32) | (p00 & 0xFFFFFFFFu);
    *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

#endif
