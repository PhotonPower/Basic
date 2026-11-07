//! module    rt_using
//! provides  rt_using_begin rt_using_num rt_using_str rt_using_end
//! requires  rt_flt_dec rt_fmt_flt
//! group     print
//! doc       PRINT USING: number fields (# * $ , . ^^^^ + -), string fields (& ! \..\), _ escape.

#include "fp_internal.h"

/* Limits: digits before and after the point are capped so that the work buffers are bounded. */
#define RT_USING__MAXDEC 300
#define RT_USING__NBUF 720

typedef struct {
    char *p;
    int32_t n, cap;
} rt_using__out;

static void rt_using__put(rt_using__out *o, char c) {
    if (o->n < o->cap) o->p[o->n++] = c;
}

typedef struct {
    int kind;            /* 1 number, 2 string '&', 3 string '!', 4 string '\..\' */
    int32_t width;       /* kind 4: number of characters */
    int lead_sign;       /* 0, '+' or '-': fixed sign position in front */
    int trail_sign;      /* 0, '+' or '-': fixed sign position at the end */
    int dollar, star, thousands, point;
    int32_t w_int;       /* positions before the point (digits, separators, '$') */
    int32_t n_digits;    /* digit positions before the point */
    int32_t dec;         /* digits after the point */
    int32_t carets;      /* length of the ^ run (exponent format), 0 if none */
    int32_t end;
} rt_using__field;

static char rt_using__dpt(int mode) {
    return mode >= 2 ? ',' : '.';
}

static char rt_using__thou(int mode) {
    return mode >= 2 ? '.' : ',';
}

static int rt_using__at(const rt_using *u, int32_t i, char c) {
    return i < u->len && u->f[i] == c;
}

/* Does a field start at position i? Returns its kind or 0. */
static int rt_using__start(const rt_using *u, int32_t i) {
    const char c = u->f[i], dpt = rt_using__dpt(u->mode);
    int32_t j;
    if (c == '#') return 1;
    if (c == dpt) return rt_using__at(u, i + 1, '#');
    if (c == '+' || c == '-') {
        return rt_using__at(u, i + 1, '#') || rt_using__at(u, i + 1, '*') || rt_using__at(u, i + 1, '$') ||
               (rt_using__at(u, i + 1, dpt) && rt_using__at(u, i + 2, '#'));
    }
    if (c == '*') {
        for (j = i; rt_using__at(u, j, '*'); ++j) {
        }
        return rt_using__at(u, j, '#') || rt_using__at(u, j, '$') || (rt_using__at(u, j, dpt) && rt_using__at(u, j + 1, '#'));
    }
    if (c == '$') return rt_using__at(u, i + 1, '#') || rt_using__at(u, i + 1, '*');
    if (c == '&') return 2;
    if (c == '!') return 3;
    if (c == '\\') {
        for (j = i + 1; rt_using__at(u, j, ' '); ++j) {
        }
        return rt_using__at(u, j, '\\') ? 4 : 0;
    }
    return 0;
}

static rt_using__field rt_using__parse(const rt_using *u, int32_t i) {
    rt_using__field fl;
    const char dpt = rt_using__dpt(u->mode), thou = rt_using__thou(u->mode);
    int32_t j = i, k;
    fl.kind = rt_using__start(u, i);
    fl.width = 0;
    fl.lead_sign = fl.trail_sign = 0;
    fl.dollar = fl.star = fl.thousands = fl.point = 0;
    fl.w_int = fl.n_digits = fl.dec = fl.carets = 0;
    if (fl.kind == 2 || fl.kind == 3) {
        fl.end = i + 1;
        return fl;
    }
    if (fl.kind == 4) {
        for (j = i + 1; u->f[j] != '\\'; ++j) {
        }
        fl.width = j - i + 1;
        fl.end = j + 1;
        return fl;
    }
    if (u->f[j] == '+' || u->f[j] == '-') fl.lead_sign = u->f[j++];
    while (j < u->len) {
        const char c = u->f[j];
        if (c == '#' || c == '*') {
            if (c == '*') fl.star = 1;
            ++fl.n_digits;
        } else if (c == '$' && !fl.dollar) {
            fl.dollar = 1;
        } else if (c == thou && fl.n_digits > 0 && rt_using__at(u, j + 1, '#')) {
            fl.thousands = 1;
        } else {
            break;
        }
        ++fl.w_int;
        ++j;
    }
    if (rt_using__at(u, j, dpt)) {
        fl.point = 1;
        for (++j; rt_using__at(u, j, '#'); ++j) ++fl.dec;
    }
    for (k = 0; rt_using__at(u, j + k, '^'); ++k) {
    }
    if (k >= 3) {
        fl.carets = k;
        j += k;
    }
    if (!fl.lead_sign && (rt_using__at(u, j, '-') || rt_using__at(u, j, '+'))) fl.trail_sign = u->f[j++];
    fl.end = j;
    return fl;
}

