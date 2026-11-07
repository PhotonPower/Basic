//! module    rt_rnd
//! provides  rt_randomize rt_rnd_next rt_rnd rt_rand
//! requires  fp_round_pack
//! group     float
//! doc       Random numbers: xorshift32 (Marsaglia 13, 17, 5), the same on all targets.

#include "fp_internal.h"

#define RT_RND_DEFAULT 0x2545F491u

static uint32_t rt_rnd__state = RT_RND_DEFAULT;

void rt_randomize(uint32_t seed) {
    rt_rnd__state = seed != 0 ? seed : RT_RND_DEFAULT;
}

uint32_t rt_rnd_next(void) {
    uint32_t x = rt_rnd__state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rt_rnd__state = x;
    return x;
}

f64 rt_rnd(void) {
    int overflow;
    return fp_round_pack(0, -32, rt_rnd_next(), &overflow); /* exact, 0 <= r < 1 */
}

int32_t rt_rand(int32_t y) {
    const uint32_t s = rt_rnd_next();
    return (int32_t)(((s >> 16) * ((uint32_t)y & 0xFFFFu)) >> 16);
}
