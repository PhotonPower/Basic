//! module    ix_bn
//! provides  ix_bn_set_u64 ix_bn_mul_add ix_bn_mul_pow10 ix_bn_shl ix_bn_bitlen ix_bn_cmp ix_bn_sub ix_bn_divq ix_bn_top64
//! group     float
//! doc       Unsigned big integers for the exact number <-> text conversions.

#include "ix_bn.h"

static void ix_bn__trim(ix_bn *a) {
    while (a->n > 0 && a->w[a->n - 1] == 0) --a->n;
}

void ix_bn_set_u64(ix_bn *a, uint64_t v) {
    a->w[0] = (uint32_t)v;
    a->w[1] = (uint32_t)(v >> 32);
    a->n = 2;
    ix_bn__trim(a);
}

void ix_bn_mul_add(ix_bn *a, uint32_t m, uint32_t add) {
    uint64_t carry = add;
    int i;
    for (i = 0; i < a->n; ++i) {
        const uint64_t t = (uint64_t)a->w[i] * m + carry;
        a->w[i] = (uint32_t)t;
        carry = t >> 32;
    }
    if (carry != 0 && a->n < IX_BN_LIMBS) a->w[a->n++] = (uint32_t)carry;
    ix_bn__trim(a);
}

void ix_bn_mul_pow10(ix_bn *a, int32_t k) {
    for (; k >= 9; k -= 9) ix_bn_mul_add(a, 1000000000u, 0);
    if (k > 0) {
        uint32_t p = 1;
        for (; k > 0; --k) p *= 10;
        ix_bn_mul_add(a, p, 0);
    }
}

void ix_bn_shl(ix_bn *a, int32_t bits) {
    const int limbs = (int)(bits / 32), sh = (int)(bits % 32);
    int i;
    if (a->n == 0 || bits == 0) return;
    if (a->n + limbs + 1 > IX_BN_LIMBS) return; /* cannot happen with the documented limits */
    a->w[a->n + limbs] = 0;
    for (i = a->n - 1; i >= 0; --i) {
        const uint32_t v = a->w[i];
        if (sh != 0) a->w[i + limbs + 1] |= v >> (32 - sh);
        a->w[i + limbs] = sh != 0 ? v << sh : v;
    }
    for (i = 0; i < limbs; ++i) a->w[i] = 0;
    a->n += limbs + 1;
    ix_bn__trim(a);
}

int32_t ix_bn_bitlen(const ix_bn *a) {
    uint32_t top;
    int32_t bits;
    if (a->n == 0) return 0;
    top = a->w[a->n - 1];
    bits = (int32_t)(a->n - 1) * 32;
    while (top != 0) {
        ++bits;
        top >>= 1;
    }
    return bits;
}

int ix_bn_cmp(const ix_bn *a, const ix_bn *b) {
    int i;
    if (a->n != b->n) return a->n < b->n ? -1 : 1;
    for (i = a->n - 1; i >= 0; --i)
        if (a->w[i] != b->w[i]) return a->w[i] < b->w[i] ? -1 : 1;
    return 0;
}

void ix_bn_sub(ix_bn *a, const ix_bn *b) {
    uint64_t borrow = 0;
    int i;
    for (i = 0; i < a->n; ++i) {
        const uint64_t bi = i < b->n ? b->w[i] : 0;
        const uint64_t t = (uint64_t)a->w[i] - bi - borrow;
        a->w[i] = (uint32_t)t;
        borrow = (t >> 63) & 1;
    }
    ix_bn__trim(a);
}

uint64_t ix_bn_divq(ix_bn *num, const ix_bn *den, int maxbit) {
    uint64_t q = 0;
    int bit;
    ix_bn t;
    for (bit = maxbit; bit >= 0; --bit) {
        t = *den;
        ix_bn_shl(&t, bit);
        if (ix_bn_cmp(num, &t) >= 0) {
            ix_bn_sub(num, &t);
            q |= (uint64_t)1 << bit;
        }
    }
    return q;
}

uint64_t ix_bn_top64(const ix_bn *a, int *sticky) {
    const int32_t len = ix_bn_bitlen(a);
    const int32_t low = len - 64; /* index of the lowest bit taken */
    uint64_t r = 0;
    int32_t i;
    *sticky = 0;
    for (i = 0; i < 64; ++i) {
        const int32_t b = low + i;
        if ((a->w[b / 32] >> (b % 32)) & 1u) r |= (uint64_t)1 << i;
    }
    for (i = 0; i < low; ++i)
        if ((a->w[i / 32] >> (i % 32)) & 1u) {
            *sticky = 1;
            break;
        }
    return r;
}
