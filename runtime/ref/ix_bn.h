/* Small unsigned big integers for exact decimal <-> binary conversion (rt_fmt_flt, rt_val). */
#ifndef IX_BN_H
#define IX_BN_H

#include <stdint.h>

/* 160 limbs = 5120 bits: enough for VAL with 800 significant digits (see rt_val.c). */
#define IX_BN_LIMBS 160

typedef struct {
    int n;                      /* used limbs; w[n-1] != 0 unless n == 0 */
    uint32_t w[IX_BN_LIMBS];    /* little-endian */
} ix_bn;

void ix_bn_set_u64(ix_bn *a, uint64_t v);
void ix_bn_mul_add(ix_bn *a, uint32_t m, uint32_t add);   /* a = a * m + add */
void ix_bn_mul_pow10(ix_bn *a, int32_t k);                /* a *= 10^k, k >= 0 */
void ix_bn_shl(ix_bn *a, int32_t bits);
int32_t ix_bn_bitlen(const ix_bn *a);
int ix_bn_cmp(const ix_bn *a, const ix_bn *b);
void ix_bn_sub(ix_bn *a, const ix_bn *b);                 /* a -= b, requires a >= b */
/* Quotient of num / den that must be < 2^(maxbit+1); num becomes the remainder. */
uint64_t ix_bn_divq(ix_bn *num, const ix_bn *den, int maxbit);
/* Top 64 bits of a (bit length >= 64); *sticky = 1 if any lower bit is set. */
uint64_t ix_bn_top64(const ix_bn *a, int *sticky);

#endif