/* Writes literal text from u->pos up to the next field; with `wrap` the format starts over once
 * at its end. Returns 1 if a field starts at u->pos. */
static int rt_using__literal(rt_using *u, rt_using__out *o, int wrap) {
    for (;;) {
        while (u->pos < u->len) {
            const char c = u->f[u->pos];
            if (c == '_') {
                rt_using__put(o, u->pos + 1 < u->len ? u->f[u->pos + 1] : '_');
                u->pos += 2;
                continue;
            }
            if (rt_using__start(u, u->pos)) return 1;
            rt_using__put(o, c);
            ++u->pos;
        }
        if (u->pos > u->len) u->pos = u->len;
        if (!wrap || !u->has_field) return 0;
        u->pos = 0;
        wrap = 0;
    }
}

/* N = round(|x| * 10^dec) from the 13 decimal digits ds (ds[0] has weight 10^dd), ties away
 * from 0. Writes the digits of N to n (at least one) and returns their count. */
static int32_t rt_using__scaled(const char *ds, int32_t dd, int32_t dec, char *n) {
    const int32_t idx = dd + dec; /* index in ds of the units digit of N */
    int32_t len = 0, i;
    if (idx < -1) {
        n[0] = '0';
        return 1;
    }
    if (idx == -1) {
        n[0] = ds[0] >= '5' ? '1' : '0';
        return 1;
    }
    if (idx >= RT_USING__NBUF - 2) { /* cannot happen with the limits above */
        n[0] = '0';
        return 1;
    }
    for (i = 0; i <= idx; ++i) n[len++] = i < 13 ? ds[i] : '0';
    if (idx + 1 < 13 && ds[idx + 1] >= '5') {
        for (i = len - 1; i >= 0; --i) {
            if (n[i] != '9') {
                ++n[i];
                break;
            }
            n[i] = '0';
        }
        if (i < 0) {
            for (i = len; i > 0; --i) n[i] = n[i - 1];
            n[0] = '1';
            ++len;
        }
    }
    return len;
}

/* Number of digits before the point in n (without leading zeros, at least 1). */
static int32_t rt_using__intdigits(const char *n, int32_t nlen, int32_t dec) {
    int32_t first = 0;
    const int32_t units = nlen - dec - 1;
    if (units < 0) return 1;
    while (first < units && n[first] == '0') ++first;
    return units - first + 1;
}

static void rt_using__number(const rt_using *u, rt_using__out *o, const rt_using__field *fl, f64 x) {
    char ds[13], n[RT_USING__NBUF], ip[RT_USING__NBUF + RT_USING__NBUF / 3 + 4];
    int32_t d, e = 0, nlen, ilen, i, first, pad;
    const int32_t dec = fl->dec > RT_USING__MAXDEC ? RT_USING__MAXDEC : fl->dec;
    int neg, nonzero = 0;
    neg = rt_flt_dec(x, 13, ds, &d);
    if (fl->carets) {
        /* exponent format: the exponent is a multiple of the digit positions before the point
         * (handbook: "###.##^^^^" gives 16.00E+00, 4.10E+03) */
        const int32_t k = fl->n_digits;
        if (ds[0] == '0') e = 0;
        else if (k > 0) e = (d >= 0 ? d / k : -((-d + k - 1) / k)) * k;
        else e = d + 1;
        nlen = rt_using__scaled(ds, d - e, dec, n);
        if (ds[0] != '0' && rt_using__intdigits(n, nlen, dec) > (k > 0 ? k : 0)) {
            e += k > 0 ? k : 1; /* rounding carried into a new digit: renormalise once */
            nlen = rt_using__scaled(ds, d - e, dec, n);
        }
    } else {
        nlen = rt_using__scaled(ds, d, dec, n);
    }
    /* left-pad so that there is at least one digit before the point */
    if (nlen < dec + 1) {
        const int32_t sh = dec + 1 - nlen;
        for (i = nlen - 1; i >= 0; --i) n[i + sh] = n[i];
        for (i = 0; i < sh; ++i) n[i] = '0';
        nlen = dec + 1;
    }
    for (i = 0; i < nlen; ++i)
        if (n[i] != '0') nonzero = 1;
    /* integer part, right to left: digits with separators, then '$' and sign */
    first = nlen - dec - rt_using__intdigits(n, nlen, dec);
    ilen = 0;
    for (i = nlen - dec - 1; i >= first; --i) {
        const int32_t pos = nlen - dec - 1 - i;
        if (fl->thousands && pos > 0 && pos % 3 == 0) ip[ilen++] = rt_using__thou(u->mode);
        ip[ilen++] = n[i];
    }
    if (fl->n_digits == 0 && ilen == 1 && ip[0] == '0') ilen = 0; /* ".##": no units digit */
    if (fl->dollar) ip[ilen++] = '$';
    if (neg && nonzero && !fl->lead_sign && !fl->trail_sign) ip[ilen++] = '-';
    /* fixed sign in front */
    if (fl->lead_sign) rt_using__put(o, neg && nonzero ? '-' : (fl->lead_sign == '+' ? '+' : ' '));
    /* integer field: right-aligned, leading characters dropped if too long */
    pad = fl->w_int - ilen;
    for (i = 0; i < pad; ++i) rt_using__put(o, fl->star ? '*' : ' ');
    for (i = (pad < 0 ? ilen + pad : ilen) - 1; i >= 0; --i) rt_using__put(o, ip[i]);
    if (fl->point) {
        rt_using__put(o, rt_using__dpt(u->mode));
        for (i = nlen - dec; i < nlen; ++i) rt_using__put(o, n[i]);
    }
    if (fl->carets) {
        char es[12];
        int32_t ex = e < 0 ? -e : e, ne = 0, want = fl->carets - 2;
        rt_using__put(o, 'E');
        rt_using__put(o, e < 0 ? '-' : '+');
        do {
            es[ne++] = (char)('0' + ex % 10);
            ex /= 10;
        } while (ex != 0);
        for (; ne < want && ne < 10; ++ne) es[ne] = '0';
        while (ne > 0) rt_using__put(o, es[--ne]);
    }
    if (fl->trail_sign) rt_using__put(o, neg && nonzero ? '-' : (fl->trail_sign == '+' ? '+' : ' '));
}

