//! module    rt_str_fmt
//! provides  rt_str_n rt_str_fmt
//! requires  rt_fmt_flt rt_using
//! group     float
//! doc       STR$(x,n) and STR$(x,y,z).

#include "fp_internal.h"

/* STR$(x,n): left-padded to n, or the first n characters (probe4 T33, probe5 T19). */
int32_t rt_str_n(char *out, int32_t cap, f64 x, int32_t n) {
    char buf[RT_FMT_FLT_MAX];
    const int32_t len = rt_fmt_flt(buf, x, 13);
    int32_t i, w = 0;
    if (n <= 0) return 0;
    for (i = 0; i < n && w < cap; ++i) out[w++] = i < n - len ? ' ' : buf[i - (n > len ? n - len : 0)];
    return w;
}

/* STR$(x,y,z) = PRINT USING with y positions, z of them after the point (probe4 T33/T34,
 * probe5 T20): format "###.##" for y = 6, z = 2. */
int32_t rt_str_fmt(char *out, int32_t cap, f64 x, int32_t y, int32_t z, int mode) {
    char f[260];
    int32_t n = 0, i;
    rt_using u;
    if (y <= 0) return 0;
    if (y > 255) y = 255;
    if (z < 0) z = 0;
    if (z > 0) {
        for (i = 0; i < y - z - 1; ++i) f[n++] = '#';
        f[n++] = mode >= 2 ? ',' : '.';
        for (i = 0; i < z && n < 258; ++i) f[n++] = '#';
    } else {
        for (i = 0; i < y; ++i) f[n++] = '#';
    }
    rt_using_begin(&u, f, n, mode);
    return rt_using_num(&u, out, cap, x);
}
