//! module    rt_mat
//! provides  rt_mat_one rt_mat_addsub rt_mat_scalar rt_mat_mul rt_mat_dot rt_mat_triple rt_mat_norm
//! provides  rt_mat_det rt_mat_rank rt_mat_inv rt_mat_trans rt_mat_cpy
//! requires  fp_add fp_sub fp_mul fp_div fp_sqrt fp_cmp fp_from_i32
//! group     matrix
//! doc       MAT commands (chapter 13, probe17, D23): the order of every floating point operation.

/* A matrix is a view (rt_mat): element (i, j) is p[i * rs + j * cs], 0 <= i < rows, 0 <= j < cols.
 * The views and the MAT BASE offsets come from the caller. Sums run in increasing index order.
 * Errors (probe17): 77 different order, 79 product not defined, 82 not square, 83 target of the
 * wrong size (INV, TRANS); a singular matrix (INV) and a zero row/column (NORM) are error 0. After an
 * error nothing more is written. */

#include <stdlib.h>

#include "rt_ref.h"

#define AT(m, i, j) ((m)->p[(long)(i) * (m)->rs + (long)(j) * (m)->cs])

static f64 fabs64(f64 x) { return x & ~((f64)1 << 63); }

/* work storage of n f64 (an Amiga program would take it from its heap) */
static f64* work(long n) {
    f64* w = (f64*)malloc((size_t)(n > 0 ? n : 1) * sizeof(f64));
    if (!w) abort();
    return w;
}

void rt_mat_one(const rt_mat* a) {
    int32_t i, j;
    if (a->rows != a->cols) {
        rt_error(RT_ERR_MAT_SQUARE);
        return;
    }
    for (i = 0; i < a->rows; ++i)
        for (j = 0; j < a->cols; ++j) AT(a, i, j) = i == j ? F64_ONE : F64_ZERO;
}

/* a = b + c or b - c (elementwise); a, b, c of the same order. a may be b or c. */
void rt_mat_addsub(const rt_mat* a, const rt_mat* b, const rt_mat* c, int sub) {
    int32_t i, j;
    if (a->rows != b->rows || a->cols != b->cols || b->rows != c->rows || b->cols != c->cols) {
        rt_error(RT_ERR_MAT_ORDER);
        return;
    }
    for (i = 0; i < a->rows; ++i)
        for (j = 0; j < a->cols; ++j) AT(a, i, j) = sub ? fp_sub(AT(b, i, j), AT(c, i, j)) : fp_add(AT(b, i, j), AT(c, i, j));
}

/* op 0: a + x, 1: a - x, 2: a * x */
void rt_mat_scalar(const rt_mat* a, f64 x, int op) {
    int32_t i, j;
    for (i = 0; i < a->rows; ++i)
        for (j = 0; j < a->cols; ++j) {
            const f64 v = AT(a, i, j);
            AT(a, i, j) = op == 0 ? fp_add(v, x) : op == 1 ? fp_sub(v, x) : fp_mul(v, x);
        }
}

/* a = b * c: a(i,j) = sum over k of b(i,k) * c(k,j); a may be b or c */
void rt_mat_mul(const rt_mat* a, const rt_mat* b, const rt_mat* c) {
    int32_t i, j, k;
    f64* t;
    if (b->cols != c->rows || a->rows != b->rows || a->cols != c->cols) {
        rt_error(RT_ERR_MAT_ORDER);
        return;
    }
    t = work((long)a->rows * a->cols);
    for (i = 0; i < a->rows; ++i)
        for (j = 0; j < a->cols; ++j) {
            f64 s = F64_ZERO;
            for (k = 0; k < b->cols; ++k) s = fp_add(s, fp_mul(AT(b, i, k), AT(c, k, j)));
            t[(long)i * a->cols + j] = s;
        }
    for (i = 0; i < a->rows; ++i)
        for (j = 0; j < a->cols; ++j) AT(a, i, j) = t[(long)i * a->cols + j];
    free(t);
}

/* x = a * b for two vectors of the same length (views 1 x n or n x 1) */
f64 rt_mat_dot(const rt_mat* a, const rt_mat* b) {
    const int32_t n = a->rows * a->cols;
    int32_t k;
    f64 s = F64_ZERO;
    if ((a->rows != 1 && a->cols != 1) || (b->rows != 1 && b->cols != 1) || n != b->rows * b->cols) {
        rt_error(RT_ERR_MAT_ORDER);
        return F64_ZERO;
    }
    for (k = 0; k < n; ++k) {
        const f64 x = a->rows == 1 ? AT(a, 0, k) : AT(a, k, 0);
        const f64 y = b->rows == 1 ? AT(b, 0, k) : AT(b, k, 0);
        s = fp_add(s, fp_mul(x, y));
    }
    return s;
}