static void rt_using__string(rt_using__out *o, const rt_using__field *fl, const char *s, int32_t slen) {
    int32_t i;
    if (fl->kind == 2) {
        for (i = 0; i < slen; ++i) rt_using__put(o, s[i]);
    } else if (fl->kind == 3) {
        if (slen > 0) rt_using__put(o, s[0]);
    } else {
        for (i = 0; i < fl->width; ++i) rt_using__put(o, i < slen ? s[i] : ' ');
    }
}

void rt_using_begin(rt_using *u, const char *f, int32_t len, int mode) {
    int32_t i;
    u->f = f;
    u->len = len;
    u->pos = 0;
    u->mode = mode;
    u->has_field = 0;
    for (i = 0; i < len; ++i) {
        if (f[i] == '_') {
            ++i;
            continue;
        }
        if (rt_using__start(u, i)) {
            u->has_field = 1;
            break;
        }
    }
}

int32_t rt_using_num(rt_using *u, char *out, int32_t cap, f64 x) {
    rt_using__out o;
    rt_using__field fl;
    o.p = out;
    o.n = 0;
    o.cap = cap;
    if (!rt_using__literal(u, &o, 1)) { /* no field in the format: the value as by PRINT */
        char buf[RT_FMT_FLT_MAX];
        const int n = rt_fmt_flt(buf, x, 13);
        int i;
        for (i = 0; i < n; ++i) rt_using__put(&o, buf[i]);
        return o.n;
    }
    fl = rt_using__parse(u, u->pos);
    u->pos = fl.end;
    if (fl.kind == 1) {
        rt_using__number(u, &o, &fl, x);
    } else { /* string field: the value as by PRINT */
        char buf[RT_FMT_FLT_MAX];
        const int n = rt_fmt_flt(buf, x, 13);
        rt_using__string(&o, &fl, buf, n);
    }
    return o.n;
}

int32_t rt_using_str(rt_using *u, char *out, int32_t cap, const char *s, int32_t slen) {
    rt_using__out o;
    rt_using__field fl;
    int32_t i;
    o.p = out;
    o.n = 0;
    o.cap = cap;
    if (!rt_using__literal(u, &o, 1)) {
        for (i = 0; i < slen; ++i) rt_using__put(&o, s[i]);
        return o.n;
    }
    fl = rt_using__parse(u, u->pos);
    u->pos = fl.end;
    if (fl.kind == 1) {
        for (i = 0; i < slen; ++i) rt_using__put(&o, s[i]); /* number field: the string unchanged */
    } else {
        rt_using__string(&o, &fl, s, slen);
    }
    return o.n;
}

int32_t rt_using_end(rt_using *u, char *out, int32_t cap) {
    rt_using__out o;
    o.p = out;
    o.n = 0;
    o.cap = cap;
    rt_using__literal(u, &o, 0);
    return o.n;
}
