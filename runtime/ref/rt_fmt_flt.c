//! module    rt_fmt_flt
//! provides  rt_fmt_flt
//! requires  rt_flt_dec
//! group     float
//! doc       Float -> text as PRINT/STR$ (13 significant digits or DEFNUM), from the exact binary value.

#include "fp_internal.h"

/* Rules determined on the original (docs/numerics.md, probe1/probe2):
 *  - the value is rounded to `digits` significant digits, ties away from 0;
 *  - without exponent if the decimal exponent d of the rounded value is -5 <= d <= 13,
 *    otherwise d.ddd E+dd with at least one decimal and at least two exponent digits;
 *  - trailing zeros of the fraction are dropped, "0.5" with a leading zero, no space before
 *    positive numbers. */
int rt_fmt_flt(char *buf, f64 a, int digits) {
    char ds[16];
    char *o = buf;
    int32_t d;
    int i, len;
    if (digits < 2) digits = 2; /* DEFNUM 1 prints 2 digits on the original (probe6 T8) */
    if (digits > 13) digits = 13;
    if (((a >> 52) & 0x7FF) == 0) {
        buf[0] = '0';
        buf[1] = 0;
        return 1;
    }
    if (rt_flt_dec(a, digits, ds, &d)) *o++ = '-';
    len = digits;
    while (len > 1 && ds[len - 1] == '0') --len;
    if (d >= -5 && d <= 13) {
        if (d >= 0) {
            for (i = 0; i <= d; ++i) *o++ = i < len ? ds[i] : '0';
            if (len > d + 1) {
                *o++ = '.';
                for (i = d + 1; i < len; ++i) *o++ = ds[i];
            }
        } else {
            *o++ = '0';
            *o++ = '.';
            for (i = -1; i > d; --i) *o++ = '0';
            for (i = 0; i < len; ++i) *o++ = ds[i];
        }
    } else {
        int32_t ex = d < 0 ? -d : d;
        char es[8];
        int n = 0;
        *o++ = ds[0];
        *o++ = '.';
        if (len == 1) *o++ = '0';
        for (i = 1; i < len; ++i) *o++ = ds[i];
        *o++ = 'E';
        *o++ = d < 0 ? '-' : '+';
        do {
            es[n++] = (char)('0' + ex % 10);
            ex /= 10;
        } while (ex != 0);
        if (n < 2) es[n++] = '0';
        while (n > 0) *o++ = es[--n];
    }
    *o = 0;
    return (int)(o - buf);
}