/* x = a * b * c: a (vector, length rows of b), c (vector, length cols of b); first t = b * c */
f64 rt_mat_triple(const rt_mat* a, const rt_mat* b, const rt_mat* c) {
    int32_t i, j;
    f64 x = F64_ZERO;
    if (a->rows != 1 || c->rows != 1 || a->cols != b->rows || c->cols != b->cols) {
        rt_error(RT_ERR_MAT_PRODUCT);
        return F64_ZERO;
    }
    for (i = 0; i < b->rows; ++i) {
        f64 t = F64_ZERO;
        for (j = 0; j < b->cols; ++j) t = fp_add(t, fp_mul(AT(b, i, j), AT(c, 0, j)));
        x = fp_add(x, fp_mul(AT(a, 0, i), t));
    }
    return x;
}

/* every row (byColumn 0) or column (1): divided by the root of its sum of squares; a zero line: error 0 */
void rt_mat_norm(const rt_mat* a, int byColumn) {
    const int32_t lines = byColumn ? a->cols : a->rows, len = byColumn ? a->rows : a->cols;
    int32_t l, k;
    for (l = 0; l < lines; ++l) {
        f64 s = F64_ZERO, r;
        for (k = 0; k < len; ++k) {
            const f64 v = byColumn ? AT(a, k, l) : AT(a, l, k);
            s = fp_add(s, fp_mul(v, v));
        }
        if (fp_cmp(s, F64_ZERO) == 0) {
            rt_error(RT_ERR_DIV_ZERO);
            return;
        }
        r = fp_sqrt(s);
        for (k = 0; k < len; ++k) {
            if (byColumn) AT(a, k, l) = fp_div(AT(a, k, l), r);
            else AT(a, l, k) = fp_div(AT(a, l, k), r);
        }
    }
}

/* row-major copy of a square view; *tol = n * 2^-52 * max |a(i,j)| (a pivot up to tol counts as 0) */
static f64* squareCopy(const rt_mat* a, f64* tol) {
    const int32_t n = a->rows;
    f64* w = work((long)n * n);
    f64 mx = F64_ZERO;
    int32_t i, j;
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) {
            const f64 v = AT(a, i, j);
            w[(long)i * n + j] = v;
            if (fp_cmp(fabs64(v), mx) > 0) mx = fabs64(v);
        }
    *tol = fp_mul(fp_from_i32(n), fp_mul(mx, (f64)0x3CB0000000000000u)); /* 2^-52 */
    return w;
}

static void swapRows(f64* w, int32_t n, int32_t a, int32_t b) {
    int32_t j;
    for (j = 0; j < n; ++j) {
        const f64 t = w[(long)a * n + j];
        w[(long)a * n + j] = w[(long)b * n + j];
        w[(long)b * n + j] = t;
    }
}

/* row with the largest |w(i,k)| for i >= from (the first of equal ones) */
static int32_t pivotRow(const f64* w, int32_t n, int32_t k, int32_t from) {
    int32_t i, p = from;
    for (i = from + 1; i < n; ++i)
        if (fp_cmp(fabs64(w[(long)i * n + k]), fabs64(w[(long)p * n + k])) > 0) p = i;
    return p;
}

/* rows below r: row i -= (w(i,k) / w(r,k)) * row r, columns k+1 .. n-1 */
static void eliminate(f64* w, int32_t n, int32_t r, int32_t k) {
    int32_t i, j;
    for (i = r + 1; i < n; ++i) {
        const f64 f = fp_div(w[(long)i * n + k], w[(long)r * n + k]);
        for (j = k + 1; j < n; ++j) w[(long)i * n + j] = fp_sub(w[(long)i * n + j], fp_mul(f, w[(long)r * n + j]));
        w[(long)i * n + k] = F64_ZERO;
    }
}

/* MAT DET (quick 0): largest pivot, a pivot up to tol is 0. MAT QDET (quick 1): the first pivot that
 * is not 0, no tolerance. The determinant is the product of the pivots in order, then the sign. */
f64 rt_mat_det(const rt_mat* a, int quick) {
    const int32_t n = a->rows;
    int32_t k, neg = 0;
    f64 tol, det = F64_ONE, *w;
    if (a->rows != a->cols) {
        rt_error(RT_ERR_MAT_SQUARE);
        return F64_ZERO;
    }
    w = squareCopy(a, &tol);
    for (k = 0; k < n; ++k) {
        int32_t p = k;
        if (quick) {
            while (p < n && fp_cmp(w[(long)p * n + k], F64_ZERO) == 0) ++p;
            if (p == n) {
                det = F64_ZERO;
                break;
            }
        } else {
            p = pivotRow(w, n, k, k);
            if (fp_cmp(fabs64(w[(long)p * n + k]), tol) <= 0) {
                det = F64_ZERO;
                break;
            }
        }
        if (p != k) {
            swapRows(w, n, p, k);
            neg = !neg;
        }
        det = fp_mul(det, w[(long)k * n + k]);
        eliminate(w, n, k, k);
    }
    free(w);
    if (neg && fp_cmp(det, F64_ZERO) != 0) det ^= (f64)1 << 63;
    return det;
}

