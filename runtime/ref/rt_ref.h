/* Public interface of the C reference runtime (docs/runtime.md, docs/numerics.md).
 *
 * Portable C99, integer arithmetic only: no float/double, no libm. A floating-point value is the
 * bit pattern of an IEEE 754 binary64 number in a uint64_t ("f64"). Valid values are +0 and the
 * normal numbers; there is no -0, no denormal, no infinity and no NaN (docs/numerics.md).
 *
 * Errors are reported through rt_error(), which the environment provides (generated program,
 * compiler constant folding, tests). In a running program rt_error() does not return; if it does
 * (tests), the routine returns +0.
 */
#ifndef RT_REF_H
#define RT_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t f64;

/* GFA-BASIC error numbers, as determined on the original (docs/language.md). */
enum {
    RT_ERR_DIV_ZERO = 0,  /* divide by zero */
    RT_ERR_OVERFLOW = 1,  /* overflow */
    RT_ERR_NO_INT = 2,    /* no integer|-2147483648 .. 2147483647 */
    RT_ERR_NO_BYTE = 3,   /* no byte|0 .. 255 */
    RT_ERR_NO_WORD = 4,   /* no word|-32768 .. 32767 */
    RT_ERR_SQR_NEG = 5,   /* no negative squareroot */
    RT_ERR_LOG = 6,       /* logarithm only for|numbers greater zero */
    RT_ERR_STR_LONG = 10, /* string too long|max. 32767 chars */
    RT_ERR_INDEX = 16,    /* array index out of range */
    RT_ERR_NOT_OPEN = 24  /* file not open */
};

void rt_error(int code);

/* ---- floating-point core (fp_*) ---------------------------------------------------------- */
#define F64_ZERO ((f64)0)
#define F64_ONE ((f64)0x3FF0000000000000u)

f64 fp_canon(f64 x);               /* value read from memory: denormal/-0 -> 0, Inf/NaN -> error 1 */
f64 fp_neg(f64 x);
f64 fp_add(f64 a, f64 b);
f64 fp_sub(f64 a, f64 b);
f64 fp_mul(f64 a, f64 b);
f64 fp_div(f64 a, f64 b);          /* b = 0 -> error 0 */
f64 fp_sqrt(f64 x);                /* x < 0 -> error 5 */
int fp_cmp(f64 a, f64 b);          /* -1, 0, 1 */
int fp_approx_eq(f64 a, f64 b);    /* GFA "==": 1 if equal at 28 significant bits */

f64 fp_from_i32(int32_t v);
int32_t fp_to_i32(f64 x);          /* truncates towards 0; out of range -> error 2 */
int16_t fp_to_i16(f64 x);          /* truncates towards 0; out of range -> error 4 */
uint8_t fp_to_u8(f64 x);           /* truncates towards 0; out of range -> error 3 */

f64 fp_trunc(f64 x);               /* TRUNC/FIX: towards 0 */
f64 fp_int(f64 x);                 /* INT: floor */
f64 fp_frac(f64 x);                /* FRAC: x - TRUNC(x) */
f64 fp_cint(f64 x);                /* CINT: nearest, ties away from 0 */
f64 fp_round(f64 x, int32_t n);    /* ROUND(x,n): INT(x*10^n+0.5)/10^n (ties upwards) */

/* Low-level: round m * 2^e (m != 0) to binary64, ties to even. On overflow sets *overflow and
 * returns 0; results below 2^-1022 become 0. */
f64 fp_round_pack(int neg, int32_t e, uint64_t m, int *overflow);
/* The same, but overflow raises error 1. */
f64 fp_finish(int neg, int32_t e, uint64_t m);

/* ---- number <-> text (rt_*) --------------------------------------------------------------- */
#define RT_FMT_FLT_MAX 32          /* buffer size for rt_fmt_flt, including the terminating 0 */

/* PRINT/STR$ form of x with `digits` significant digits (13, or the DEFNUM value; values
 * below 2 count as 2 and above 13 as 13, see probe6 T8).
 * Writes a 0-terminated string and returns its length. */
int rt_fmt_flt(char *buf, f64 x, int digits);

/* VAL: reads a number from s[0..len). *used (may be NULL) receives the number of characters
 * consumed (VAL?). Out of range -> 0 without error, as in the original. */
f64 rt_val(const char *s, int32_t len, int32_t *used);

/* ---- random numbers (xorshift32) ---------------------------------------------------------- */
void rt_randomize(uint32_t seed);  /* 0 is replaced by a fixed constant */
uint32_t rt_rnd_next(void);        /* advances the state and returns it */
f64 rt_rnd(void);                  /* RND: next state / 2^32 */
int32_t rt_rand(int32_t y);        /* RAND(y): (state >> 16) * (y AND &HFFFF) >> 16 */

#ifdef __cplusplus
}
#endif

#endif
