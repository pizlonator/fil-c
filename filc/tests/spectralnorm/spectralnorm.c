/* The Computer Language Benchmarks Game
 * https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
 *
 * contributed by Miles
 * optimization with 4x4 kernel + intrinsics
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <x86intrin.h>

// compute values of A 4 at a time instead of 1
static inline __m256d eval_A(__m128i i, __m128i j) {
    __m128i ONE = _mm_set1_epi32(1);
    __m128i ip1 = _mm_add_epi32(i, ONE);
    __m128i ipj = _mm_add_epi32(i, j);
    __m128i ipjp1 = _mm_add_epi32(ip1, j);
    __m128i a = _mm_mullo_epi32(ipj, ipjp1);
    a = _mm_srli_epi32(a, 1);
    a = _mm_add_epi32(a, ip1);
    return _mm256_cvtepi32_pd(a);
}

// compute results over a 4x4 submatrix of A
static inline void kernel(__m256d u, __m256d s[4], __m256d r[4]) {
    __m256d f[4], p[4];

    // f[i] is each outfix of size 1 for r[i], scaled by u
    // p[i] is the product of r[i]
    for (int i = 0; i < 4; i++) {
        f[i] = _mm256_permute_pd(r[i], 0b0101);
        p[i] = _mm256_mul_pd(r[i], f[i]);
        __m256d t = _mm256_permute2f128_pd(p[i], p[i], 0x01);
        f[i] = _mm256_mul_pd(t, f[i]);
        p[i] = _mm256_mul_pd(t, p[i]);
        f[i] = _mm256_mul_pd(f[i], u);
    }

    __m256d w, x, y, z;

    // collect p[i] into z, and get reciprocal
    x = _mm256_blend_pd(p[0], p[1], 0b1010);
    y = _mm256_blend_pd(p[2], p[3], 0b1010);
    z = _mm256_blend_pd(x, y, 0b1100);
    __m128 q = _mm256_cvtpd_ps(z);
    // approximate reciprocal
    q = _mm_rcp_ps(q);
    x = _mm256_cvtps_pd(q);
    // refine with variation of Goldschmidt’s algorithm
    w = _mm256_mul_pd(x, z);
    y = _mm256_set1_pd(3.0);
    z = _mm256_mul_pd(w, x);
    w = _mm256_sub_pd(w, y);
    x = _mm256_mul_pd(x, y);
    z = _mm256_mul_pd(z, w);
    z = _mm256_add_pd(z, x);

    // broadcast each 1/z over p[i]
    x = _mm256_unpacklo_pd(z, z);
    y = _mm256_unpackhi_pd(z, z);
    w = _mm256_permute2f128_pd(x, x, 1);
    z = _mm256_permute2f128_pd(y, y, 1);
    p[0] = _mm256_blend_pd(x, w, 0b1100);
    p[1] = _mm256_blend_pd(y, z, 0b1100);
    p[2] = _mm256_blend_pd(x, w, 0b0011);
    p[3] = _mm256_blend_pd(y, z, 0b0011);

    // increment each row-sum by the product u / A[i, j..j+3]
    for (int i = 0; i < 4; i++) {
        __m256d t = _mm256_mul_pd(f[i], p[i]);
        s[i] = _mm256_add_pd(s[i], t);
    }
}

static void eval_A_times_u(int n, double *u, double *Au) {
    // force static schedule since each chunk performs equal amounts of work
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i += 4) {
        __m256d s[4];
        for (int k = 0; k < 4; k++)
            s[k] = _mm256_set1_pd(0.0);

        for (int j = 0; j < n; j += 4) {
            __m256d r[4];
            // generate the values of A for the 4x4 submatrix with
            // upper-left at (i, j)
            for (int k = 0; k < 4; k++) {
                __m128i x = _mm_set1_epi32(i+k);
                __m128i y = _mm_setr_epi32(j, j+1, j+2, j+3);
                r[k] = eval_A(x, y);
            }

            kernel(_mm256_load_pd(u+j), s, r);
        }

        // sum the values in each s[i] and store in z
        __m256d t0 = _mm256_hadd_pd(s[0], s[1]);
        __m256d t1 = _mm256_hadd_pd(s[2], s[3]);
        __m256d x = _mm256_permute2f128_pd(t0, t1, 0x21);
        __m256d y = _mm256_blend_pd(t0, t1, 0b1100);
        __m256d z = _mm256_add_pd(x, y);

        _mm256_store_pd(Au+i, z);
    }

    // clear overhang values
    Au[n] = 0.0;
    Au[n+1] = 0.0;
    Au[n+2] = 0.0;
}

// same as above except indices of A flipped (transposed)
static void eval_At_times_u(int n, double *u, double *Au) {
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < n; i += 4) {
        __m256d s[4];
        for (int k = 0; k < 4; k++)
            s[k] = _mm256_set1_pd(0.0);

        for (int j = 0; j < n; j += 4) {
            __m256d r[4];
            for (int k = 0; k < 4; k++) {
                __m128i x = _mm_set1_epi32(i+k);
                __m128i y = _mm_setr_epi32(j, j+1, j+2, j+3);
                r[k] = eval_A(y, x);
            }

            kernel(_mm256_load_pd(u+j), s, r);
        }

        __m256d t0 = _mm256_hadd_pd(s[0], s[1]);
        __m256d t1 = _mm256_hadd_pd(s[2], s[3]);
        __m256d x = _mm256_permute2f128_pd(t0, t1, 0x21);
        __m256d y = _mm256_blend_pd(t0, t1, 0b1100);
        __m256d z = _mm256_add_pd(x, y);

        _mm256_store_pd(Au+i, z);
    }

    Au[n] = 0.0;
    Au[n+1] = 0.0;
    Au[n+2] = 0.0;
}

static void eval_AtA_times_u(int n, double *u, double *AtAu) {
    double v[n+3] __attribute__((aligned(sizeof(__m256d))));

    eval_A_times_u(n, u, v);
    eval_At_times_u(n, v, AtAu);
}

int main(int argc, char *argv[]) {
    int n = atoi(argv[1]);

    // overhang of 3 values for computing in strides of 4 incase n % 4 != 0
    // aligned to __m256d to use aligned loads/stores
    double u[n+3] __attribute__((aligned(sizeof(__m256d))));
    double v[n+3] __attribute__((aligned(sizeof(__m256d))));

    for (int i = 0; i < n; i++)
        u[i] = 1.0;
    // initiate overhang values to zero
    u[n] = 0.0;
    u[n+1] = 0.0;
    u[n+2] = 0.0;

    for (int i = 0; i < 10; i++) {
        eval_AtA_times_u(n, u, v);
        eval_AtA_times_u(n, v, u);
    }

    __m256d uv = _mm256_set1_pd(0.0);
    __m256d v2 = _mm256_set1_pd(0.0);

    for (int i = 0; i < n; i += 4) {
        __m256d x = _mm256_load_pd(u+i);
        __m256d y = _mm256_load_pd(v+i);
        x = _mm256_mul_pd(x, y);
        y = _mm256_mul_pd(y, y);
        uv = _mm256_add_pd(uv, x);
        v2 = _mm256_add_pd(v2, y);
    }

    __m256d z = _mm256_hadd_pd(uv, v2);
    __m128d x = _mm256_extractf128_pd(z, 0);
    __m128d y = _mm256_extractf128_pd(z, 1);
    x = _mm_add_pd(x, y);
    x = _mm_sqrt_pd(x);
    double r[2] __attribute__((aligned(sizeof(__m128d))));
    _mm_store_pd(r, x);

    printf("%0.9f\n", r[0] / r[1]);

    return 0;
}