/* MAT RANG: row echelon form with the largest pivots; a column without a pivot above tol is skipped */
f64 rt_mat_rank(const rt_mat* a) {
    const int32_t n = a->rows;
    int32_t k, r = 0;
    f64 tol, *w;
    if (a->rows != a->cols) {
        rt_error(RT_ERR_MAT_SQUARE);
        return F64_ZERO;
    }
    w = squareCopy(a, &tol);
    for (k = 0; k < n && r < n; ++k) {
        const int32_t p = pivotRow(w, n, k, r);
        if (fp_cmp(fabs64(w[(long)p * n + k]), tol) <= 0) continue;
        if (p != r) swapRows(w, n, p, r);
        eliminate(w, n, r, k);
        ++r;
    }
    free(w);
    return fp_from_i32(r);
}

/* MAT INV b()=a(): Gauss-Jordan with the largest pivots; singular (pivot up to tol): error 0 */
void rt_mat_inv(const rt_mat* b, const rt_mat* a) {
    const int32_t n = a->rows;
    int32_t i, j, k;
    f64 tol, *w, *v;
    if (a->rows != a->cols) {
        rt_error(RT_ERR_MAT_SQUARE);
        return;
    }
    if (b->rows != n || b->cols != n) {
        rt_error(RT_ERR_MAT_TRANS);
        return;
    }
    w = squareCopy(a, &tol);
    v = work((long)n * n);
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) v[(long)i * n + j] = i == j ? F64_ONE : F64_ZERO;
    for (k = 0; k < n; ++k) {
        const int32_t p = pivotRow(w, n, k, k);
        f64 piv;
        if (fp_cmp(fabs64(w[(long)p * n + k]), tol) <= 0) {
            free(w);
            free(v);
            rt_error(RT_ERR_DIV_ZERO);
            return;
        }
        if (p != k) {
            swapRows(w, n, p, k);
            swapRows(v, n, p, k);
        }
        piv = w[(long)k * n + k];
        for (j = k + 1; j < n; ++j) w[(long)k * n + j] = fp_div(w[(long)k * n + j], piv);
        for (j = 0; j < n; ++j) v[(long)k * n + j] = fp_div(v[(long)k * n + j], piv);
        w[(long)k * n + k] = F64_ONE;
        for (i = 0; i < n; ++i) {
            f64 f;
            if (i == k) continue;
            f = w[(long)i * n + k];
            if (fp_cmp(f, F64_ZERO) == 0) continue;
            for (j = k + 1; j < n; ++j) w[(long)i * n + j] = fp_sub(w[(long)i * n + j], fp_mul(f, w[(long)k * n + j]));
            for (j = 0; j < n; ++j) v[(long)i * n + j] = fp_sub(v[(long)i * n + j], fp_mul(f, v[(long)k * n + j]));
            w[(long)i * n + k] = F64_ZERO;
        }
    }
    for (i = 0; i < n; ++i)
        for (j = 0; j < n; ++j) AT(b, i, j) = v[(long)i * n + j];
    free(w);
    free(v);
}

/* MAT TRANS a()=b(): a(j,i) = b(i,j); a must be cols(b) x rows(b) (error 83). a may be b (square). */
void rt_mat_trans(const rt_mat* a, const rt_mat* b) {
    int32_t i, j;
    f64* t;
    if (a->rows != b->cols || a->cols != b->rows) {
        rt_error(RT_ERR_MAT_TRANS);
        return;
    }
    t = work((long)b->rows * b->cols);
    for (i = 0; i < b->rows; ++i)
        for (j = 0; j < b->cols; ++j) t[(long)i * b->cols + j] = AT(b, i, j);
    for (i = 0; i < b->rows; ++i)
        for (j = 0; j < b->cols; ++j) AT(a, j, i) = t[(long)i * b->cols + j];
    free(t);
}

/* MAT CPY / XCPY: a block of h rows x w columns of the source from (si, sj) to (di, dj) of the
 * destination (transposed: source (si + r, sj + c) -> destination (di + c, dj + r)). h, w < 0: as
 * many as fit; only elements whose indices exist in source and destination are copied. */
void rt_mat_cpy(const rt_mat* d, int32_t di, int32_t dj, const rt_mat* s, int32_t si, int32_t sj, int32_t h, int32_t w,
                int transpose) {
    int32_t r, c, hh = s->rows - si, ww = s->cols - sj;
    f64* t;
    if (transpose) {
        if (d->cols - dj < hh) hh = d->cols - dj;
        if (d->rows - di < ww) ww = d->rows - di;
    } else {
        if (d->rows - di < hh) hh = d->rows - di;
        if (d->cols - dj < ww) ww = d->cols - dj;
    }
    if (h >= 0 && h < hh) hh = h;
    if (w >= 0 && w < ww) ww = w;
    if (si < 0 || sj < 0 || di < 0 || dj < 0 || hh <= 0 || ww <= 0) return;
    t = work((long)hh * ww); /* the source may overlap the destination */
    for (r = 0; r < hh; ++r)
        for (c = 0; c < ww; ++c) t[(long)r * ww + c] = AT(s, si + r, sj + c);
    for (r = 0; r < hh; ++r)
        for (c = 0; c < ww; ++c) {
            if (transpose) AT(d, di + c, dj + r) = t[(long)r * ww + c];
            else AT(d, di + r, dj + c) = t[(long)r * ww + c];
        }
    free(t);
}
