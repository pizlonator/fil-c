/*--

This file is a part of libsais, a library for linear time suffix array,
longest common prefix array and burrows wheeler transform construction.

   Copyright (c) 2021-2022 Ilya Grebnov <ilya.grebnov@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

Please see the file LICENSE for full copyright information.

The stability patches that fix undefined behaviour in unbwt routines:

   Copyright (c) 2022 Kamila Szewczyk <kspalaiologos@gmail.com>

   Licensed under the same license as the original software.

--*/

#ifndef LIBSAIS_H
#define LIBSAIS_H

#include "common.h"

/* libsais source code amalgamate. */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(_x) (void)(_x)

typedef s32 sa_sint_t;
typedef u32 sa_uint_t;
typedef ptrdiff_t fast_sint_t;
typedef size_t fast_uint_t;

#define SAINT_BIT (32)
#define SAINT_MAX INT32_MAX
#define SAINT_MIN INT32_MIN

#define ALPHABET_SIZE (1 << CHAR_BIT)
#define UNBWT_FASTBITS (17)

#define SUFFIX_GROUP_BIT (SAINT_BIT - 1)
#define SUFFIX_GROUP_MARKER (((sa_sint_t)1) << (SUFFIX_GROUP_BIT - 1))

#define BUCKETS_INDEX2(_c, _s) (((_c) << 1) + (_s))
#define BUCKETS_INDEX4(_c, _s) (((_c) << 2) + (_s))

#define LIBSAIS_PER_THREAD_CACHE_SIZE (24576)

typedef struct LIBSAIS_THREAD_CACHE {
    sa_sint_t symbol;
    sa_sint_t index;
} LIBSAIS_THREAD_CACHE;

typedef union LIBSAIS_THREAD_STATE {
    struct {
        fast_sint_t position;
        fast_sint_t count;

        fast_sint_t m;
        fast_sint_t last_lms_suffix;

        sa_sint_t * buckets;
        LIBSAIS_THREAD_CACHE * cache;
    } state;

    u8 padding[64];
} LIBSAIS_THREAD_STATE;

typedef struct LIBSAIS_CONTEXT {
    sa_sint_t * buckets;
    LIBSAIS_THREAD_STATE * thread_state;
    fast_sint_t threads;
} LIBSAIS_CONTEXT;

typedef struct LIBSAIS_UNBWT_CONTEXT {
    sa_uint_t * bucket2;
    u16 * fastbits;
    sa_uint_t * buckets;
    fast_sint_t threads;
} LIBSAIS_UNBWT_CONTEXT;

static void * libsais_align_up(const void * address, size_t alignment) {
    return (void *)((((ptrdiff_t)address) + ((ptrdiff_t)alignment) - 1) & (-((ptrdiff_t)alignment)));
}

static void * libsais_alloc_aligned(size_t size, size_t alignment) {
    void * address = malloc(size + sizeof(short) + alignment - 1);
    if (address != NULL) {
        void * aligned_address = libsais_align_up((void *)((ptrdiff_t)address + (ptrdiff_t)(sizeof(short))), alignment);
        ((short *)aligned_address)[-1] = (short)((ptrdiff_t)aligned_address - (ptrdiff_t)address);

        return aligned_address;
    }

    return NULL;
}

static void libsais_free_aligned(void * aligned_address) {
    if (aligned_address != NULL) {
        free((void *)((ptrdiff_t)aligned_address - ((short *)aligned_address)[-1]));
    }
}

static LIBSAIS_THREAD_STATE * libsais_alloc_thread_state(sa_sint_t threads) {
    LIBSAIS_THREAD_STATE * RESTRICT thread_state =
        (LIBSAIS_THREAD_STATE *)libsais_alloc_aligned((size_t)threads * sizeof(LIBSAIS_THREAD_STATE), 4096);
    sa_sint_t * RESTRICT thread_buckets =
        (sa_sint_t *)libsais_alloc_aligned((size_t)threads * 4 * ALPHABET_SIZE * sizeof(sa_sint_t), 4096);
    LIBSAIS_THREAD_CACHE * RESTRICT thread_cache = (LIBSAIS_THREAD_CACHE *)libsais_alloc_aligned(
        (size_t)threads * LIBSAIS_PER_THREAD_CACHE_SIZE * sizeof(LIBSAIS_THREAD_CACHE), 4096);

    if (thread_state != NULL && thread_buckets != NULL && thread_cache != NULL) {
        fast_sint_t t;
        for (t = 0; t < threads; ++t) {
            thread_state[t].state.buckets = thread_buckets;
            thread_buckets += 4 * ALPHABET_SIZE;
            thread_state[t].state.cache = thread_cache;
            thread_cache += LIBSAIS_PER_THREAD_CACHE_SIZE;
        }

        return thread_state;
    }

    libsais_free_aligned(thread_cache);
    libsais_free_aligned(thread_buckets);
    libsais_free_aligned(thread_state);
    return NULL;
}

static void libsais_free_thread_state(LIBSAIS_THREAD_STATE * thread_state) {
    if (thread_state != NULL) {
        libsais_free_aligned(thread_state[0].state.cache);
        libsais_free_aligned(thread_state[0].state.buckets);
        libsais_free_aligned(thread_state);
    }
}

static LIBSAIS_CONTEXT * libsais_create_ctx_main(sa_sint_t threads) {
    LIBSAIS_CONTEXT * RESTRICT ctx = (LIBSAIS_CONTEXT *)libsais_alloc_aligned(sizeof(LIBSAIS_CONTEXT), 64);
    sa_sint_t * RESTRICT buckets = (sa_sint_t *)libsais_alloc_aligned(8 * ALPHABET_SIZE * sizeof(sa_sint_t), 4096);
    LIBSAIS_THREAD_STATE * RESTRICT thread_state = threads > 1 ? libsais_alloc_thread_state(threads) : NULL;

    if (ctx != NULL && buckets != NULL && (thread_state != NULL || threads == 1)) {
        ctx->buckets = buckets;
        ctx->threads = threads;
        ctx->thread_state = thread_state;

        return ctx;
    }

    libsais_free_thread_state(thread_state);
    libsais_free_aligned(buckets);
    libsais_free_aligned(ctx);
    return NULL;
}

static void libsais_free_ctx_main(LIBSAIS_CONTEXT * ctx) {
    if (ctx != NULL) {
        libsais_free_thread_state(ctx->thread_state);
        libsais_free_aligned(ctx->buckets);
        libsais_free_aligned(ctx);
    }
}
static void libsais_gather_lms_suffixes_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n, fast_sint_t m,
                                           fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    if (omp_block_size > 0) {
        const fast_sint_t prefetch_distance = 128;

        fast_sint_t i, j = omp_block_start + omp_block_size, c0 = T[omp_block_start + omp_block_size - 1], c1 = -1;

        while (j < n && (c1 = T[j]) == c0) {
            ++j;
        }

        fast_uint_t s = c0 >= c1;

        for (i = omp_block_start + omp_block_size - 2, j = omp_block_start + 3; i >= j; i -= 4) {
            prefetch(&T[i - prefetch_distance]);

            c1 = T[i - 0];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((s & 3) == 1);
            c0 = T[i - 1];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 0);
            m -= ((s & 3) == 1);
            c1 = T[i - 2];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 1);
            m -= ((s & 3) == 1);
            c0 = T[i - 3];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 2);
            m -= ((s & 3) == 1);
        }

        for (j -= 3; i >= j; i -= 1) {
            c1 = c0;
            c0 = T[i];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((s & 3) == 1);
        }

        SA[m] = (sa_sint_t)(i + 1);
    }
}

static void libsais_gather_lms_suffixes_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                               sa_sint_t threads, LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (n / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : n - omp_block_start;

        if (omp_num_threads == 1) {
            libsais_gather_lms_suffixes_8u(T, SA, n, (fast_sint_t)n - 1, omp_block_start, omp_block_size);
        }
    }
}

static sa_sint_t libsais_gather_lms_suffixes_32s(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t i = n - 2;
    sa_sint_t m = n - 1;
    fast_uint_t s = 1;
    fast_sint_t c0 = T[n - 1];
    fast_sint_t c1 = 0;

    for (; i >= 3; i -= 4) {
        prefetch(&T[i - prefetch_distance]);

        c1 = T[i - 0];
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        SA[m] = i + 1;
        m -= ((s & 3) == 1);
        c0 = T[i - 1];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        SA[m] = i - 0;
        m -= ((s & 3) == 1);
        c1 = T[i - 2];
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        SA[m] = i - 1;
        m -= ((s & 3) == 1);
        c0 = T[i - 3];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        SA[m] = i - 2;
        m -= ((s & 3) == 1);
    }

    for (; i >= 0; i -= 1) {
        c1 = c0;
        c0 = T[i];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        SA[m] = i + 1;
        m -= ((s & 3) == 1);
    }

    return n - 1 - m;
}

static sa_sint_t libsais_gather_compacted_lms_suffixes_32s(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                           sa_sint_t n) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t i = n - 2;
    sa_sint_t m = n - 1;
    fast_uint_t s = 1;
    fast_sint_t c0 = T[n - 1];
    fast_sint_t c1 = 0;

    for (; i >= 3; i -= 4) {
        prefetch(&T[i - prefetch_distance]);

        c1 = T[i - 0];
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        SA[m] = i + 1;
        m -= ((fast_sint_t)(s & 3) == (c0 >= 0));
        c0 = T[i - 1];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        SA[m] = i - 0;
        m -= ((fast_sint_t)(s & 3) == (c1 >= 0));
        c1 = T[i - 2];
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        SA[m] = i - 1;
        m -= ((fast_sint_t)(s & 3) == (c0 >= 0));
        c0 = T[i - 3];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        SA[m] = i - 2;
        m -= ((fast_sint_t)(s & 3) == (c1 >= 0));
    }

    for (; i >= 0; i -= 1) {
        c1 = c0;
        c0 = T[i];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        SA[m] = i + 1;
        m -= ((fast_sint_t)(s & 3) == (c1 >= 0));
    }

    return n - 1 - m;
}
static void libsais_count_lms_suffixes_32s_2k(const sa_sint_t * RESTRICT T, sa_sint_t n, sa_sint_t k,
                                              sa_sint_t * RESTRICT buckets) {
    const fast_sint_t prefetch_distance = 32;

    memset(buckets, 0, 2 * (size_t)k * sizeof(sa_sint_t));

    sa_sint_t i = n - 2;
    fast_uint_t s = 1;
    fast_sint_t c0 = T[n - 1];
    fast_sint_t c1 = 0;

    for (; i >= prefetch_distance + 3; i -= 4) {
        prefetch(&T[i - 2 * prefetch_distance]);

        prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 0], 0)]);
        prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 1], 0)]);
        prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 2], 0)]);
        prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 3], 0)]);

        c1 = T[i - 0];
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        buckets[BUCKETS_INDEX2((fast_uint_t)c0, (s & 3) == 1)]++;

        c0 = T[i - 1];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;

        c1 = T[i - 2];
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        buckets[BUCKETS_INDEX2((fast_uint_t)c0, (s & 3) == 1)]++;

        c0 = T[i - 3];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;
    }

    for (; i >= 0; i -= 1) {
        c1 = c0;
        c0 = T[i];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;
    }

    buckets[BUCKETS_INDEX2((fast_uint_t)c0, 0)]++;
}
static sa_sint_t libsais_count_and_gather_lms_suffixes_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                          sa_sint_t * RESTRICT buckets, fast_sint_t omp_block_start,
                                                          fast_sint_t omp_block_size) {
    memset(buckets, 0, 4 * ALPHABET_SIZE * sizeof(sa_sint_t));

    fast_sint_t m = omp_block_start + omp_block_size - 1;

    if (omp_block_size > 0) {
        const fast_sint_t prefetch_distance = 128;

        fast_sint_t i, j = m + 1, c0 = T[m], c1 = -1;

        while (j < n && (c1 = T[j]) == c0) {
            ++j;
        }

        fast_uint_t s = c0 >= c1;

        for (i = m - 1, j = omp_block_start + 3; i >= j; i -= 4) {
            prefetch(&T[i - prefetch_distance]);

            c1 = T[i - 0];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c0, s & 3)]++;

            c0 = T[i - 1];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 0);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c1, s & 3)]++;

            c1 = T[i - 2];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c0, s & 3)]++;

            c0 = T[i - 3];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 2);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c1, s & 3)]++;
        }

        for (j -= 3; i >= j; i -= 1) {
            c1 = c0;
            c0 = T[i];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c1, s & 3)]++;
        }

        c1 = (i >= 0) ? T[i] : -1;
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        SA[m] = (sa_sint_t)(i + 1);
        m -= ((s & 3) == 1);
        buckets[BUCKETS_INDEX4((fast_uint_t)c0, s & 3)]++;
    }

    return (sa_sint_t)(omp_block_start + omp_block_size - 1 - m);
}

static sa_sint_t libsais_count_and_gather_lms_suffixes_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                              sa_sint_t n, sa_sint_t * RESTRICT buckets,
                                                              sa_sint_t threads,
                                                              LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t m = 0;

    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (n / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : n - omp_block_start;

        if (omp_num_threads == 1) {
            m = libsais_count_and_gather_lms_suffixes_8u(T, SA, n, buckets, omp_block_start, omp_block_size);
        }
    }

    return m;
}

static sa_sint_t libsais_count_and_gather_lms_suffixes_32s_4k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                              sa_sint_t n, sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                              fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    memset(buckets, 0, 4 * (size_t)k * sizeof(sa_sint_t));

    fast_sint_t m = omp_block_start + omp_block_size - 1;

    if (omp_block_size > 0) {
        const fast_sint_t prefetch_distance = 32;

        fast_sint_t i, j = m + 1, c0 = T[m], c1 = -1;

        while (j < n && (c1 = T[j]) == c0) {
            ++j;
        }

        fast_uint_t s = c0 >= c1;

        for (i = m - 1, j = omp_block_start + prefetch_distance + 3; i >= j; i -= 4) {
            prefetch(&T[i - 2 * prefetch_distance]);

            prefetchw(&buckets[BUCKETS_INDEX4(T[i - prefetch_distance - 0], 0)]);
            prefetchw(&buckets[BUCKETS_INDEX4(T[i - prefetch_distance - 1], 0)]);
            prefetchw(&buckets[BUCKETS_INDEX4(T[i - prefetch_distance - 2], 0)]);
            prefetchw(&buckets[BUCKETS_INDEX4(T[i - prefetch_distance - 3], 0)]);

            c1 = T[i - 0];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c0, s & 3)]++;

            c0 = T[i - 1];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 0);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c1, s & 3)]++;

            c1 = T[i - 2];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c0, s & 3)]++;

            c0 = T[i - 3];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 2);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c1, s & 3)]++;
        }

        for (j -= prefetch_distance + 3; i >= j; i -= 1) {
            c1 = c0;
            c0 = T[i];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX4((fast_uint_t)c1, s & 3)]++;
        }

        c1 = (i >= 0) ? T[i] : -1;
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        SA[m] = (sa_sint_t)(i + 1);
        m -= ((s & 3) == 1);
        buckets[BUCKETS_INDEX4((fast_uint_t)c0, s & 3)]++;
    }

    return (sa_sint_t)(omp_block_start + omp_block_size - 1 - m);
}

static sa_sint_t libsais_count_and_gather_lms_suffixes_32s_2k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                              sa_sint_t n, sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                              fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    memset(buckets, 0, 2 * (size_t)k * sizeof(sa_sint_t));

    fast_sint_t m = omp_block_start + omp_block_size - 1;

    if (omp_block_size > 0) {
        const fast_sint_t prefetch_distance = 32;

        fast_sint_t i, j = m + 1, c0 = T[m], c1 = -1;

        while (j < n && (c1 = T[j]) == c0) {
            ++j;
        }

        fast_uint_t s = c0 >= c1;

        for (i = m - 1, j = omp_block_start + prefetch_distance + 3; i >= j; i -= 4) {
            prefetch(&T[i - 2 * prefetch_distance]);

            prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 0], 0)]);
            prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 1], 0)]);
            prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 2], 0)]);
            prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 3], 0)]);

            c1 = T[i - 0];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX2((fast_uint_t)c0, (s & 3) == 1)]++;

            c0 = T[i - 1];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 0);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;

            c1 = T[i - 2];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX2((fast_uint_t)c0, (s & 3) == 1)]++;

            c0 = T[i - 3];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 2);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;
        }

        for (j -= prefetch_distance + 3; i >= j; i -= 1) {
            c1 = c0;
            c0 = T[i];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((s & 3) == 1);
            buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;
        }

        c1 = (i >= 0) ? T[i] : -1;
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        SA[m] = (sa_sint_t)(i + 1);
        m -= ((s & 3) == 1);
        buckets[BUCKETS_INDEX2((fast_uint_t)c0, (s & 3) == 1)]++;
    }

    return (sa_sint_t)(omp_block_start + omp_block_size - 1 - m);
}

static sa_sint_t libsais_count_and_gather_compacted_lms_suffixes_32s_2k(const sa_sint_t * RESTRICT T,
                                                                        sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                        sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                                        fast_sint_t omp_block_start,
                                                                        fast_sint_t omp_block_size) {
    memset(buckets, 0, 2 * (size_t)k * sizeof(sa_sint_t));

    fast_sint_t m = omp_block_start + omp_block_size - 1;

    if (omp_block_size > 0) {
        const fast_sint_t prefetch_distance = 32;

        fast_sint_t i, j = m + 1, c0 = T[m], c1 = -1;

        while (j < n && (c1 = T[j]) == c0) {
            ++j;
        }

        fast_uint_t s = c0 >= c1;

        for (i = m - 1, j = omp_block_start + prefetch_distance + 3; i >= j; i -= 4) {
            prefetch(&T[i - 2 * prefetch_distance]);

            prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 0] & SAINT_MAX, 0)]);
            prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 1] & SAINT_MAX, 0)]);
            prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 2] & SAINT_MAX, 0)]);
            prefetchw(&buckets[BUCKETS_INDEX2(T[i - prefetch_distance - 3] & SAINT_MAX, 0)]);

            c1 = T[i - 0];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((fast_sint_t)(s & 3) == (c0 >= 0));
            c0 &= SAINT_MAX;
            buckets[BUCKETS_INDEX2((fast_uint_t)c0, (s & 3) == 1)]++;

            c0 = T[i - 1];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 0);
            m -= ((fast_sint_t)(s & 3) == (c1 >= 0));
            c1 &= SAINT_MAX;
            buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;

            c1 = T[i - 2];
            s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 1);
            m -= ((fast_sint_t)(s & 3) == (c0 >= 0));
            c0 &= SAINT_MAX;
            buckets[BUCKETS_INDEX2((fast_uint_t)c0, (s & 3) == 1)]++;

            c0 = T[i - 3];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i - 2);
            m -= ((fast_sint_t)(s & 3) == (c1 >= 0));
            c1 &= SAINT_MAX;
            buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;
        }

        for (j -= prefetch_distance + 3; i >= j; i -= 1) {
            c1 = c0;
            c0 = T[i];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            SA[m] = (sa_sint_t)(i + 1);
            m -= ((fast_sint_t)(s & 3) == (c1 >= 0));
            c1 &= SAINT_MAX;
            buckets[BUCKETS_INDEX2((fast_uint_t)c1, (s & 3) == 1)]++;
        }

        c1 = (i >= 0) ? T[i] : -1;
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        SA[m] = (sa_sint_t)(i + 1);
        m -= ((fast_sint_t)(s & 3) == (c0 >= 0));
        c0 &= SAINT_MAX;
        buckets[BUCKETS_INDEX2((fast_uint_t)c0, (s & 3) == 1)]++;
    }

    return (sa_sint_t)(omp_block_start + omp_block_size - 1 - m);
}
static sa_sint_t libsais_count_and_gather_lms_suffixes_32s_4k_nofs_omp(const sa_sint_t * RESTRICT T,
                                                                       sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                       sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                                       sa_sint_t threads) {
    sa_sint_t m = 0;
    {
        (void)(threads);

        fast_sint_t omp_num_threads = 1;

        if (omp_num_threads == 1) {
            m = libsais_count_and_gather_lms_suffixes_32s_4k(T, SA, n, k, buckets, 0, n);
        }
    }

    return m;
}

static sa_sint_t libsais_count_and_gather_lms_suffixes_32s_2k_nofs_omp(const sa_sint_t * RESTRICT T,
                                                                       sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                       sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                                       sa_sint_t threads) {
    sa_sint_t m = 0;
    {
        (void)(threads);

        fast_sint_t omp_num_threads = 1;

        if (omp_num_threads == 1) {
            m = libsais_count_and_gather_lms_suffixes_32s_2k(T, SA, n, k, buckets, 0, n);
        }
    }

    return m;
}

static sa_sint_t libsais_count_and_gather_compacted_lms_suffixes_32s_2k_nofs_omp(const sa_sint_t * RESTRICT T,
                                                                                 sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                                 sa_sint_t k,
                                                                                 sa_sint_t * RESTRICT buckets,
                                                                                 sa_sint_t threads) {
    sa_sint_t m = 0;
    {
        (void)(threads);

        fast_sint_t omp_num_threads = 1;

        if (omp_num_threads == 1) {
            m = libsais_count_and_gather_compacted_lms_suffixes_32s_2k(T, SA, n, k, buckets, 0, n);
        }
    }

    return m;
}

static sa_sint_t libsais_count_and_gather_lms_suffixes_32s_4k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                                  sa_sint_t n, sa_sint_t k,
                                                                  sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                                                  LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t m;
    (void)(thread_state);

    { m = libsais_count_and_gather_lms_suffixes_32s_4k_nofs_omp(T, SA, n, k, buckets, threads); }

    return m;
}

static sa_sint_t libsais_count_and_gather_lms_suffixes_32s_2k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                                  sa_sint_t n, sa_sint_t k,
                                                                  sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                                                  LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t m;
    (void)(thread_state);

    { m = libsais_count_and_gather_lms_suffixes_32s_2k_nofs_omp(T, SA, n, k, buckets, threads); }

    return m;
}

static void libsais_count_and_gather_compacted_lms_suffixes_32s_2k_omp(const sa_sint_t * RESTRICT T,
                                                                       sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                       sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                                       sa_sint_t threads,
                                                                       LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    (void)(thread_state);

    { libsais_count_and_gather_compacted_lms_suffixes_32s_2k_nofs_omp(T, SA, n, k, buckets, threads); }
}

static void libsais_count_suffixes_32s(const sa_sint_t * RESTRICT T, sa_sint_t n, sa_sint_t k,
                                       sa_sint_t * RESTRICT buckets) {
    const fast_sint_t prefetch_distance = 32;

    memset(buckets, 0, (size_t)k * sizeof(sa_sint_t));

    fast_sint_t i, j;
    for (i = 0, j = (fast_sint_t)n - 7; i < j; i += 8) {
        prefetch(&T[i + prefetch_distance]);

        buckets[T[i + 0]]++;
        buckets[T[i + 1]]++;
        buckets[T[i + 2]]++;
        buckets[T[i + 3]]++;
        buckets[T[i + 4]]++;
        buckets[T[i + 5]]++;
        buckets[T[i + 6]]++;
        buckets[T[i + 7]]++;
    }

    for (j += 7; i < j; i += 1) {
        buckets[T[i]]++;
    }
}

static void libsais_initialize_buckets_start_and_end_8u(sa_sint_t * RESTRICT buckets, sa_sint_t * RESTRICT freq) {
    sa_sint_t * RESTRICT bucket_start = &buckets[6 * ALPHABET_SIZE];
    sa_sint_t * RESTRICT bucket_end = &buckets[7 * ALPHABET_SIZE];

    if (freq != NULL) {
        fast_sint_t i, j;
        sa_sint_t sum = 0;
        for (i = BUCKETS_INDEX4(0, 0), j = 0; i <= BUCKETS_INDEX4(ALPHABET_SIZE - 1, 0);
             i += BUCKETS_INDEX4(1, 0), j += 1) {
            bucket_start[j] = sum;
            sum += (freq[j] = buckets[i + BUCKETS_INDEX4(0, 0)] + buckets[i + BUCKETS_INDEX4(0, 1)] +
                              buckets[i + BUCKETS_INDEX4(0, 2)] + buckets[i + BUCKETS_INDEX4(0, 3)]);
            bucket_end[j] = sum;
        }
    } else {
        fast_sint_t i, j;
        sa_sint_t sum = 0;
        for (i = BUCKETS_INDEX4(0, 0), j = 0; i <= BUCKETS_INDEX4(ALPHABET_SIZE - 1, 0);
             i += BUCKETS_INDEX4(1, 0), j += 1) {
            bucket_start[j] = sum;
            sum += buckets[i + BUCKETS_INDEX4(0, 0)] + buckets[i + BUCKETS_INDEX4(0, 1)] +
                   buckets[i + BUCKETS_INDEX4(0, 2)] + buckets[i + BUCKETS_INDEX4(0, 3)];
            bucket_end[j] = sum;
        }
    }
}

static void libsais_initialize_buckets_start_and_end_32s_6k(sa_sint_t k, sa_sint_t * RESTRICT buckets) {
    sa_sint_t * RESTRICT bucket_start = &buckets[4 * k];
    sa_sint_t * RESTRICT bucket_end = &buckets[5 * k];

    fast_sint_t i, j;
    sa_sint_t sum = 0;
    for (i = BUCKETS_INDEX4(0, 0), j = 0; i <= BUCKETS_INDEX4((fast_sint_t)k - 1, 0);
         i += BUCKETS_INDEX4(1, 0), j += 1) {
        bucket_start[j] = sum;
        sum += buckets[i + BUCKETS_INDEX4(0, 0)] + buckets[i + BUCKETS_INDEX4(0, 1)] +
               buckets[i + BUCKETS_INDEX4(0, 2)] + buckets[i + BUCKETS_INDEX4(0, 3)];
        bucket_end[j] = sum;
    }
}

static void libsais_initialize_buckets_start_and_end_32s_4k(sa_sint_t k, sa_sint_t * RESTRICT buckets) {
    sa_sint_t * RESTRICT bucket_start = &buckets[2 * k];
    sa_sint_t * RESTRICT bucket_end = &buckets[3 * k];

    fast_sint_t i, j;
    sa_sint_t sum = 0;
    for (i = BUCKETS_INDEX2(0, 0), j = 0; i <= BUCKETS_INDEX2((fast_sint_t)k - 1, 0);
         i += BUCKETS_INDEX2(1, 0), j += 1) {
        bucket_start[j] = sum;
        sum += buckets[i + BUCKETS_INDEX2(0, 0)] + buckets[i + BUCKETS_INDEX2(0, 1)];
        bucket_end[j] = sum;
    }
}

static void libsais_initialize_buckets_end_32s_2k(sa_sint_t k, sa_sint_t * RESTRICT buckets) {
    fast_sint_t i;
    sa_sint_t sum0 = 0;
    for (i = BUCKETS_INDEX2(0, 0); i <= BUCKETS_INDEX2((fast_sint_t)k - 1, 0); i += BUCKETS_INDEX2(1, 0)) {
        sum0 += buckets[i + BUCKETS_INDEX2(0, 0)] + buckets[i + BUCKETS_INDEX2(0, 1)];
        buckets[i + BUCKETS_INDEX2(0, 0)] = sum0;
    }
}

static void libsais_initialize_buckets_start_and_end_32s_2k(sa_sint_t k, sa_sint_t * RESTRICT buckets) {
    fast_sint_t i, j;
    for (i = BUCKETS_INDEX2(0, 0), j = 0; i <= BUCKETS_INDEX2((fast_sint_t)k - 1, 0);
         i += BUCKETS_INDEX2(1, 0), j += 1) {
        buckets[j] = buckets[i];
    }

    buckets[k] = 0;
    memcpy(&buckets[k + 1], buckets, ((size_t)k - 1) * sizeof(sa_sint_t));
}

static void libsais_initialize_buckets_start_32s_1k(sa_sint_t k, sa_sint_t * RESTRICT buckets) {
    fast_sint_t i;
    sa_sint_t sum = 0;
    for (i = 0; i <= (fast_sint_t)k - 1; i += 1) {
        sa_sint_t tmp = buckets[i];
        buckets[i] = sum;
        sum += tmp;
    }
}

static void libsais_initialize_buckets_end_32s_1k(sa_sint_t k, sa_sint_t * RESTRICT buckets) {
    fast_sint_t i;
    sa_sint_t sum = 0;
    for (i = 0; i <= (fast_sint_t)k - 1; i += 1) {
        sum += buckets[i];
        buckets[i] = sum;
    }
}

static sa_sint_t libsais_initialize_buckets_for_lms_suffixes_radix_sort_8u(const u8 * RESTRICT T,
                                                                           sa_sint_t * RESTRICT buckets,
                                                                           sa_sint_t first_lms_suffix) {
    {
        fast_uint_t s = 0;
        fast_sint_t c0 = T[first_lms_suffix];
        fast_sint_t c1 = 0;

        for (; --first_lms_suffix >= 0;) {
            c1 = c0;
            c0 = T[first_lms_suffix];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            buckets[BUCKETS_INDEX4((fast_uint_t)c1, s & 3)]--;
        }

        buckets[BUCKETS_INDEX4((fast_uint_t)c0, (s << 1) & 3)]--;
    }

    {
        sa_sint_t * RESTRICT temp_bucket = &buckets[4 * ALPHABET_SIZE];

        fast_sint_t i, j;
        sa_sint_t sum = 0;
        for (i = BUCKETS_INDEX4(0, 0), j = BUCKETS_INDEX2(0, 0); i <= BUCKETS_INDEX4(ALPHABET_SIZE - 1, 0);
             i += BUCKETS_INDEX4(1, 0), j += BUCKETS_INDEX2(1, 0)) {
            temp_bucket[j + BUCKETS_INDEX2(0, 1)] = sum;
            sum += buckets[i + BUCKETS_INDEX4(0, 1)] + buckets[i + BUCKETS_INDEX4(0, 3)];
            temp_bucket[j] = sum;
        }

        return sum;
    }
}

static void libsais_initialize_buckets_for_lms_suffixes_radix_sort_32s_2k(const sa_sint_t * RESTRICT T, sa_sint_t k,
                                                                          sa_sint_t * RESTRICT buckets,
                                                                          sa_sint_t first_lms_suffix) {
    buckets[BUCKETS_INDEX2(T[first_lms_suffix], 0)]++;
    buckets[BUCKETS_INDEX2(T[first_lms_suffix], 1)]--;

    fast_sint_t i;
    sa_sint_t sum0 = 0, sum1 = 0;
    for (i = BUCKETS_INDEX2(0, 0); i <= BUCKETS_INDEX2((fast_sint_t)k - 1, 0); i += BUCKETS_INDEX2(1, 0)) {
        sum0 += buckets[i + BUCKETS_INDEX2(0, 0)] + buckets[i + BUCKETS_INDEX2(0, 1)];
        sum1 += buckets[i + BUCKETS_INDEX2(0, 1)];

        buckets[i + BUCKETS_INDEX2(0, 0)] = sum0;
        buckets[i + BUCKETS_INDEX2(0, 1)] = sum1;
    }
}

static sa_sint_t libsais_initialize_buckets_for_lms_suffixes_radix_sort_32s_6k(const sa_sint_t * RESTRICT T,
                                                                               sa_sint_t k,
                                                                               sa_sint_t * RESTRICT buckets,
                                                                               sa_sint_t first_lms_suffix) {
    {
        fast_uint_t s = 0;
        fast_sint_t c0 = T[first_lms_suffix];
        fast_sint_t c1 = 0;

        for (; --first_lms_suffix >= 0;) {
            c1 = c0;
            c0 = T[first_lms_suffix];
            s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
            buckets[BUCKETS_INDEX4((fast_uint_t)c1, s & 3)]--;
        }

        buckets[BUCKETS_INDEX4((fast_uint_t)c0, (s << 1) & 3)]--;
    }

    {
        sa_sint_t * RESTRICT temp_bucket = &buckets[4 * k];

        fast_sint_t i, j;
        sa_sint_t sum = 0;
        for (i = BUCKETS_INDEX4(0, 0), j = 0; i <= BUCKETS_INDEX4((fast_sint_t)k - 1, 0);
             i += BUCKETS_INDEX4(1, 0), j += 1) {
            sum += buckets[i + BUCKETS_INDEX4(0, 1)] + buckets[i + BUCKETS_INDEX4(0, 3)];
            temp_bucket[j] = sum;
        }

        return sum;
    }
}

static void libsais_initialize_buckets_for_radix_and_partial_sorting_32s_4k(const sa_sint_t * RESTRICT T, sa_sint_t k,
                                                                            sa_sint_t * RESTRICT buckets,
                                                                            sa_sint_t first_lms_suffix) {
    sa_sint_t * RESTRICT bucket_start = &buckets[2 * k];
    sa_sint_t * RESTRICT bucket_end = &buckets[3 * k];

    buckets[BUCKETS_INDEX2(T[first_lms_suffix], 0)]++;
    buckets[BUCKETS_INDEX2(T[first_lms_suffix], 1)]--;

    fast_sint_t i, j;
    sa_sint_t sum0 = 0, sum1 = 0;
    for (i = BUCKETS_INDEX2(0, 0), j = 0; i <= BUCKETS_INDEX2((fast_sint_t)k - 1, 0);
         i += BUCKETS_INDEX2(1, 0), j += 1) {
        bucket_start[j] = sum1;

        sum0 += buckets[i + BUCKETS_INDEX2(0, 1)];
        sum1 += buckets[i + BUCKETS_INDEX2(0, 0)] + buckets[i + BUCKETS_INDEX2(0, 1)];
        buckets[i + BUCKETS_INDEX2(0, 1)] = sum0;

        bucket_end[j] = sum1;
    }
}

static void libsais_radix_sort_lms_suffixes_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                               sa_sint_t * RESTRICT induction_bucket, fast_sint_t omp_block_start,
                                               fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + prefetch_distance + 3; i >= j; i -= 4) {
        prefetch(&SA[i - 2 * prefetch_distance]);

        prefetch(&T[SA[i - prefetch_distance - 0]]);
        prefetch(&T[SA[i - prefetch_distance - 1]]);
        prefetch(&T[SA[i - prefetch_distance - 2]]);
        prefetch(&T[SA[i - prefetch_distance - 3]]);

        sa_sint_t p0 = SA[i - 0];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p0], 0)]] = p0;
        sa_sint_t p1 = SA[i - 1];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p1], 0)]] = p1;
        sa_sint_t p2 = SA[i - 2];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p2], 0)]] = p2;
        sa_sint_t p3 = SA[i - 3];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p3], 0)]] = p3;
    }

    for (j -= prefetch_distance + 3; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p], 0)]] = p;
    }
}

static void libsais_radix_sort_lms_suffixes_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                   sa_sint_t m, sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                                   LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_num_threads = 1;

        if (omp_num_threads == 1) {
            libsais_radix_sort_lms_suffixes_8u(T, SA, &buckets[4 * ALPHABET_SIZE], (fast_sint_t)n - (fast_sint_t)m + 1,
                                               (fast_sint_t)m - 1);
        }
    }
}

static void libsais_radix_sort_lms_suffixes_32s_6k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                   sa_sint_t * RESTRICT induction_bucket, fast_sint_t omp_block_start,
                                                   fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + 2 * prefetch_distance + 3; i >= j; i -= 4) {
        prefetch(&SA[i - 3 * prefetch_distance]);

        prefetch(&T[SA[i - 2 * prefetch_distance - 0]]);
        prefetch(&T[SA[i - 2 * prefetch_distance - 1]]);
        prefetch(&T[SA[i - 2 * prefetch_distance - 2]]);
        prefetch(&T[SA[i - 2 * prefetch_distance - 3]]);

        prefetchw(&induction_bucket[T[SA[i - prefetch_distance - 0]]]);
        prefetchw(&induction_bucket[T[SA[i - prefetch_distance - 1]]]);
        prefetchw(&induction_bucket[T[SA[i - prefetch_distance - 2]]]);
        prefetchw(&induction_bucket[T[SA[i - prefetch_distance - 3]]]);

        sa_sint_t p0 = SA[i - 0];
        SA[--induction_bucket[T[p0]]] = p0;
        sa_sint_t p1 = SA[i - 1];
        SA[--induction_bucket[T[p1]]] = p1;
        sa_sint_t p2 = SA[i - 2];
        SA[--induction_bucket[T[p2]]] = p2;
        sa_sint_t p3 = SA[i - 3];
        SA[--induction_bucket[T[p3]]] = p3;
    }

    for (j -= 2 * prefetch_distance + 3; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        SA[--induction_bucket[T[p]]] = p;
    }
}

static void libsais_radix_sort_lms_suffixes_32s_2k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                   sa_sint_t * RESTRICT induction_bucket, fast_sint_t omp_block_start,
                                                   fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + 2 * prefetch_distance + 3; i >= j; i -= 4) {
        prefetch(&SA[i - 3 * prefetch_distance]);

        prefetch(&T[SA[i - 2 * prefetch_distance - 0]]);
        prefetch(&T[SA[i - 2 * prefetch_distance - 1]]);
        prefetch(&T[SA[i - 2 * prefetch_distance - 2]]);
        prefetch(&T[SA[i - 2 * prefetch_distance - 3]]);

        prefetchw(&induction_bucket[BUCKETS_INDEX2(T[SA[i - prefetch_distance - 0]], 0)]);
        prefetchw(&induction_bucket[BUCKETS_INDEX2(T[SA[i - prefetch_distance - 1]], 0)]);
        prefetchw(&induction_bucket[BUCKETS_INDEX2(T[SA[i - prefetch_distance - 2]], 0)]);
        prefetchw(&induction_bucket[BUCKETS_INDEX2(T[SA[i - prefetch_distance - 3]], 0)]);

        sa_sint_t p0 = SA[i - 0];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p0], 0)]] = p0;
        sa_sint_t p1 = SA[i - 1];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p1], 0)]] = p1;
        sa_sint_t p2 = SA[i - 2];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p2], 0)]] = p2;
        sa_sint_t p3 = SA[i - 3];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p3], 0)]] = p3;
    }

    for (j -= 2 * prefetch_distance + 3; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        SA[--induction_bucket[BUCKETS_INDEX2(T[p], 0)]] = p;
    }
}
static void libsais_radix_sort_lms_suffixes_32s_6k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                       sa_sint_t n, sa_sint_t m, sa_sint_t * RESTRICT induction_bucket,
                                                       sa_sint_t threads,
                                                       LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (threads == 1 || m < 65536) {
        libsais_radix_sort_lms_suffixes_32s_6k(T, SA, induction_bucket, (fast_sint_t)n - (fast_sint_t)m + 1,
                                               (fast_sint_t)m - 1);
    }
    (void)(thread_state);
}

static void libsais_radix_sort_lms_suffixes_32s_2k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                       sa_sint_t n, sa_sint_t m, sa_sint_t * RESTRICT induction_bucket,
                                                       sa_sint_t threads,
                                                       LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (threads == 1 || m < 65536) {
        libsais_radix_sort_lms_suffixes_32s_2k(T, SA, induction_bucket, (fast_sint_t)n - (fast_sint_t)m + 1,
                                               (fast_sint_t)m - 1);
    }
    (void)(thread_state);
}

static sa_sint_t libsais_radix_sort_lms_suffixes_32s_1k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                        sa_sint_t n, sa_sint_t * RESTRICT buckets) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t i = n - 2;
    sa_sint_t m = 0;
    fast_uint_t s = 1;
    fast_sint_t c0 = T[n - 1];
    fast_sint_t c1 = 0;
    fast_sint_t c2 = 0;

    for (; i >= prefetch_distance + 3; i -= 4) {
        prefetch(&T[i - 2 * prefetch_distance]);

        prefetchw(&buckets[T[i - prefetch_distance - 0]]);
        prefetchw(&buckets[T[i - prefetch_distance - 1]]);
        prefetchw(&buckets[T[i - prefetch_distance - 2]]);
        prefetchw(&buckets[T[i - prefetch_distance - 3]]);

        c1 = T[i - 0];
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        if ((s & 3) == 1) {
            SA[--buckets[c2 = c0]] = i + 1;
            m++;
        }

        c0 = T[i - 1];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        if ((s & 3) == 1) {
            SA[--buckets[c2 = c1]] = i - 0;
            m++;
        }

        c1 = T[i - 2];
        s = (s << 1) + (fast_uint_t)(c1 > (c0 - (fast_sint_t)(s & 1)));
        if ((s & 3) == 1) {
            SA[--buckets[c2 = c0]] = i - 1;
            m++;
        }

        c0 = T[i - 3];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        if ((s & 3) == 1) {
            SA[--buckets[c2 = c1]] = i - 2;
            m++;
        }
    }

    for (; i >= 0; i -= 1) {
        c1 = c0;
        c0 = T[i];
        s = (s << 1) + (fast_uint_t)(c0 > (c1 - (fast_sint_t)(s & 1)));
        if ((s & 3) == 1) {
            SA[--buckets[c2 = c1]] = i + 1;
            m++;
        }
    }

    if (m > 1) {
        SA[buckets[c2]] = 0;
    }

    return m;
}

static void libsais_radix_sort_set_markers_32s_6k(sa_sint_t * RESTRICT SA, sa_sint_t * RESTRICT induction_bucket,
                                                  fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 3; i < j; i += 4) {
        prefetch(&induction_bucket[i + 2 * prefetch_distance]);

        prefetchw(&SA[induction_bucket[i + prefetch_distance + 0]]);
        prefetchw(&SA[induction_bucket[i + prefetch_distance + 1]]);
        prefetchw(&SA[induction_bucket[i + prefetch_distance + 2]]);
        prefetchw(&SA[induction_bucket[i + prefetch_distance + 3]]);

        SA[induction_bucket[i + 0]] |= SAINT_MIN;
        SA[induction_bucket[i + 1]] |= SAINT_MIN;
        SA[induction_bucket[i + 2]] |= SAINT_MIN;
        SA[induction_bucket[i + 3]] |= SAINT_MIN;
    }

    for (j += prefetch_distance + 3; i < j; i += 1) {
        SA[induction_bucket[i]] |= SAINT_MIN;
    }
}

static void libsais_radix_sort_set_markers_32s_4k(sa_sint_t * RESTRICT SA, sa_sint_t * RESTRICT induction_bucket,
                                                  fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 3; i < j; i += 4) {
        prefetch(&induction_bucket[BUCKETS_INDEX2(i + 2 * prefetch_distance, 0)]);

        prefetchw(&SA[induction_bucket[BUCKETS_INDEX2(i + prefetch_distance + 0, 0)]]);
        prefetchw(&SA[induction_bucket[BUCKETS_INDEX2(i + prefetch_distance + 1, 0)]]);
        prefetchw(&SA[induction_bucket[BUCKETS_INDEX2(i + prefetch_distance + 2, 0)]]);
        prefetchw(&SA[induction_bucket[BUCKETS_INDEX2(i + prefetch_distance + 3, 0)]]);

        SA[induction_bucket[BUCKETS_INDEX2(i + 0, 0)]] |= SUFFIX_GROUP_MARKER;
        SA[induction_bucket[BUCKETS_INDEX2(i + 1, 0)]] |= SUFFIX_GROUP_MARKER;
        SA[induction_bucket[BUCKETS_INDEX2(i + 2, 0)]] |= SUFFIX_GROUP_MARKER;
        SA[induction_bucket[BUCKETS_INDEX2(i + 3, 0)]] |= SUFFIX_GROUP_MARKER;
    }

    for (j += prefetch_distance + 3; i < j; i += 1) {
        SA[induction_bucket[BUCKETS_INDEX2(i, 0)]] |= SUFFIX_GROUP_MARKER;
    }
}

static void libsais_radix_sort_set_markers_32s_6k_omp(sa_sint_t * RESTRICT SA, sa_sint_t k,
                                                      sa_sint_t * RESTRICT induction_bucket, sa_sint_t threads) {
    {
        (void)(threads);

        fast_sint_t omp_block_start = 0;
        fast_sint_t omp_block_size = (fast_sint_t)k - 1;
        libsais_radix_sort_set_markers_32s_6k(SA, induction_bucket, omp_block_start, omp_block_size);
    }
}

static void libsais_radix_sort_set_markers_32s_4k_omp(sa_sint_t * RESTRICT SA, sa_sint_t k,
                                                      sa_sint_t * RESTRICT induction_bucket, sa_sint_t threads) {
    {
        (void)(threads);

        fast_sint_t omp_block_start = 0;
        fast_sint_t omp_block_size = (fast_sint_t)k - 1;
        libsais_radix_sort_set_markers_32s_4k(SA, induction_bucket, omp_block_start, omp_block_size);
    }
}

static void libsais_initialize_buckets_for_partial_sorting_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT buckets,
                                                              sa_sint_t first_lms_suffix,
                                                              sa_sint_t left_suffixes_count) {
    sa_sint_t * RESTRICT temp_bucket = &buckets[4 * ALPHABET_SIZE];

    buckets[BUCKETS_INDEX4((fast_uint_t)T[first_lms_suffix], 1)]++;

    fast_sint_t i, j;
    sa_sint_t sum0 = left_suffixes_count + 1, sum1 = 0;
    for (i = BUCKETS_INDEX4(0, 0), j = BUCKETS_INDEX2(0, 0); i <= BUCKETS_INDEX4(ALPHABET_SIZE - 1, 0);
         i += BUCKETS_INDEX4(1, 0), j += BUCKETS_INDEX2(1, 0)) {
        temp_bucket[j + BUCKETS_INDEX2(0, 0)] = sum0;

        sum0 += buckets[i + BUCKETS_INDEX4(0, 0)] + buckets[i + BUCKETS_INDEX4(0, 2)];
        sum1 += buckets[i + BUCKETS_INDEX4(0, 1)];

        buckets[j + BUCKETS_INDEX2(0, 0)] = sum0;
        buckets[j + BUCKETS_INDEX2(0, 1)] = sum1;
    }
}

static void libsais_initialize_buckets_for_partial_sorting_32s_6k(const sa_sint_t * RESTRICT T, sa_sint_t k,
                                                                  sa_sint_t * RESTRICT buckets,
                                                                  sa_sint_t first_lms_suffix,
                                                                  sa_sint_t left_suffixes_count) {
    sa_sint_t * RESTRICT temp_bucket = &buckets[4 * k];

    fast_sint_t i, j;
    sa_sint_t sum0 = left_suffixes_count + 1, sum1 = 0, sum2 = 0;
    for (first_lms_suffix = T[first_lms_suffix], i = BUCKETS_INDEX4(0, 0), j = BUCKETS_INDEX2(0, 0);
         i <= BUCKETS_INDEX4((fast_sint_t)first_lms_suffix - 1, 0);
         i += BUCKETS_INDEX4(1, 0), j += BUCKETS_INDEX2(1, 0)) {
        sa_sint_t SS = buckets[i + BUCKETS_INDEX4(0, 0)];
        sa_sint_t LS = buckets[i + BUCKETS_INDEX4(0, 1)];
        sa_sint_t SL = buckets[i + BUCKETS_INDEX4(0, 2)];
        sa_sint_t LL = buckets[i + BUCKETS_INDEX4(0, 3)];

        buckets[i + BUCKETS_INDEX4(0, 0)] = sum0;
        buckets[i + BUCKETS_INDEX4(0, 1)] = sum2;
        buckets[i + BUCKETS_INDEX4(0, 2)] = 0;
        buckets[i + BUCKETS_INDEX4(0, 3)] = 0;

        sum0 += SS + SL;
        sum1 += LS;
        sum2 += LS + LL;

        temp_bucket[j + BUCKETS_INDEX2(0, 0)] = sum0;
        temp_bucket[j + BUCKETS_INDEX2(0, 1)] = sum1;
    }

    for (sum1 += 1; i <= BUCKETS_INDEX4((fast_sint_t)k - 1, 0); i += BUCKETS_INDEX4(1, 0), j += BUCKETS_INDEX2(1, 0)) {
        sa_sint_t SS = buckets[i + BUCKETS_INDEX4(0, 0)];
        sa_sint_t LS = buckets[i + BUCKETS_INDEX4(0, 1)];
        sa_sint_t SL = buckets[i + BUCKETS_INDEX4(0, 2)];
        sa_sint_t LL = buckets[i + BUCKETS_INDEX4(0, 3)];

        buckets[i + BUCKETS_INDEX4(0, 0)] = sum0;
        buckets[i + BUCKETS_INDEX4(0, 1)] = sum2;
        buckets[i + BUCKETS_INDEX4(0, 2)] = 0;
        buckets[i + BUCKETS_INDEX4(0, 3)] = 0;

        sum0 += SS + SL;
        sum1 += LS;
        sum2 += LS + LL;

        temp_bucket[j + BUCKETS_INDEX2(0, 0)] = sum0;
        temp_bucket[j + BUCKETS_INDEX2(0, 1)] = sum1;
    }
}

static sa_sint_t libsais_partial_sorting_scan_left_to_right_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                               sa_sint_t * RESTRICT buckets, sa_sint_t d,
                                                               fast_sint_t omp_block_start,
                                                               fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT induction_bucket = &buckets[4 * ALPHABET_SIZE];
    sa_sint_t * RESTRICT distinct_names = &buckets[2 * ALPHABET_SIZE];

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 1; i < j; i += 2) {
        prefetch(&SA[i + 2 * prefetch_distance]);

        prefetch(&T[SA[i + prefetch_distance + 0] & SAINT_MAX] - 1);
        prefetch(&T[SA[i + prefetch_distance + 0] & SAINT_MAX] - 2);
        prefetch(&T[SA[i + prefetch_distance + 1] & SAINT_MAX] - 1);
        prefetch(&T[SA[i + prefetch_distance + 1] & SAINT_MAX] - 2);

        sa_sint_t p0 = SA[i + 0];
        d += (p0 < 0);
        p0 &= SAINT_MAX;
        sa_sint_t v0 = BUCKETS_INDEX2(T[p0 - 1], T[p0 - 2] >= T[p0 - 1]);
        SA[induction_bucket[v0]++] = (p0 - 1) | ((sa_sint_t)(distinct_names[v0] != d) << (SAINT_BIT - 1));
        distinct_names[v0] = d;

        sa_sint_t p1 = SA[i + 1];
        d += (p1 < 0);
        p1 &= SAINT_MAX;
        sa_sint_t v1 = BUCKETS_INDEX2(T[p1 - 1], T[p1 - 2] >= T[p1 - 1]);
        SA[induction_bucket[v1]++] = (p1 - 1) | ((sa_sint_t)(distinct_names[v1] != d) << (SAINT_BIT - 1));
        distinct_names[v1] = d;
    }

    for (j += prefetch_distance + 1; i < j; i += 1) {
        sa_sint_t p = SA[i];
        d += (p < 0);
        p &= SAINT_MAX;
        sa_sint_t v = BUCKETS_INDEX2(T[p - 1], T[p - 2] >= T[p - 1]);
        SA[induction_bucket[v]++] = (p - 1) | ((sa_sint_t)(distinct_names[v] != d) << (SAINT_BIT - 1));
        distinct_names[v] = d;
    }

    return d;
}
static sa_sint_t libsais_partial_sorting_scan_left_to_right_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                                   sa_sint_t n, sa_sint_t * RESTRICT buckets,
                                                                   sa_sint_t left_suffixes_count, sa_sint_t d,
                                                                   sa_sint_t threads,
                                                                   LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t * RESTRICT induction_bucket = &buckets[4 * ALPHABET_SIZE];
    sa_sint_t * RESTRICT distinct_names = &buckets[2 * ALPHABET_SIZE];

    SA[induction_bucket[BUCKETS_INDEX2(T[n - 1], T[n - 2] >= T[n - 1])]++] = (n - 1) | SAINT_MIN;
    distinct_names[BUCKETS_INDEX2(T[n - 1], T[n - 2] >= T[n - 1])] = ++d;

    if (threads == 1 || left_suffixes_count < 65536) {
        d = libsais_partial_sorting_scan_left_to_right_8u(T, SA, buckets, d, 0, left_suffixes_count);
    }
    (void)(thread_state);
    return d;
}

static sa_sint_t libsais_partial_sorting_scan_left_to_right_32s_6k(const sa_sint_t * RESTRICT T,
                                                                   sa_sint_t * RESTRICT SA,
                                                                   sa_sint_t * RESTRICT buckets, sa_sint_t d,
                                                                   fast_sint_t omp_block_start,
                                                                   fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - 2 * prefetch_distance - 1; i < j; i += 2) {
        prefetch(&SA[i + 3 * prefetch_distance]);

        prefetch(&T[SA[i + 2 * prefetch_distance + 0] & SAINT_MAX] - 1);
        prefetch(&T[SA[i + 2 * prefetch_distance + 0] & SAINT_MAX] - 2);
        prefetch(&T[SA[i + 2 * prefetch_distance + 1] & SAINT_MAX] - 1);
        prefetch(&T[SA[i + 2 * prefetch_distance + 1] & SAINT_MAX] - 2);

        sa_sint_t p0 = SA[i + prefetch_distance + 0] & SAINT_MAX;
        sa_sint_t v0 = BUCKETS_INDEX4(T[p0 - (p0 > 0)], 0);
        prefetchw(&buckets[v0]);
        sa_sint_t p1 = SA[i + prefetch_distance + 1] & SAINT_MAX;
        sa_sint_t v1 = BUCKETS_INDEX4(T[p1 - (p1 > 0)], 0);
        prefetchw(&buckets[v1]);

        sa_sint_t p2 = SA[i + 0];
        d += (p2 < 0);
        p2 &= SAINT_MAX;
        sa_sint_t v2 = BUCKETS_INDEX4(T[p2 - 1], T[p2 - 2] >= T[p2 - 1]);
        SA[buckets[v2]++] = (p2 - 1) | ((sa_sint_t)(buckets[2 + v2] != d) << (SAINT_BIT - 1));
        buckets[2 + v2] = d;

        sa_sint_t p3 = SA[i + 1];
        d += (p3 < 0);
        p3 &= SAINT_MAX;
        sa_sint_t v3 = BUCKETS_INDEX4(T[p3 - 1], T[p3 - 2] >= T[p3 - 1]);
        SA[buckets[v3]++] = (p3 - 1) | ((sa_sint_t)(buckets[2 + v3] != d) << (SAINT_BIT - 1));
        buckets[2 + v3] = d;
    }

    for (j += 2 * prefetch_distance + 1; i < j; i += 1) {
        sa_sint_t p = SA[i];
        d += (p < 0);
        p &= SAINT_MAX;
        sa_sint_t v = BUCKETS_INDEX4(T[p - 1], T[p - 2] >= T[p - 1]);
        SA[buckets[v]++] = (p - 1) | ((sa_sint_t)(buckets[2 + v] != d) << (SAINT_BIT - 1));
        buckets[2 + v] = d;
    }

    return d;
}

static sa_sint_t libsais_partial_sorting_scan_left_to_right_32s_4k(const sa_sint_t * RESTRICT T,
                                                                   sa_sint_t * RESTRICT SA, sa_sint_t k,
                                                                   sa_sint_t * RESTRICT buckets, sa_sint_t d,
                                                                   fast_sint_t omp_block_start,
                                                                   fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT induction_bucket = &buckets[2 * k];
    sa_sint_t * RESTRICT distinct_names = &buckets[0 * k];

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - 2 * prefetch_distance - 1; i < j; i += 2) {
        prefetchw(&SA[i + 3 * prefetch_distance]);

        sa_sint_t s0 = SA[i + 2 * prefetch_distance + 0];
        const sa_sint_t * Ts0 = &T[s0 & ~SUFFIX_GROUP_MARKER] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        Ts0--;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i + 2 * prefetch_distance + 1];
        const sa_sint_t * Ts1 = &T[s1 & ~SUFFIX_GROUP_MARKER] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        Ts1--;
        prefetch(s1 > 0 ? Ts1 : NULL);
        sa_sint_t s2 = SA[i + 1 * prefetch_distance + 0];
        if (s2 > 0) {
            const fast_sint_t Ts2 = T[(s2 & ~SUFFIX_GROUP_MARKER) - 1];
            prefetchw(&induction_bucket[Ts2]);
            prefetchw(&distinct_names[BUCKETS_INDEX2(Ts2, 0)]);
        }
        sa_sint_t s3 = SA[i + 1 * prefetch_distance + 1];
        if (s3 > 0) {
            const fast_sint_t Ts3 = T[(s3 & ~SUFFIX_GROUP_MARKER) - 1];
            prefetchw(&induction_bucket[Ts3]);
            prefetchw(&distinct_names[BUCKETS_INDEX2(Ts3, 0)]);
        }

        sa_sint_t p0 = SA[i + 0];
        SA[i + 0] = p0 & SAINT_MAX;
        if (p0 > 0) {
            SA[i + 0] = 0;
            d += (p0 >> (SUFFIX_GROUP_BIT - 1));
            p0 &= ~SUFFIX_GROUP_MARKER;
            sa_sint_t v0 = BUCKETS_INDEX2(T[p0 - 1], T[p0 - 2] < T[p0 - 1]);
            SA[induction_bucket[T[p0 - 1]]++] = (p0 - 1) | ((sa_sint_t)(T[p0 - 2] < T[p0 - 1]) << (SAINT_BIT - 1)) |
                                                ((sa_sint_t)(distinct_names[v0] != d) << (SUFFIX_GROUP_BIT - 1));
            distinct_names[v0] = d;
        }

        sa_sint_t p1 = SA[i + 1];
        SA[i + 1] = p1 & SAINT_MAX;
        if (p1 > 0) {
            SA[i + 1] = 0;
            d += (p1 >> (SUFFIX_GROUP_BIT - 1));
            p1 &= ~SUFFIX_GROUP_MARKER;
            sa_sint_t v1 = BUCKETS_INDEX2(T[p1 - 1], T[p1 - 2] < T[p1 - 1]);
            SA[induction_bucket[T[p1 - 1]]++] = (p1 - 1) | ((sa_sint_t)(T[p1 - 2] < T[p1 - 1]) << (SAINT_BIT - 1)) |
                                                ((sa_sint_t)(distinct_names[v1] != d) << (SUFFIX_GROUP_BIT - 1));
            distinct_names[v1] = d;
        }
    }

    for (j += 2 * prefetch_distance + 1; i < j; i += 1) {
        sa_sint_t p = SA[i];
        SA[i] = p & SAINT_MAX;
        if (p > 0) {
            SA[i] = 0;
            d += (p >> (SUFFIX_GROUP_BIT - 1));
            p &= ~SUFFIX_GROUP_MARKER;
            sa_sint_t v = BUCKETS_INDEX2(T[p - 1], T[p - 2] < T[p - 1]);
            SA[induction_bucket[T[p - 1]]++] = (p - 1) | ((sa_sint_t)(T[p - 2] < T[p - 1]) << (SAINT_BIT - 1)) |
                                               ((sa_sint_t)(distinct_names[v] != d) << (SUFFIX_GROUP_BIT - 1));
            distinct_names[v] = d;
        }
    }

    return d;
}

static void libsais_partial_sorting_scan_left_to_right_32s_1k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                              sa_sint_t * RESTRICT induction_bucket,
                                                              fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - 2 * prefetch_distance - 1; i < j; i += 2) {
        prefetchw(&SA[i + 3 * prefetch_distance]);

        sa_sint_t s0 = SA[i + 2 * prefetch_distance + 0];
        const sa_sint_t * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i + 2 * prefetch_distance + 1];
        const sa_sint_t * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        sa_sint_t s2 = SA[i + 1 * prefetch_distance + 0];
        if (s2 > 0) {
            prefetchw(&induction_bucket[T[s2 - 1]]);
            prefetch(&T[s2] - 2);
        }
        sa_sint_t s3 = SA[i + 1 * prefetch_distance + 1];
        if (s3 > 0) {
            prefetchw(&induction_bucket[T[s3 - 1]]);
            prefetch(&T[s3] - 2);
        }

        sa_sint_t p0 = SA[i + 0];
        SA[i + 0] = p0 & SAINT_MAX;
        if (p0 > 0) {
            SA[i + 0] = 0;
            SA[induction_bucket[T[p0 - 1]]++] = (p0 - 1) | ((sa_sint_t)(T[p0 - 2] < T[p0 - 1]) << (SAINT_BIT - 1));
        }
        sa_sint_t p1 = SA[i + 1];
        SA[i + 1] = p1 & SAINT_MAX;
        if (p1 > 0) {
            SA[i + 1] = 0;
            SA[induction_bucket[T[p1 - 1]]++] = (p1 - 1) | ((sa_sint_t)(T[p1 - 2] < T[p1 - 1]) << (SAINT_BIT - 1));
        }
    }

    for (j += 2 * prefetch_distance + 1; i < j; i += 1) {
        sa_sint_t p = SA[i];
        SA[i] = p & SAINT_MAX;
        if (p > 0) {
            SA[i] = 0;
            SA[induction_bucket[T[p - 1]]++] = (p - 1) | ((sa_sint_t)(T[p - 2] < T[p - 1]) << (SAINT_BIT - 1));
        }
    }
}
static sa_sint_t libsais_partial_sorting_scan_left_to_right_32s_6k_omp(
    const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t * RESTRICT buckets,
    sa_sint_t left_suffixes_count, sa_sint_t d, sa_sint_t threads, LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    SA[buckets[BUCKETS_INDEX4(T[n - 1], T[n - 2] >= T[n - 1])]++] = (n - 1) | SAINT_MIN;
    buckets[2 + BUCKETS_INDEX4(T[n - 1], T[n - 2] >= T[n - 1])] = ++d;

    if (threads == 1 || left_suffixes_count < 65536) {
        d = libsais_partial_sorting_scan_left_to_right_32s_6k(T, SA, buckets, d, 0, left_suffixes_count);
    }
    (void)(thread_state);
    return d;
}

static sa_sint_t libsais_partial_sorting_scan_left_to_right_32s_4k_omp(const sa_sint_t * RESTRICT T,
                                                                       sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                       sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                                       sa_sint_t d, sa_sint_t threads,
                                                                       LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t * RESTRICT induction_bucket = &buckets[2 * k];
    sa_sint_t * RESTRICT distinct_names = &buckets[0 * k];

    SA[induction_bucket[T[n - 1]]++] =
        (n - 1) | ((sa_sint_t)(T[n - 2] < T[n - 1]) << (SAINT_BIT - 1)) | SUFFIX_GROUP_MARKER;
    distinct_names[BUCKETS_INDEX2(T[n - 1], T[n - 2] < T[n - 1])] = ++d;

    if (threads == 1 || n < 65536) {
        d = libsais_partial_sorting_scan_left_to_right_32s_4k(T, SA, k, buckets, d, 0, n);
    }
    (void)(thread_state);
    return d;
}

static void libsais_partial_sorting_scan_left_to_right_32s_1k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                                  sa_sint_t n, sa_sint_t * RESTRICT buckets,
                                                                  sa_sint_t threads,
                                                                  LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    SA[buckets[T[n - 1]]++] = (n - 1) | ((sa_sint_t)(T[n - 2] < T[n - 1]) << (SAINT_BIT - 1));

    if (threads == 1 || n < 65536) {
        libsais_partial_sorting_scan_left_to_right_32s_1k(T, SA, buckets, 0, n);
    }
    (void)(thread_state);
}

static void libsais_partial_sorting_shift_markers_8u_omp(sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                         const sa_sint_t * RESTRICT buckets, sa_sint_t threads) {
    const fast_sint_t prefetch_distance = 32;

    const sa_sint_t * RESTRICT temp_bucket = &buckets[4 * ALPHABET_SIZE];

    fast_sint_t c;
    (void)(threads);
    (void)(n);

    for (c = BUCKETS_INDEX2(ALPHABET_SIZE - 1, 0); c >= BUCKETS_INDEX2(1, 0); c -= BUCKETS_INDEX2(1, 0)) {
        fast_sint_t i, j;
        sa_sint_t s = SAINT_MIN;
        for (i = (fast_sint_t)temp_bucket[c] - 1, j = (fast_sint_t)buckets[c - BUCKETS_INDEX2(1, 0)] + 3; i >= j;
             i -= 4) {
            prefetchw(&SA[i - prefetch_distance]);

            sa_sint_t p0 = SA[i - 0], q0 = (p0 & SAINT_MIN) ^ s;
            s = s ^ q0;
            SA[i - 0] = p0 ^ q0;
            sa_sint_t p1 = SA[i - 1], q1 = (p1 & SAINT_MIN) ^ s;
            s = s ^ q1;
            SA[i - 1] = p1 ^ q1;
            sa_sint_t p2 = SA[i - 2], q2 = (p2 & SAINT_MIN) ^ s;
            s = s ^ q2;
            SA[i - 2] = p2 ^ q2;
            sa_sint_t p3 = SA[i - 3], q3 = (p3 & SAINT_MIN) ^ s;
            s = s ^ q3;
            SA[i - 3] = p3 ^ q3;
        }

        for (j -= 3; i >= j; i -= 1) {
            sa_sint_t p = SA[i], q = (p & SAINT_MIN) ^ s;
            s = s ^ q;
            SA[i] = p ^ q;
        }
    }
}

static void libsais_partial_sorting_shift_markers_32s_6k_omp(sa_sint_t * RESTRICT SA, sa_sint_t k,
                                                             const sa_sint_t * RESTRICT buckets, sa_sint_t threads) {
    const fast_sint_t prefetch_distance = 32;

    const sa_sint_t * RESTRICT temp_bucket = &buckets[4 * k];

    fast_sint_t c;
    (void)(threads);

    for (c = (fast_sint_t)k - 1; c >= 1; c -= 1) {
        fast_sint_t i, j;
        sa_sint_t s = SAINT_MIN;
        for (i = (fast_sint_t)buckets[BUCKETS_INDEX4(c, 0)] - 1,
            j = (fast_sint_t)temp_bucket[BUCKETS_INDEX2(c - 1, 0)] + 3;
             i >= j; i -= 4) {
            prefetchw(&SA[i - prefetch_distance]);

            sa_sint_t p0 = SA[i - 0], q0 = (p0 & SAINT_MIN) ^ s;
            s = s ^ q0;
            SA[i - 0] = p0 ^ q0;
            sa_sint_t p1 = SA[i - 1], q1 = (p1 & SAINT_MIN) ^ s;
            s = s ^ q1;
            SA[i - 1] = p1 ^ q1;
            sa_sint_t p2 = SA[i - 2], q2 = (p2 & SAINT_MIN) ^ s;
            s = s ^ q2;
            SA[i - 2] = p2 ^ q2;
            sa_sint_t p3 = SA[i - 3], q3 = (p3 & SAINT_MIN) ^ s;
            s = s ^ q3;
            SA[i - 3] = p3 ^ q3;
        }

        for (j -= 3; i >= j; i -= 1) {
            sa_sint_t p = SA[i], q = (p & SAINT_MIN) ^ s;
            s = s ^ q;
            SA[i] = p ^ q;
        }
    }
}

static void libsais_partial_sorting_shift_markers_32s_4k(sa_sint_t * RESTRICT SA, sa_sint_t n) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i;
    sa_sint_t s = SUFFIX_GROUP_MARKER;
    for (i = (fast_sint_t)n - 1; i >= 3; i -= 4) {
        prefetchw(&SA[i - prefetch_distance]);

        sa_sint_t p0 = SA[i - 0],
                  q0 = ((p0 & SUFFIX_GROUP_MARKER) ^ s) & ((sa_sint_t)(p0 > 0) << ((SUFFIX_GROUP_BIT - 1)));
        s = s ^ q0;
        SA[i - 0] = p0 ^ q0;
        sa_sint_t p1 = SA[i - 1],
                  q1 = ((p1 & SUFFIX_GROUP_MARKER) ^ s) & ((sa_sint_t)(p1 > 0) << ((SUFFIX_GROUP_BIT - 1)));
        s = s ^ q1;
        SA[i - 1] = p1 ^ q1;
        sa_sint_t p2 = SA[i - 2],
                  q2 = ((p2 & SUFFIX_GROUP_MARKER) ^ s) & ((sa_sint_t)(p2 > 0) << ((SUFFIX_GROUP_BIT - 1)));
        s = s ^ q2;
        SA[i - 2] = p2 ^ q2;
        sa_sint_t p3 = SA[i - 3],
                  q3 = ((p3 & SUFFIX_GROUP_MARKER) ^ s) & ((sa_sint_t)(p3 > 0) << ((SUFFIX_GROUP_BIT - 1)));
        s = s ^ q3;
        SA[i - 3] = p3 ^ q3;
    }

    for (; i >= 0; i -= 1) {
        sa_sint_t p = SA[i], q = ((p & SUFFIX_GROUP_MARKER) ^ s) & ((sa_sint_t)(p > 0) << ((SUFFIX_GROUP_BIT - 1)));
        s = s ^ q;
        SA[i] = p ^ q;
    }
}

static void libsais_partial_sorting_shift_buckets_32s_6k(sa_sint_t k, sa_sint_t * RESTRICT buckets) {
    sa_sint_t * RESTRICT temp_bucket = &buckets[4 * k];

    fast_sint_t i;
    for (i = BUCKETS_INDEX2(0, 0); i <= BUCKETS_INDEX2((fast_sint_t)k - 1, 0); i += BUCKETS_INDEX2(1, 0)) {
        buckets[2 * i + BUCKETS_INDEX4(0, 0)] = temp_bucket[i + BUCKETS_INDEX2(0, 0)];
        buckets[2 * i + BUCKETS_INDEX4(0, 1)] = temp_bucket[i + BUCKETS_INDEX2(0, 1)];
    }
}

static sa_sint_t libsais_partial_sorting_scan_right_to_left_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                               sa_sint_t * RESTRICT buckets, sa_sint_t d,
                                                               fast_sint_t omp_block_start,
                                                               fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT induction_bucket = &buckets[0 * ALPHABET_SIZE];
    sa_sint_t * RESTRICT distinct_names = &buckets[2 * ALPHABET_SIZE];

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + prefetch_distance + 1; i >= j; i -= 2) {
        prefetch(&SA[i - 2 * prefetch_distance]);

        prefetch(&T[SA[i - prefetch_distance - 0] & SAINT_MAX] - 1);
        prefetch(&T[SA[i - prefetch_distance - 0] & SAINT_MAX] - 2);
        prefetch(&T[SA[i - prefetch_distance - 1] & SAINT_MAX] - 1);
        prefetch(&T[SA[i - prefetch_distance - 1] & SAINT_MAX] - 2);

        sa_sint_t p0 = SA[i - 0];
        d += (p0 < 0);
        p0 &= SAINT_MAX;
        sa_sint_t v0 = BUCKETS_INDEX2(T[p0 - 1], T[p0 - 2] > T[p0 - 1]);
        SA[--induction_bucket[v0]] = (p0 - 1) | ((sa_sint_t)(distinct_names[v0] != d) << (SAINT_BIT - 1));
        distinct_names[v0] = d;

        sa_sint_t p1 = SA[i - 1];
        d += (p1 < 0);
        p1 &= SAINT_MAX;
        sa_sint_t v1 = BUCKETS_INDEX2(T[p1 - 1], T[p1 - 2] > T[p1 - 1]);
        SA[--induction_bucket[v1]] = (p1 - 1) | ((sa_sint_t)(distinct_names[v1] != d) << (SAINT_BIT - 1));
        distinct_names[v1] = d;
    }

    for (j -= prefetch_distance + 1; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        d += (p < 0);
        p &= SAINT_MAX;
        sa_sint_t v = BUCKETS_INDEX2(T[p - 1], T[p - 2] > T[p - 1]);
        SA[--induction_bucket[v]] = (p - 1) | ((sa_sint_t)(distinct_names[v] != d) << (SAINT_BIT - 1));
        distinct_names[v] = d;
    }

    return d;
}
static void libsais_partial_sorting_scan_right_to_left_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                              sa_sint_t n, sa_sint_t * RESTRICT buckets,
                                                              sa_sint_t first_lms_suffix, sa_sint_t left_suffixes_count,
                                                              sa_sint_t d, sa_sint_t threads,
                                                              LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    fast_sint_t scan_start = (fast_sint_t)left_suffixes_count + 1;
    fast_sint_t scan_end = (fast_sint_t)n - (fast_sint_t)first_lms_suffix;

    if (threads == 1 || (scan_end - scan_start) < 65536) {
        libsais_partial_sorting_scan_right_to_left_8u(T, SA, buckets, d, scan_start, scan_end - scan_start);
    }
    (void)(thread_state);
}

static sa_sint_t libsais_partial_sorting_scan_right_to_left_32s_6k(const sa_sint_t * RESTRICT T,
                                                                   sa_sint_t * RESTRICT SA,
                                                                   sa_sint_t * RESTRICT buckets, sa_sint_t d,
                                                                   fast_sint_t omp_block_start,
                                                                   fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + 2 * prefetch_distance + 1; i >= j; i -= 2) {
        prefetch(&SA[i - 3 * prefetch_distance]);

        prefetch(&T[SA[i - 2 * prefetch_distance - 0] & SAINT_MAX] - 1);
        prefetch(&T[SA[i - 2 * prefetch_distance - 0] & SAINT_MAX] - 2);
        prefetch(&T[SA[i - 2 * prefetch_distance - 1] & SAINT_MAX] - 1);
        prefetch(&T[SA[i - 2 * prefetch_distance - 1] & SAINT_MAX] - 2);

        sa_sint_t p0 = SA[i - prefetch_distance - 0] & SAINT_MAX;
        sa_sint_t v0 = BUCKETS_INDEX4(T[p0 - (p0 > 0)], 0);
        prefetchw(&buckets[v0]);
        sa_sint_t p1 = SA[i - prefetch_distance - 1] & SAINT_MAX;
        sa_sint_t v1 = BUCKETS_INDEX4(T[p1 - (p1 > 0)], 0);
        prefetchw(&buckets[v1]);

        sa_sint_t p2 = SA[i - 0];
        d += (p2 < 0);
        p2 &= SAINT_MAX;
        sa_sint_t v2 = BUCKETS_INDEX4(T[p2 - 1], T[p2 - 2] > T[p2 - 1]);
        SA[--buckets[v2]] = (p2 - 1) | ((sa_sint_t)(buckets[2 + v2] != d) << (SAINT_BIT - 1));
        buckets[2 + v2] = d;

        sa_sint_t p3 = SA[i - 1];
        d += (p3 < 0);
        p3 &= SAINT_MAX;
        sa_sint_t v3 = BUCKETS_INDEX4(T[p3 - 1], T[p3 - 2] > T[p3 - 1]);
        SA[--buckets[v3]] = (p3 - 1) | ((sa_sint_t)(buckets[2 + v3] != d) << (SAINT_BIT - 1));
        buckets[2 + v3] = d;
    }

    for (j -= 2 * prefetch_distance + 1; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        d += (p < 0);
        p &= SAINT_MAX;
        sa_sint_t v = BUCKETS_INDEX4(T[p - 1], T[p - 2] > T[p - 1]);
        SA[--buckets[v]] = (p - 1) | ((sa_sint_t)(buckets[2 + v] != d) << (SAINT_BIT - 1));
        buckets[2 + v] = d;
    }

    return d;
}

static sa_sint_t libsais_partial_sorting_scan_right_to_left_32s_4k(const sa_sint_t * RESTRICT T,
                                                                   sa_sint_t * RESTRICT SA, sa_sint_t k,
                                                                   sa_sint_t * RESTRICT buckets, sa_sint_t d,
                                                                   fast_sint_t omp_block_start,
                                                                   fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT induction_bucket = &buckets[3 * k];
    sa_sint_t * RESTRICT distinct_names = &buckets[0 * k];

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + 2 * prefetch_distance + 1; i >= j; i -= 2) {
        prefetchw(&SA[i - 3 * prefetch_distance]);

        sa_sint_t s0 = SA[i - 2 * prefetch_distance - 0];
        const sa_sint_t * Ts0 = &T[s0 & ~SUFFIX_GROUP_MARKER] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        Ts0--;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i - 2 * prefetch_distance - 1];
        const sa_sint_t * Ts1 = &T[s1 & ~SUFFIX_GROUP_MARKER] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        Ts1--;
        prefetch(s1 > 0 ? Ts1 : NULL);
        sa_sint_t s2 = SA[i - 1 * prefetch_distance - 0];
        if (s2 > 0) {
            const fast_sint_t Ts2 = T[(s2 & ~SUFFIX_GROUP_MARKER) - 1];
            prefetchw(&induction_bucket[Ts2]);
            prefetchw(&distinct_names[BUCKETS_INDEX2(Ts2, 0)]);
        }
        sa_sint_t s3 = SA[i - 1 * prefetch_distance - 1];
        if (s3 > 0) {
            const fast_sint_t Ts3 = T[(s3 & ~SUFFIX_GROUP_MARKER) - 1];
            prefetchw(&induction_bucket[Ts3]);
            prefetchw(&distinct_names[BUCKETS_INDEX2(Ts3, 0)]);
        }

        sa_sint_t p0 = SA[i - 0];
        if (p0 > 0) {
            SA[i - 0] = 0;
            d += (p0 >> (SUFFIX_GROUP_BIT - 1));
            p0 &= ~SUFFIX_GROUP_MARKER;
            sa_sint_t v0 = BUCKETS_INDEX2(T[p0 - 1], T[p0 - 2] > T[p0 - 1]);
            SA[--induction_bucket[T[p0 - 1]]] = (p0 - 1) | ((sa_sint_t)(T[p0 - 2] > T[p0 - 1]) << (SAINT_BIT - 1)) |
                                                ((sa_sint_t)(distinct_names[v0] != d) << (SUFFIX_GROUP_BIT - 1));
            distinct_names[v0] = d;
        }

        sa_sint_t p1 = SA[i - 1];
        if (p1 > 0) {
            SA[i - 1] = 0;
            d += (p1 >> (SUFFIX_GROUP_BIT - 1));
            p1 &= ~SUFFIX_GROUP_MARKER;
            sa_sint_t v1 = BUCKETS_INDEX2(T[p1 - 1], T[p1 - 2] > T[p1 - 1]);
            SA[--induction_bucket[T[p1 - 1]]] = (p1 - 1) | ((sa_sint_t)(T[p1 - 2] > T[p1 - 1]) << (SAINT_BIT - 1)) |
                                                ((sa_sint_t)(distinct_names[v1] != d) << (SUFFIX_GROUP_BIT - 1));
            distinct_names[v1] = d;
        }
    }

    for (j -= 2 * prefetch_distance + 1; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        if (p > 0) {
            SA[i] = 0;
            d += (p >> (SUFFIX_GROUP_BIT - 1));
            p &= ~SUFFIX_GROUP_MARKER;
            sa_sint_t v = BUCKETS_INDEX2(T[p - 1], T[p - 2] > T[p - 1]);
            SA[--induction_bucket[T[p - 1]]] = (p - 1) | ((sa_sint_t)(T[p - 2] > T[p - 1]) << (SAINT_BIT - 1)) |
                                               ((sa_sint_t)(distinct_names[v] != d) << (SUFFIX_GROUP_BIT - 1));
            distinct_names[v] = d;
        }
    }

    return d;
}

static void libsais_partial_sorting_scan_right_to_left_32s_1k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                              sa_sint_t * RESTRICT induction_bucket,
                                                              fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + 2 * prefetch_distance + 1; i >= j; i -= 2) {
        prefetchw(&SA[i - 3 * prefetch_distance]);

        sa_sint_t s0 = SA[i - 2 * prefetch_distance - 0];
        const sa_sint_t * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i - 2 * prefetch_distance - 1];
        const sa_sint_t * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        sa_sint_t s2 = SA[i - 1 * prefetch_distance - 0];
        if (s2 > 0) {
            prefetchw(&induction_bucket[T[s2 - 1]]);
            prefetch(&T[s2] - 2);
        }
        sa_sint_t s3 = SA[i - 1 * prefetch_distance - 1];
        if (s3 > 0) {
            prefetchw(&induction_bucket[T[s3 - 1]]);
            prefetch(&T[s3] - 2);
        }

        sa_sint_t p0 = SA[i - 0];
        if (p0 > 0) {
            SA[i - 0] = 0;
            SA[--induction_bucket[T[p0 - 1]]] = (p0 - 1) | ((sa_sint_t)(T[p0 - 2] > T[p0 - 1]) << (SAINT_BIT - 1));
        }
        sa_sint_t p1 = SA[i - 1];
        if (p1 > 0) {
            SA[i - 1] = 0;
            SA[--induction_bucket[T[p1 - 1]]] = (p1 - 1) | ((sa_sint_t)(T[p1 - 2] > T[p1 - 1]) << (SAINT_BIT - 1));
        }
    }

    for (j -= 2 * prefetch_distance + 1; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        if (p > 0) {
            SA[i] = 0;
            SA[--induction_bucket[T[p - 1]]] = (p - 1) | ((sa_sint_t)(T[p - 2] > T[p - 1]) << (SAINT_BIT - 1));
        }
    }
}
static sa_sint_t libsais_partial_sorting_scan_right_to_left_32s_6k_omp(
    const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t * RESTRICT buckets,
    sa_sint_t first_lms_suffix, sa_sint_t left_suffixes_count, sa_sint_t d, sa_sint_t threads,
    LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    fast_sint_t scan_start = (fast_sint_t)left_suffixes_count + 1;
    fast_sint_t scan_end = (fast_sint_t)n - (fast_sint_t)first_lms_suffix;

    if (threads == 1 || (scan_end - scan_start) < 65536) {
        d = libsais_partial_sorting_scan_right_to_left_32s_6k(T, SA, buckets, d, scan_start, scan_end - scan_start);
    }
    (void)(thread_state);
    return d;
}

static sa_sint_t libsais_partial_sorting_scan_right_to_left_32s_4k_omp(const sa_sint_t * RESTRICT T,
                                                                       sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                       sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                                       sa_sint_t d, sa_sint_t threads,
                                                                       LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (threads == 1 || n < 65536) {
        d = libsais_partial_sorting_scan_right_to_left_32s_4k(T, SA, k, buckets, d, 0, n);
    }
    (void)(thread_state);
    return d;
}

static void libsais_partial_sorting_scan_right_to_left_32s_1k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                                  sa_sint_t n, sa_sint_t * RESTRICT buckets,
                                                                  sa_sint_t threads,
                                                                  LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (threads == 1 || n < 65536) {
        libsais_partial_sorting_scan_right_to_left_32s_1k(T, SA, buckets, 0, n);
    }
    (void)(thread_state);
}

static fast_sint_t libsais_partial_sorting_gather_lms_suffixes_32s_4k(sa_sint_t * RESTRICT SA,
                                                                      fast_sint_t omp_block_start,
                                                                      fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j, l;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - 3, l = omp_block_start; i < j; i += 4) {
        prefetch(&SA[i + prefetch_distance]);

        sa_sint_t s0 = SA[i + 0];
        SA[l] = (s0 - SUFFIX_GROUP_MARKER) & (~SUFFIX_GROUP_MARKER);
        l += (s0 < 0);
        sa_sint_t s1 = SA[i + 1];
        SA[l] = (s1 - SUFFIX_GROUP_MARKER) & (~SUFFIX_GROUP_MARKER);
        l += (s1 < 0);
        sa_sint_t s2 = SA[i + 2];
        SA[l] = (s2 - SUFFIX_GROUP_MARKER) & (~SUFFIX_GROUP_MARKER);
        l += (s2 < 0);
        sa_sint_t s3 = SA[i + 3];
        SA[l] = (s3 - SUFFIX_GROUP_MARKER) & (~SUFFIX_GROUP_MARKER);
        l += (s3 < 0);
    }

    for (j += 3; i < j; i += 1) {
        sa_sint_t s = SA[i];
        SA[l] = (s - SUFFIX_GROUP_MARKER) & (~SUFFIX_GROUP_MARKER);
        l += (s < 0);
    }

    return l;
}

static fast_sint_t libsais_partial_sorting_gather_lms_suffixes_32s_1k(sa_sint_t * RESTRICT SA,
                                                                      fast_sint_t omp_block_start,
                                                                      fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j, l;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - 3, l = omp_block_start; i < j; i += 4) {
        prefetch(&SA[i + prefetch_distance]);

        sa_sint_t s0 = SA[i + 0];
        SA[l] = s0 & SAINT_MAX;
        l += (s0 < 0);
        sa_sint_t s1 = SA[i + 1];
        SA[l] = s1 & SAINT_MAX;
        l += (s1 < 0);
        sa_sint_t s2 = SA[i + 2];
        SA[l] = s2 & SAINT_MAX;
        l += (s2 < 0);
        sa_sint_t s3 = SA[i + 3];
        SA[l] = s3 & SAINT_MAX;
        l += (s3 < 0);
    }

    for (j += 3; i < j; i += 1) {
        sa_sint_t s = SA[i];
        SA[l] = s & SAINT_MAX;
        l += (s < 0);
    }

    return l;
}

static void libsais_partial_sorting_gather_lms_suffixes_32s_4k_omp(sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                   sa_sint_t threads,
                                                                   LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (n / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : n - omp_block_start;

        if (omp_num_threads == 1) {
            libsais_partial_sorting_gather_lms_suffixes_32s_4k(SA, omp_block_start, omp_block_size);
        }
    }
}

static void libsais_partial_sorting_gather_lms_suffixes_32s_1k_omp(sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                   sa_sint_t threads,
                                                                   LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (n / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : n - omp_block_start;

        if (omp_num_threads == 1) {
            libsais_partial_sorting_gather_lms_suffixes_32s_1k(SA, omp_block_start, omp_block_size);
        }
    }
}

static void libsais_induce_partial_order_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                sa_sint_t * RESTRICT buckets, sa_sint_t first_lms_suffix,
                                                sa_sint_t left_suffixes_count, sa_sint_t threads,
                                                LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    memset(&buckets[2 * ALPHABET_SIZE], 0, 2 * ALPHABET_SIZE * sizeof(sa_sint_t));

    sa_sint_t d = libsais_partial_sorting_scan_left_to_right_8u_omp(T, SA, n, buckets, left_suffixes_count, 0, threads,
                                                                    thread_state);
    libsais_partial_sorting_shift_markers_8u_omp(SA, n, buckets, threads);
    libsais_partial_sorting_scan_right_to_left_8u_omp(T, SA, n, buckets, first_lms_suffix, left_suffixes_count, d,
                                                      threads, thread_state);
}

static void libsais_induce_partial_order_32s_6k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                    sa_sint_t k, sa_sint_t * RESTRICT buckets,
                                                    sa_sint_t first_lms_suffix, sa_sint_t left_suffixes_count,
                                                    sa_sint_t threads, LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t d = libsais_partial_sorting_scan_left_to_right_32s_6k_omp(T, SA, n, buckets, left_suffixes_count, 0,
                                                                        threads, thread_state);
    libsais_partial_sorting_shift_markers_32s_6k_omp(SA, k, buckets, threads);
    libsais_partial_sorting_shift_buckets_32s_6k(k, buckets);
    libsais_partial_sorting_scan_right_to_left_32s_6k_omp(T, SA, n, buckets, first_lms_suffix, left_suffixes_count, d,
                                                          threads, thread_state);
}

static void libsais_induce_partial_order_32s_4k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                    sa_sint_t k, sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                                    LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    memset(buckets, 0, 2 * (size_t)k * sizeof(sa_sint_t));

    sa_sint_t d = libsais_partial_sorting_scan_left_to_right_32s_4k_omp(T, SA, n, k, buckets, 0, threads, thread_state);
    libsais_partial_sorting_shift_markers_32s_4k(SA, n);
    libsais_partial_sorting_scan_right_to_left_32s_4k_omp(T, SA, n, k, buckets, d, threads, thread_state);
    libsais_partial_sorting_gather_lms_suffixes_32s_4k_omp(SA, n, threads, thread_state);
}

static void libsais_induce_partial_order_32s_2k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                    sa_sint_t k, sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                                    LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    libsais_partial_sorting_scan_left_to_right_32s_1k_omp(T, SA, n, &buckets[1 * k], threads, thread_state);
    libsais_partial_sorting_scan_right_to_left_32s_1k_omp(T, SA, n, &buckets[0 * k], threads, thread_state);
    libsais_partial_sorting_gather_lms_suffixes_32s_1k_omp(SA, n, threads, thread_state);
}

static void libsais_induce_partial_order_32s_1k_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                    sa_sint_t k, sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                                    LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    libsais_count_suffixes_32s(T, n, k, buckets);
    libsais_initialize_buckets_start_32s_1k(k, buckets);
    libsais_partial_sorting_scan_left_to_right_32s_1k_omp(T, SA, n, buckets, threads, thread_state);

    libsais_count_suffixes_32s(T, n, k, buckets);
    libsais_initialize_buckets_end_32s_1k(k, buckets);
    libsais_partial_sorting_scan_right_to_left_32s_1k_omp(T, SA, n, buckets, threads, thread_state);

    libsais_partial_sorting_gather_lms_suffixes_32s_1k_omp(SA, n, threads, thread_state);
}

static sa_sint_t libsais_renumber_lms_suffixes_8u(sa_sint_t * RESTRICT SA, sa_sint_t m, sa_sint_t name,
                                                  fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT SAm = &SA[m];

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 3; i < j; i += 4) {
        prefetch(&SA[i + 2 * prefetch_distance]);

        prefetchw(&SAm[(SA[i + prefetch_distance + 0] & SAINT_MAX) >> 1]);
        prefetchw(&SAm[(SA[i + prefetch_distance + 1] & SAINT_MAX) >> 1]);
        prefetchw(&SAm[(SA[i + prefetch_distance + 2] & SAINT_MAX) >> 1]);
        prefetchw(&SAm[(SA[i + prefetch_distance + 3] & SAINT_MAX) >> 1]);

        sa_sint_t p0 = SA[i + 0];
        SAm[(p0 & SAINT_MAX) >> 1] = name | SAINT_MIN;
        name += p0 < 0;
        sa_sint_t p1 = SA[i + 1];
        SAm[(p1 & SAINT_MAX) >> 1] = name | SAINT_MIN;
        name += p1 < 0;
        sa_sint_t p2 = SA[i + 2];
        SAm[(p2 & SAINT_MAX) >> 1] = name | SAINT_MIN;
        name += p2 < 0;
        sa_sint_t p3 = SA[i + 3];
        SAm[(p3 & SAINT_MAX) >> 1] = name | SAINT_MIN;
        name += p3 < 0;
    }

    for (j += prefetch_distance + 3; i < j; i += 1) {
        sa_sint_t p = SA[i];
        SAm[(p & SAINT_MAX) >> 1] = name | SAINT_MIN;
        name += p < 0;
    }

    return name;
}

static fast_sint_t libsais_gather_marked_suffixes_8u(sa_sint_t * RESTRICT SA, sa_sint_t m, fast_sint_t l,
                                                     fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    l -= 1;

    fast_sint_t i, j;
    for (i = (fast_sint_t)m + omp_block_start + omp_block_size - 1, j = (fast_sint_t)m + omp_block_start + 3; i >= j;
         i -= 4) {
        prefetch(&SA[i - prefetch_distance]);

        sa_sint_t s0 = SA[i - 0];
        SA[l] = s0 & SAINT_MAX;
        l -= s0 < 0;
        sa_sint_t s1 = SA[i - 1];
        SA[l] = s1 & SAINT_MAX;
        l -= s1 < 0;
        sa_sint_t s2 = SA[i - 2];
        SA[l] = s2 & SAINT_MAX;
        l -= s2 < 0;
        sa_sint_t s3 = SA[i - 3];
        SA[l] = s3 & SAINT_MAX;
        l -= s3 < 0;
    }

    for (j -= 3; i >= j; i -= 1) {
        sa_sint_t s = SA[i];
        SA[l] = s & SAINT_MAX;
        l -= s < 0;
    }

    l += 1;

    return l;
}

static sa_sint_t libsais_renumber_lms_suffixes_8u_omp(sa_sint_t * RESTRICT SA, sa_sint_t m, sa_sint_t threads,
                                                      LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t name = 0;
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (m / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : m - omp_block_start;

        if (omp_num_threads == 1) {
            name = libsais_renumber_lms_suffixes_8u(SA, m, 0, omp_block_start, omp_block_size);
        }
    }

    return name;
}

static void libsais_gather_marked_lms_suffixes_8u_omp(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m, sa_sint_t fs,
                                                      sa_sint_t threads, LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (((fast_sint_t)n >> 1) / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size =
            omp_thread_num < omp_num_threads - 1 ? omp_block_stride : ((fast_sint_t)n >> 1) - omp_block_start;

        if (omp_num_threads == 1) {
            libsais_gather_marked_suffixes_8u(SA, m, (fast_sint_t)n + (fast_sint_t)fs, omp_block_start, omp_block_size);
        }
    }
}

static sa_sint_t libsais_renumber_and_gather_lms_suffixes_8u_omp(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m,
                                                                 sa_sint_t fs, sa_sint_t threads,
                                                                 LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    memset(&SA[m], 0, ((size_t)n >> 1) * sizeof(sa_sint_t));

    sa_sint_t name = libsais_renumber_lms_suffixes_8u_omp(SA, m, threads, thread_state);
    if (name < m) {
        libsais_gather_marked_lms_suffixes_8u_omp(SA, n, m, fs, threads, thread_state);
    } else {
        fast_sint_t i;
        for (i = 0; i < m; i += 1) {
            SA[i] &= SAINT_MAX;
        }
    }

    return name;
}

static sa_sint_t libsais_renumber_distinct_lms_suffixes_32s_4k(sa_sint_t * RESTRICT SA, sa_sint_t m, sa_sint_t name,
                                                               fast_sint_t omp_block_start,
                                                               fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT SAm = &SA[m];

    fast_sint_t i, j;
    sa_sint_t p0, p1, p2, p3 = 0;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 3; i < j; i += 4) {
        prefetchw(&SA[i + 2 * prefetch_distance]);

        prefetchw(&SAm[(SA[i + prefetch_distance + 0] & SAINT_MAX) >> 1]);
        prefetchw(&SAm[(SA[i + prefetch_distance + 1] & SAINT_MAX) >> 1]);
        prefetchw(&SAm[(SA[i + prefetch_distance + 2] & SAINT_MAX) >> 1]);
        prefetchw(&SAm[(SA[i + prefetch_distance + 3] & SAINT_MAX) >> 1]);

        p0 = SA[i + 0];
        SAm[(SA[i + 0] = p0 & SAINT_MAX) >> 1] = name | (p0 & p3 & SAINT_MIN);
        name += p0 < 0;
        p1 = SA[i + 1];
        SAm[(SA[i + 1] = p1 & SAINT_MAX) >> 1] = name | (p1 & p0 & SAINT_MIN);
        name += p1 < 0;
        p2 = SA[i + 2];
        SAm[(SA[i + 2] = p2 & SAINT_MAX) >> 1] = name | (p2 & p1 & SAINT_MIN);
        name += p2 < 0;
        p3 = SA[i + 3];
        SAm[(SA[i + 3] = p3 & SAINT_MAX) >> 1] = name | (p3 & p2 & SAINT_MIN);
        name += p3 < 0;
    }

    for (j += prefetch_distance + 3; i < j; i += 1) {
        p2 = p3;
        p3 = SA[i];
        SAm[(SA[i] = p3 & SAINT_MAX) >> 1] = name | (p3 & p2 & SAINT_MIN);
        name += p3 < 0;
    }

    return name;
}

static void libsais_mark_distinct_lms_suffixes_32s(sa_sint_t * RESTRICT SA, sa_sint_t m, fast_sint_t omp_block_start,
                                                   fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    sa_sint_t p0, p1, p2, p3 = 0;
    for (i = (fast_sint_t)m + omp_block_start, j = (fast_sint_t)m + omp_block_start + omp_block_size - 3; i < j;
         i += 4) {
        prefetchw(&SA[i + prefetch_distance]);

        p0 = SA[i + 0];
        SA[i + 0] = p0 & (p3 | SAINT_MAX);
        p0 = (p0 == 0) ? p3 : p0;
        p1 = SA[i + 1];
        SA[i + 1] = p1 & (p0 | SAINT_MAX);
        p1 = (p1 == 0) ? p0 : p1;
        p2 = SA[i + 2];
        SA[i + 2] = p2 & (p1 | SAINT_MAX);
        p2 = (p2 == 0) ? p1 : p2;
        p3 = SA[i + 3];
        SA[i + 3] = p3 & (p2 | SAINT_MAX);
        p3 = (p3 == 0) ? p2 : p3;
    }

    for (j += 3; i < j; i += 1) {
        p2 = p3;
        p3 = SA[i];
        SA[i] = p3 & (p2 | SAINT_MAX);
        p3 = (p3 == 0) ? p2 : p3;
    }
}

static void libsais_clamp_lms_suffixes_length_32s(sa_sint_t * RESTRICT SA, sa_sint_t m, fast_sint_t omp_block_start,
                                                  fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT SAm = &SA[m];

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - 3; i < j; i += 4) {
        prefetchw(&SAm[i + prefetch_distance]);

        SAm[i + 0] = (SAm[i + 0] < 0 ? SAm[i + 0] : 0) & SAINT_MAX;
        SAm[i + 1] = (SAm[i + 1] < 0 ? SAm[i + 1] : 0) & SAINT_MAX;
        SAm[i + 2] = (SAm[i + 2] < 0 ? SAm[i + 2] : 0) & SAINT_MAX;
        SAm[i + 3] = (SAm[i + 3] < 0 ? SAm[i + 3] : 0) & SAINT_MAX;
    }

    for (j += 3; i < j; i += 1) {
        SAm[i] = (SAm[i] < 0 ? SAm[i] : 0) & SAINT_MAX;
    }
}

static sa_sint_t libsais_renumber_distinct_lms_suffixes_32s_4k_omp(sa_sint_t * RESTRICT SA, sa_sint_t m,
                                                                   sa_sint_t threads,
                                                                   LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t name = 0;
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (m / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : m - omp_block_start;

        if (omp_num_threads == 1) {
            name = libsais_renumber_distinct_lms_suffixes_32s_4k(SA, m, 1, omp_block_start, omp_block_size);
        }
    }

    return name - 1;
}

static void libsais_mark_distinct_lms_suffixes_32s_omp(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m,
                                                       sa_sint_t threads) {
    {
        (void)(threads);

        fast_sint_t omp_block_start = 0;
        fast_sint_t omp_block_size = (fast_sint_t)n >> 1;

        libsais_mark_distinct_lms_suffixes_32s(SA, m, omp_block_start, omp_block_size);
    }
}

static void libsais_clamp_lms_suffixes_length_32s_omp(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m,
                                                      sa_sint_t threads) {
    {
        (void)(threads);

        fast_sint_t omp_block_start = 0;
        fast_sint_t omp_block_size = (fast_sint_t)n >> 1;

        libsais_clamp_lms_suffixes_length_32s(SA, m, omp_block_start, omp_block_size);
    }
}

static sa_sint_t libsais_renumber_and_mark_distinct_lms_suffixes_32s_4k_omp(
    sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m, sa_sint_t threads,
    LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    memset(&SA[m], 0, ((size_t)n >> 1) * sizeof(sa_sint_t));

    sa_sint_t name = libsais_renumber_distinct_lms_suffixes_32s_4k_omp(SA, m, threads, thread_state);
    if (name < m) {
        libsais_mark_distinct_lms_suffixes_32s_omp(SA, n, m, threads);
    }

    return name;
}

static sa_sint_t libsais_renumber_and_mark_distinct_lms_suffixes_32s_1k_omp(sa_sint_t * RESTRICT T,
                                                                            sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                                            sa_sint_t m, sa_sint_t threads) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT SAm = &SA[m];

    {
        libsais_gather_lms_suffixes_32s(T, SA, n);

        memset(&SA[m], 0, ((size_t)n - (size_t)m - (size_t)m) * sizeof(sa_sint_t));

        fast_sint_t i, j;
        for (i = (fast_sint_t)n - (fast_sint_t)m, j = (fast_sint_t)n - 1 - prefetch_distance - 3; i < j; i += 4) {
            prefetch(&SA[i + 2 * prefetch_distance]);

            prefetchw(&SAm[((sa_uint_t)SA[i + prefetch_distance + 0]) >> 1]);
            prefetchw(&SAm[((sa_uint_t)SA[i + prefetch_distance + 1]) >> 1]);
            prefetchw(&SAm[((sa_uint_t)SA[i + prefetch_distance + 2]) >> 1]);
            prefetchw(&SAm[((sa_uint_t)SA[i + prefetch_distance + 3]) >> 1]);

            SAm[((sa_uint_t)SA[i + 0]) >> 1] = SA[i + 1] - SA[i + 0] + 1 + SAINT_MIN;
            SAm[((sa_uint_t)SA[i + 1]) >> 1] = SA[i + 2] - SA[i + 1] + 1 + SAINT_MIN;
            SAm[((sa_uint_t)SA[i + 2]) >> 1] = SA[i + 3] - SA[i + 2] + 1 + SAINT_MIN;
            SAm[((sa_uint_t)SA[i + 3]) >> 1] = SA[i + 4] - SA[i + 3] + 1 + SAINT_MIN;
        }

        for (j += prefetch_distance + 3; i < j; i += 1) {
            SAm[((sa_uint_t)SA[i]) >> 1] = SA[i + 1] - SA[i] + 1 + SAINT_MIN;
        }

        SAm[((sa_uint_t)SA[n - 1]) >> 1] = 1 + SAINT_MIN;
    }

    { libsais_clamp_lms_suffixes_length_32s_omp(SA, n, m, threads); }

    sa_sint_t name = 1;

    {
        fast_sint_t i, j, p = SA[0], plen = SAm[p >> 1];
        sa_sint_t pdiff = SAINT_MIN;
        for (i = 1, j = m - prefetch_distance - 1; i < j; i += 2) {
            prefetch(&SA[i + 2 * prefetch_distance]);

            prefetchw(&SAm[((sa_uint_t)SA[i + prefetch_distance + 0]) >> 1]);
            prefetch(&T[((sa_uint_t)SA[i + prefetch_distance + 0])]);
            prefetchw(&SAm[((sa_uint_t)SA[i + prefetch_distance + 1]) >> 1]);
            prefetch(&T[((sa_uint_t)SA[i + prefetch_distance + 1])]);

            fast_sint_t q = SA[i + 0], qlen = SAm[q >> 1];
            sa_sint_t qdiff = SAINT_MIN;
            if (plen == qlen) {
                fast_sint_t l = 0;
                do {
                    if (T[p + l] != T[q + l]) {
                        break;
                    }
                } while (++l < qlen);
                qdiff = (sa_sint_t)(l - qlen) & SAINT_MIN;
            }
            SAm[p >> 1] = name | (pdiff & qdiff);
            name += (qdiff < 0);

            p = SA[i + 1];
            plen = SAm[p >> 1];
            pdiff = SAINT_MIN;
            if (qlen == plen) {
                fast_sint_t l = 0;
                do {
                    if (T[q + l] != T[p + l]) {
                        break;
                    }
                } while (++l < plen);
                pdiff = (sa_sint_t)(l - plen) & SAINT_MIN;
            }
            SAm[q >> 1] = name | (qdiff & pdiff);
            name += (pdiff < 0);
        }

        for (j += prefetch_distance + 1; i < j; i += 1) {
            fast_sint_t q = SA[i], qlen = SAm[q >> 1];
            sa_sint_t qdiff = SAINT_MIN;
            if (plen == qlen) {
                fast_sint_t l = 0;
                do {
                    if (T[p + l] != T[q + l]) {
                        break;
                    }
                } while (++l < plen);
                qdiff = (sa_sint_t)(l - plen) & SAINT_MIN;
            }
            SAm[p >> 1] = name | (pdiff & qdiff);
            name += (qdiff < 0);

            p = q;
            plen = qlen;
            pdiff = qdiff;
        }

        SAm[p >> 1] = name | pdiff;
        name++;
    }

    if (name <= m) {
        libsais_mark_distinct_lms_suffixes_32s_omp(SA, n, m, threads);
    }

    return name - 1;
}

static void libsais_reconstruct_lms_suffixes(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m,
                                             fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    const sa_sint_t * RESTRICT SAnm = &SA[n - m];

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 3; i < j; i += 4) {
        prefetchw(&SA[i + 2 * prefetch_distance]);

        prefetch(&SAnm[SA[i + prefetch_distance + 0]]);
        prefetch(&SAnm[SA[i + prefetch_distance + 1]]);
        prefetch(&SAnm[SA[i + prefetch_distance + 2]]);
        prefetch(&SAnm[SA[i + prefetch_distance + 3]]);

        SA[i + 0] = SAnm[SA[i + 0]];
        SA[i + 1] = SAnm[SA[i + 1]];
        SA[i + 2] = SAnm[SA[i + 2]];
        SA[i + 3] = SAnm[SA[i + 3]];
    }

    for (j += prefetch_distance + 3; i < j; i += 1) {
        SA[i] = SAnm[SA[i]];
    }
}

static void libsais_reconstruct_lms_suffixes_omp(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m, sa_sint_t threads) {
    {
        (void)(threads);

        fast_sint_t omp_block_start = 0;
        fast_sint_t omp_block_size = m;
        libsais_reconstruct_lms_suffixes(SA, n, m, omp_block_start, omp_block_size);
    }
}

static void libsais_place_lms_suffixes_interval_8u(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m,
                                                   const sa_sint_t * RESTRICT buckets) {
    const sa_sint_t * RESTRICT bucket_end = &buckets[7 * ALPHABET_SIZE];

    fast_sint_t c, j = n;
    for (c = ALPHABET_SIZE - 2; c >= 0; --c) {
        fast_sint_t l = (fast_sint_t)buckets[BUCKETS_INDEX2(c, 1) + BUCKETS_INDEX2(1, 0)] -
                        (fast_sint_t)buckets[BUCKETS_INDEX2(c, 1)];
        if (l > 0) {
            fast_sint_t i = bucket_end[c];
            if (j - i > 0) {
                memset(&SA[i], 0, (size_t)(j - i) * sizeof(sa_sint_t));
            }

            memmove(&SA[j = (i - l)], &SA[m -= (sa_sint_t)l], (size_t)l * sizeof(sa_sint_t));
        }
    }

    memset(&SA[0], 0, (size_t)j * sizeof(sa_sint_t));
}

static void libsais_place_lms_suffixes_interval_32s_4k(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t k, sa_sint_t m,
                                                       const sa_sint_t * RESTRICT buckets) {
    const sa_sint_t * RESTRICT bucket_end = &buckets[3 * k];

    fast_sint_t c, j = n;
    for (c = (fast_sint_t)k - 2; c >= 0; --c) {
        fast_sint_t l = (fast_sint_t)buckets[BUCKETS_INDEX2(c, 1) + BUCKETS_INDEX2(1, 0)] -
                        (fast_sint_t)buckets[BUCKETS_INDEX2(c, 1)];
        if (l > 0) {
            fast_sint_t i = bucket_end[c];
            if (j - i > 0) {
                memset(&SA[i], 0, (size_t)(j - i) * sizeof(sa_sint_t));
            }

            memmove(&SA[j = (i - l)], &SA[m -= (sa_sint_t)l], (size_t)l * sizeof(sa_sint_t));
        }
    }

    memset(&SA[0], 0, (size_t)j * sizeof(sa_sint_t));
}

static void libsais_place_lms_suffixes_interval_32s_2k(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t k, sa_sint_t m,
                                                       const sa_sint_t * RESTRICT buckets) {
    fast_sint_t j = n;

    if (k > 1) {
        fast_sint_t c;
        for (c = BUCKETS_INDEX2((fast_sint_t)k - 2, 0); c >= BUCKETS_INDEX2(0, 0); c -= BUCKETS_INDEX2(1, 0)) {
            fast_sint_t l =
                (fast_sint_t)buckets[c + BUCKETS_INDEX2(1, 1)] - (fast_sint_t)buckets[c + BUCKETS_INDEX2(0, 1)];
            if (l > 0) {
                fast_sint_t i = buckets[c];
                if (j - i > 0) {
                    memset(&SA[i], 0, (size_t)(j - i) * sizeof(sa_sint_t));
                }

                memmove(&SA[j = (i - l)], &SA[m -= (sa_sint_t)l], (size_t)l * sizeof(sa_sint_t));
            }
        }
    }

    memset(&SA[0], 0, (size_t)j * sizeof(sa_sint_t));
}

static void libsais_place_lms_suffixes_interval_32s_1k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                       sa_sint_t k, sa_sint_t m, sa_sint_t * RESTRICT buckets) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t c = k - 1;
    fast_sint_t i, l = buckets[c];
    for (i = (fast_sint_t)m - 1; i >= prefetch_distance + 3; i -= 4) {
        prefetch(&SA[i - 2 * prefetch_distance]);

        prefetch(&T[SA[i - prefetch_distance - 0]]);
        prefetch(&T[SA[i - prefetch_distance - 1]]);
        prefetch(&T[SA[i - prefetch_distance - 2]]);
        prefetch(&T[SA[i - prefetch_distance - 3]]);

        sa_sint_t p0 = SA[i - 0];
        if (T[p0] != c) {
            c = T[p0];
            memset(&SA[buckets[c]], 0, (size_t)(l - buckets[c]) * sizeof(sa_sint_t));
            l = buckets[c];
        }
        SA[--l] = p0;
        sa_sint_t p1 = SA[i - 1];
        if (T[p1] != c) {
            c = T[p1];
            memset(&SA[buckets[c]], 0, (size_t)(l - buckets[c]) * sizeof(sa_sint_t));
            l = buckets[c];
        }
        SA[--l] = p1;
        sa_sint_t p2 = SA[i - 2];
        if (T[p2] != c) {
            c = T[p2];
            memset(&SA[buckets[c]], 0, (size_t)(l - buckets[c]) * sizeof(sa_sint_t));
            l = buckets[c];
        }
        SA[--l] = p2;
        sa_sint_t p3 = SA[i - 3];
        if (T[p3] != c) {
            c = T[p3];
            memset(&SA[buckets[c]], 0, (size_t)(l - buckets[c]) * sizeof(sa_sint_t));
            l = buckets[c];
        }
        SA[--l] = p3;
    }

    for (; i >= 0; i -= 1) {
        sa_sint_t p = SA[i];
        if (T[p] != c) {
            c = T[p];
            memset(&SA[buckets[c]], 0, (size_t)(l - buckets[c]) * sizeof(sa_sint_t));
            l = buckets[c];
        }
        SA[--l] = p;
    }

    memset(&SA[0], 0, (size_t)l * sizeof(sa_sint_t));
}

static void libsais_place_lms_suffixes_histogram_32s_6k(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t k, sa_sint_t m,
                                                        const sa_sint_t * RESTRICT buckets) {
    const sa_sint_t * RESTRICT bucket_end = &buckets[5 * k];

    fast_sint_t c, j = n;
    for (c = (fast_sint_t)k - 2; c >= 0; --c) {
        fast_sint_t l = (fast_sint_t)buckets[BUCKETS_INDEX4(c, 1)];
        if (l > 0) {
            fast_sint_t i = bucket_end[c];
            if (j - i > 0) {
                memset(&SA[i], 0, (size_t)(j - i) * sizeof(sa_sint_t));
            }

            memmove(&SA[j = (i - l)], &SA[m -= (sa_sint_t)l], (size_t)l * sizeof(sa_sint_t));
        }
    }

    memset(&SA[0], 0, (size_t)j * sizeof(sa_sint_t));
}

static void libsais_place_lms_suffixes_histogram_32s_4k(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t k, sa_sint_t m,
                                                        const sa_sint_t * RESTRICT buckets) {
    const sa_sint_t * RESTRICT bucket_end = &buckets[3 * k];

    fast_sint_t c, j = n;
    for (c = (fast_sint_t)k - 2; c >= 0; --c) {
        fast_sint_t l = (fast_sint_t)buckets[BUCKETS_INDEX2(c, 1)];
        if (l > 0) {
            fast_sint_t i = bucket_end[c];
            if (j - i > 0) {
                memset(&SA[i], 0, (size_t)(j - i) * sizeof(sa_sint_t));
            }

            memmove(&SA[j = (i - l)], &SA[m -= (sa_sint_t)l], (size_t)l * sizeof(sa_sint_t));
        }
    }

    memset(&SA[0], 0, (size_t)j * sizeof(sa_sint_t));
}

static void libsais_place_lms_suffixes_histogram_32s_2k(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t k, sa_sint_t m,
                                                        const sa_sint_t * RESTRICT buckets) {
    fast_sint_t j = n;

    if (k > 1) {
        fast_sint_t c;
        for (c = BUCKETS_INDEX2((fast_sint_t)k - 2, 0); c >= BUCKETS_INDEX2(0, 0); c -= BUCKETS_INDEX2(1, 0)) {
            fast_sint_t l = (fast_sint_t)buckets[c + BUCKETS_INDEX2(0, 1)];
            if (l > 0) {
                fast_sint_t i = buckets[c];
                if (j - i > 0) {
                    memset(&SA[i], 0, (size_t)(j - i) * sizeof(sa_sint_t));
                }

                memmove(&SA[j = (i - l)], &SA[m -= (sa_sint_t)l], (size_t)l * sizeof(sa_sint_t));
            }
        }
    }

    memset(&SA[0], 0, (size_t)j * sizeof(sa_sint_t));
}

static void libsais_final_bwt_scan_left_to_right_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                    sa_sint_t * RESTRICT induction_bucket, fast_sint_t omp_block_start,
                                                    fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 1; i < j; i += 2) {
        prefetchw(&SA[i + 2 * prefetch_distance]);

        sa_sint_t s0 = SA[i + prefetch_distance + 0];
        const u8 * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        Ts0--;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i + prefetch_distance + 1];
        const u8 * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        Ts1--;
        prefetch(s1 > 0 ? Ts1 : NULL);

        sa_sint_t p0 = SA[i + 0];
        SA[i + 0] = p0 & SAINT_MAX;
        if (p0 > 0) {
            p0--;
            SA[i + 0] = T[p0] | SAINT_MIN;
            SA[induction_bucket[T[p0]]++] = p0 | ((sa_sint_t)(T[p0 - (p0 > 0)] < T[p0]) << (SAINT_BIT - 1));
        }
        sa_sint_t p1 = SA[i + 1];
        SA[i + 1] = p1 & SAINT_MAX;
        if (p1 > 0) {
            p1--;
            SA[i + 1] = T[p1] | SAINT_MIN;
            SA[induction_bucket[T[p1]]++] = p1 | ((sa_sint_t)(T[p1 - (p1 > 0)] < T[p1]) << (SAINT_BIT - 1));
        }
    }

    for (j += prefetch_distance + 1; i < j; i += 1) {
        sa_sint_t p = SA[i];
        SA[i] = p & SAINT_MAX;
        if (p > 0) {
            p--;
            SA[i] = T[p] | SAINT_MIN;
            SA[induction_bucket[T[p]]++] = p | ((sa_sint_t)(T[p - (p > 0)] < T[p]) << (SAINT_BIT - 1));
        }
    }
}

static void libsais_final_bwt_aux_scan_left_to_right_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t rm,
                                                        sa_sint_t * RESTRICT I, sa_sint_t * RESTRICT induction_bucket,
                                                        fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 1; i < j; i += 2) {
        prefetchw(&SA[i + 2 * prefetch_distance]);

        sa_sint_t s0 = SA[i + prefetch_distance + 0];
        const u8 * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        Ts0--;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i + prefetch_distance + 1];
        const u8 * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        Ts1--;
        prefetch(s1 > 0 ? Ts1 : NULL);

        sa_sint_t p0 = SA[i + 0];
        SA[i + 0] = p0 & SAINT_MAX;
        if (p0 > 0) {
            p0--;
            SA[i + 0] = T[p0] | SAINT_MIN;
            SA[induction_bucket[T[p0]]++] = p0 | ((sa_sint_t)(T[p0 - (p0 > 0)] < T[p0]) << (SAINT_BIT - 1));
            if ((p0 & rm) == 0) {
                I[p0 / (rm + 1)] = induction_bucket[T[p0]];
            }
        }
        sa_sint_t p1 = SA[i + 1];
        SA[i + 1] = p1 & SAINT_MAX;
        if (p1 > 0) {
            p1--;
            SA[i + 1] = T[p1] | SAINT_MIN;
            SA[induction_bucket[T[p1]]++] = p1 | ((sa_sint_t)(T[p1 - (p1 > 0)] < T[p1]) << (SAINT_BIT - 1));
            if ((p1 & rm) == 0) {
                I[p1 / (rm + 1)] = induction_bucket[T[p1]];
            }
        }
    }

    for (j += prefetch_distance + 1; i < j; i += 1) {
        sa_sint_t p = SA[i];
        SA[i] = p & SAINT_MAX;
        if (p > 0) {
            p--;
            SA[i] = T[p] | SAINT_MIN;
            SA[induction_bucket[T[p]]++] = p | ((sa_sint_t)(T[p - (p > 0)] < T[p]) << (SAINT_BIT - 1));
            if ((p & rm) == 0) {
                I[p / (rm + 1)] = induction_bucket[T[p]];
            }
        }
    }
}

static void libsais_final_sorting_scan_left_to_right_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                        sa_sint_t * RESTRICT induction_bucket,
                                                        fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 1; i < j; i += 2) {
        prefetchw(&SA[i + 2 * prefetch_distance]);

        sa_sint_t s0 = SA[i + prefetch_distance + 0];
        const u8 * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        Ts0--;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i + prefetch_distance + 1];
        const u8 * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        Ts1--;
        prefetch(s1 > 0 ? Ts1 : NULL);

        sa_sint_t p0 = SA[i + 0];
        SA[i + 0] = p0 ^ SAINT_MIN;
        if (p0 > 0) {
            p0--;
            SA[induction_bucket[T[p0]]++] = p0 | ((sa_sint_t)(T[p0 - (p0 > 0)] < T[p0]) << (SAINT_BIT - 1));
        }
        sa_sint_t p1 = SA[i + 1];
        SA[i + 1] = p1 ^ SAINT_MIN;
        if (p1 > 0) {
            p1--;
            SA[induction_bucket[T[p1]]++] = p1 | ((sa_sint_t)(T[p1 - (p1 > 0)] < T[p1]) << (SAINT_BIT - 1));
        }
    }

    for (j += prefetch_distance + 1; i < j; i += 1) {
        sa_sint_t p = SA[i];
        SA[i] = p ^ SAINT_MIN;
        if (p > 0) {
            p--;
            SA[induction_bucket[T[p]]++] = p | ((sa_sint_t)(T[p - (p > 0)] < T[p]) << (SAINT_BIT - 1));
        }
    }
}

static void libsais_final_sorting_scan_left_to_right_32s(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                         sa_sint_t * RESTRICT induction_bucket,
                                                         fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - 2 * prefetch_distance - 1; i < j; i += 2) {
        prefetchw(&SA[i + 3 * prefetch_distance]);

        sa_sint_t s0 = SA[i + 2 * prefetch_distance + 0];
        const sa_sint_t * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i + 2 * prefetch_distance + 1];
        const sa_sint_t * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        sa_sint_t s2 = SA[i + 1 * prefetch_distance + 0];
        if (s2 > 0) {
            prefetchw(&induction_bucket[T[s2 - 1]]);
            prefetch(&T[s2] - 2);
        }
        sa_sint_t s3 = SA[i + 1 * prefetch_distance + 1];
        if (s3 > 0) {
            prefetchw(&induction_bucket[T[s3 - 1]]);
            prefetch(&T[s3] - 2);
        }

        sa_sint_t p0 = SA[i + 0];
        SA[i + 0] = p0 ^ SAINT_MIN;
        if (p0 > 0) {
            p0--;
            SA[induction_bucket[T[p0]]++] = p0 | ((sa_sint_t)(T[p0 - (p0 > 0)] < T[p0]) << (SAINT_BIT - 1));
        }
        sa_sint_t p1 = SA[i + 1];
        SA[i + 1] = p1 ^ SAINT_MIN;
        if (p1 > 0) {
            p1--;
            SA[induction_bucket[T[p1]]++] = p1 | ((sa_sint_t)(T[p1 - (p1 > 0)] < T[p1]) << (SAINT_BIT - 1));
        }
    }

    for (j += 2 * prefetch_distance + 1; i < j; i += 1) {
        sa_sint_t p = SA[i];
        SA[i] = p ^ SAINT_MIN;
        if (p > 0) {
            p--;
            SA[induction_bucket[T[p]]++] = p | ((sa_sint_t)(T[p - (p > 0)] < T[p]) << (SAINT_BIT - 1));
        }
    }
}
static void libsais_final_bwt_scan_left_to_right_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, fast_sint_t n,
                                                        sa_sint_t * RESTRICT induction_bucket, sa_sint_t threads,
                                                        LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    SA[induction_bucket[T[(sa_sint_t)n - 1]]++] =
        ((sa_sint_t)n - 1) | ((sa_sint_t)(T[(sa_sint_t)n - 2] < T[(sa_sint_t)n - 1]) << (SAINT_BIT - 1));

    if (threads == 1 || n < 65536) {
        libsais_final_bwt_scan_left_to_right_8u(T, SA, induction_bucket, 0, n);
    }
    (void)(thread_state);
}

static void libsais_final_bwt_aux_scan_left_to_right_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                            fast_sint_t n, sa_sint_t rm, sa_sint_t * RESTRICT I,
                                                            sa_sint_t * RESTRICT induction_bucket, sa_sint_t threads,
                                                            LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    SA[induction_bucket[T[(sa_sint_t)n - 1]]++] =
        ((sa_sint_t)n - 1) | ((sa_sint_t)(T[(sa_sint_t)n - 2] < T[(sa_sint_t)n - 1]) << (SAINT_BIT - 1));

    if ((((sa_sint_t)n - 1) & rm) == 0) {
        I[((sa_sint_t)n - 1) / (rm + 1)] = induction_bucket[T[(sa_sint_t)n - 1]];
    }

    if (threads == 1 || n < 65536) {
        libsais_final_bwt_aux_scan_left_to_right_8u(T, SA, rm, I, induction_bucket, 0, n);
    }
    (void)(thread_state);
}

static void libsais_final_sorting_scan_left_to_right_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                            fast_sint_t n, sa_sint_t * RESTRICT induction_bucket,
                                                            sa_sint_t threads,
                                                            LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    SA[induction_bucket[T[(sa_sint_t)n - 1]]++] =
        ((sa_sint_t)n - 1) | ((sa_sint_t)(T[(sa_sint_t)n - 2] < T[(sa_sint_t)n - 1]) << (SAINT_BIT - 1));

    if (threads == 1 || n < 65536) {
        libsais_final_sorting_scan_left_to_right_8u(T, SA, induction_bucket, 0, n);
    }
    (void)(thread_state);
}

static void libsais_final_sorting_scan_left_to_right_32s_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                             sa_sint_t n, sa_sint_t * RESTRICT induction_bucket,
                                                             sa_sint_t threads,
                                                             LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    SA[induction_bucket[T[n - 1]]++] = (n - 1) | ((sa_sint_t)(T[n - 2] < T[n - 1]) << (SAINT_BIT - 1));

    if (threads == 1 || n < 65536) {
        libsais_final_sorting_scan_left_to_right_32s(T, SA, induction_bucket, 0, n);
    }
    (void)(thread_state);
}

static sa_sint_t libsais_final_bwt_scan_right_to_left_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                         sa_sint_t * RESTRICT induction_bucket,
                                                         fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    sa_sint_t index = -1;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + prefetch_distance + 1; i >= j; i -= 2) {
        prefetchw(&SA[i - 2 * prefetch_distance]);

        sa_sint_t s0 = SA[i - prefetch_distance - 0];
        const u8 * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        Ts0--;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i - prefetch_distance - 1];
        const u8 * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        Ts1--;
        prefetch(s1 > 0 ? Ts1 : NULL);

        sa_sint_t p0 = SA[i - 0];
        index = (p0 == 0) ? (sa_sint_t)(i - 0) : index;
        SA[i - 0] = p0 & SAINT_MAX;
        if (p0 > 0) {
            p0--;
            u8 c0 = T[p0 - (p0 > 0)], c1 = T[p0];
            SA[i - 0] = c1;
            sa_sint_t t = c0 | SAINT_MIN;
            SA[--induction_bucket[c1]] = (c0 <= c1) ? p0 : t;
        }

        sa_sint_t p1 = SA[i - 1];
        index = (p1 == 0) ? (sa_sint_t)(i - 1) : index;
        SA[i - 1] = p1 & SAINT_MAX;
        if (p1 > 0) {
            p1--;
            u8 c0 = T[p1 - (p1 > 0)], c1 = T[p1];
            SA[i - 1] = c1;
            sa_sint_t t = c0 | SAINT_MIN;
            SA[--induction_bucket[c1]] = (c0 <= c1) ? p1 : t;
        }
    }

    for (j -= prefetch_distance + 1; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        index = (p == 0) ? (sa_sint_t)i : index;
        SA[i] = p & SAINT_MAX;
        if (p > 0) {
            p--;
            u8 c0 = T[p - (p > 0)], c1 = T[p];
            SA[i] = c1;
            sa_sint_t t = c0 | SAINT_MIN;
            SA[--induction_bucket[c1]] = (c0 <= c1) ? p : t;
        }
    }

    return index;
}

static void libsais_final_bwt_aux_scan_right_to_left_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t rm,
                                                        sa_sint_t * RESTRICT I, sa_sint_t * RESTRICT induction_bucket,
                                                        fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + prefetch_distance + 1; i >= j; i -= 2) {
        prefetchw(&SA[i - 2 * prefetch_distance]);

        sa_sint_t s0 = SA[i - prefetch_distance - 0];
        const u8 * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        Ts0--;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i - prefetch_distance - 1];
        const u8 * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        Ts1--;
        prefetch(s1 > 0 ? Ts1 : NULL);

        sa_sint_t p0 = SA[i - 0];
        SA[i - 0] = p0 & SAINT_MAX;
        if (p0 > 0) {
            p0--;
            u8 c0 = T[p0 - (p0 > 0)], c1 = T[p0];
            SA[i - 0] = c1;
            sa_sint_t t = c0 | SAINT_MIN;
            SA[--induction_bucket[c1]] = (c0 <= c1) ? p0 : t;
            if ((p0 & rm) == 0) {
                I[p0 / (rm + 1)] = induction_bucket[T[p0]] + 1;
            }
        }

        sa_sint_t p1 = SA[i - 1];
        SA[i - 1] = p1 & SAINT_MAX;
        if (p1 > 0) {
            p1--;
            u8 c0 = T[p1 - (p1 > 0)], c1 = T[p1];
            SA[i - 1] = c1;
            sa_sint_t t = c0 | SAINT_MIN;
            SA[--induction_bucket[c1]] = (c0 <= c1) ? p1 : t;
            if ((p1 & rm) == 0) {
                I[p1 / (rm + 1)] = induction_bucket[T[p1]] + 1;
            }
        }
    }

    for (j -= prefetch_distance + 1; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        SA[i] = p & SAINT_MAX;
        if (p > 0) {
            p--;
            u8 c0 = T[p - (p > 0)], c1 = T[p];
            SA[i] = c1;
            sa_sint_t t = c0 | SAINT_MIN;
            SA[--induction_bucket[c1]] = (c0 <= c1) ? p : t;
            if ((p & rm) == 0) {
                I[p / (rm + 1)] = induction_bucket[T[p]] + 1;
            }
        }
    }
}

static void libsais_final_sorting_scan_right_to_left_8u(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                        sa_sint_t * RESTRICT induction_bucket,
                                                        fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + prefetch_distance + 1; i >= j; i -= 2) {
        prefetchw(&SA[i - 2 * prefetch_distance]);

        sa_sint_t s0 = SA[i - prefetch_distance - 0];
        const u8 * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        Ts0--;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i - prefetch_distance - 1];
        const u8 * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        Ts1--;
        prefetch(s1 > 0 ? Ts1 : NULL);

        sa_sint_t p0 = SA[i - 0];
        SA[i - 0] = p0 & SAINT_MAX;
        if (p0 > 0) {
            p0--;
            SA[--induction_bucket[T[p0]]] = p0 | ((sa_sint_t)(T[p0 - (p0 > 0)] > T[p0]) << (SAINT_BIT - 1));
        }
        sa_sint_t p1 = SA[i - 1];
        SA[i - 1] = p1 & SAINT_MAX;
        if (p1 > 0) {
            p1--;
            SA[--induction_bucket[T[p1]]] = p1 | ((sa_sint_t)(T[p1 - (p1 > 0)] > T[p1]) << (SAINT_BIT - 1));
        }
    }

    for (j -= prefetch_distance + 1; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        SA[i] = p & SAINT_MAX;
        if (p > 0) {
            p--;
            SA[--induction_bucket[T[p]]] = p | ((sa_sint_t)(T[p - (p > 0)] > T[p]) << (SAINT_BIT - 1));
        }
    }
}

static void libsais_final_sorting_scan_right_to_left_32s(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                         sa_sint_t * RESTRICT induction_bucket,
                                                         fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start + omp_block_size - 1, j = omp_block_start + 2 * prefetch_distance + 1; i >= j; i -= 2) {
        prefetchw(&SA[i - 3 * prefetch_distance]);

        sa_sint_t s0 = SA[i - 2 * prefetch_distance - 0];
        const sa_sint_t * Ts0 = &T[s0] - 1;
        prefetch(s0 > 0 ? Ts0 : NULL);
        sa_sint_t s1 = SA[i - 2 * prefetch_distance - 1];
        const sa_sint_t * Ts1 = &T[s1] - 1;
        prefetch(s1 > 0 ? Ts1 : NULL);
        sa_sint_t s2 = SA[i - 1 * prefetch_distance - 0];
        if (s2 > 0) {
            prefetchw(&induction_bucket[T[s2 - 1]]);
            prefetch(&T[s2] - 2);
        }
        sa_sint_t s3 = SA[i - 1 * prefetch_distance - 1];
        if (s3 > 0) {
            prefetchw(&induction_bucket[T[s3 - 1]]);
            prefetch(&T[s3] - 2);
        }

        sa_sint_t p0 = SA[i - 0];
        SA[i - 0] = p0 & SAINT_MAX;
        if (p0 > 0) {
            p0--;
            SA[--induction_bucket[T[p0]]] = p0 | ((sa_sint_t)(T[p0 - (p0 > 0)] > T[p0]) << (SAINT_BIT - 1));
        }
        sa_sint_t p1 = SA[i - 1];
        SA[i - 1] = p1 & SAINT_MAX;
        if (p1 > 0) {
            p1--;
            SA[--induction_bucket[T[p1]]] = p1 | ((sa_sint_t)(T[p1 - (p1 > 0)] > T[p1]) << (SAINT_BIT - 1));
        }
    }

    for (j -= 2 * prefetch_distance + 1; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        SA[i] = p & SAINT_MAX;
        if (p > 0) {
            p--;
            SA[--induction_bucket[T[p]]] = p | ((sa_sint_t)(T[p - (p > 0)] > T[p]) << (SAINT_BIT - 1));
        }
    }
}
static sa_sint_t libsais_final_bwt_scan_right_to_left_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                             sa_sint_t n, sa_sint_t * RESTRICT induction_bucket,
                                                             sa_sint_t threads,
                                                             LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t index = -1;

    if (threads == 1 || n < 65536) {
        index = libsais_final_bwt_scan_right_to_left_8u(T, SA, induction_bucket, 0, n);
    }
    (void)(thread_state);
    return index;
}

static void libsais_final_bwt_aux_scan_right_to_left_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                            sa_sint_t rm, sa_sint_t * RESTRICT I,
                                                            sa_sint_t * RESTRICT induction_bucket, sa_sint_t threads,
                                                            LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (threads == 1 || n < 65536) {
        libsais_final_bwt_aux_scan_right_to_left_8u(T, SA, rm, I, induction_bucket, 0, n);
    }
    (void)(thread_state);
}

static void libsais_final_sorting_scan_right_to_left_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                            sa_sint_t * RESTRICT induction_bucket, sa_sint_t threads,
                                                            LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (threads == 1 || n < 65536) {
        libsais_final_sorting_scan_right_to_left_8u(T, SA, induction_bucket, 0, n);
    }
    (void)(thread_state);
}

static void libsais_final_sorting_scan_right_to_left_32s_omp(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                             sa_sint_t n, sa_sint_t * RESTRICT induction_bucket,
                                                             sa_sint_t threads,
                                                             LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (threads == 1 || n < 65536) {
        libsais_final_sorting_scan_right_to_left_32s(T, SA, induction_bucket, 0, n);
    }
    (void)(thread_state);
}

static void libsais_clear_lms_suffixes_omp(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t k,
                                           sa_sint_t * RESTRICT bucket_start, sa_sint_t * RESTRICT bucket_end,
                                           sa_sint_t threads) {
    fast_sint_t c;
    (void)(threads);
    (void)(n);

    for (c = 0; c < k; ++c) {
        if (bucket_end[c] > bucket_start[c]) {
            memset(&SA[bucket_start[c]], 0, ((size_t)bucket_end[c] - (size_t)bucket_start[c]) * sizeof(sa_sint_t));
        }
    }
}

static sa_sint_t libsais_induce_final_order_8u_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                   sa_sint_t bwt, sa_sint_t r, sa_sint_t * RESTRICT I,
                                                   sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                                   LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (!bwt) {
        libsais_final_sorting_scan_left_to_right_8u_omp(T, SA, n, &buckets[6 * ALPHABET_SIZE], threads, thread_state);
        if (threads > 1 && n >= 65536) {
            libsais_clear_lms_suffixes_omp(SA, n, ALPHABET_SIZE, &buckets[6 * ALPHABET_SIZE],
                                           &buckets[7 * ALPHABET_SIZE], threads);
        }
        libsais_final_sorting_scan_right_to_left_8u_omp(T, SA, n, &buckets[7 * ALPHABET_SIZE], threads, thread_state);
        return 0;
    } else if (I != NULL) {
        libsais_final_bwt_aux_scan_left_to_right_8u_omp(T, SA, n, r - 1, I, &buckets[6 * ALPHABET_SIZE], threads,
                                                        thread_state);
        if (threads > 1 && n >= 65536) {
            libsais_clear_lms_suffixes_omp(SA, n, ALPHABET_SIZE, &buckets[6 * ALPHABET_SIZE],
                                           &buckets[7 * ALPHABET_SIZE], threads);
        }
        libsais_final_bwt_aux_scan_right_to_left_8u_omp(T, SA, n, r - 1, I, &buckets[7 * ALPHABET_SIZE], threads,
                                                        thread_state);
        return 0;
    } else {
        libsais_final_bwt_scan_left_to_right_8u_omp(T, SA, n, &buckets[6 * ALPHABET_SIZE], threads, thread_state);
        if (threads > 1 && n >= 65536) {
            libsais_clear_lms_suffixes_omp(SA, n, ALPHABET_SIZE, &buckets[6 * ALPHABET_SIZE],
                                           &buckets[7 * ALPHABET_SIZE], threads);
        }
        return libsais_final_bwt_scan_right_to_left_8u_omp(T, SA, n, &buckets[7 * ALPHABET_SIZE], threads,
                                                           thread_state);
    }
}

static void libsais_induce_final_order_32s_6k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                              sa_sint_t k, sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                              LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    libsais_final_sorting_scan_left_to_right_32s_omp(T, SA, n, &buckets[4 * k], threads, thread_state);
    libsais_final_sorting_scan_right_to_left_32s_omp(T, SA, n, &buckets[5 * k], threads, thread_state);
}

static void libsais_induce_final_order_32s_4k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                              sa_sint_t k, sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                              LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    libsais_final_sorting_scan_left_to_right_32s_omp(T, SA, n, &buckets[2 * k], threads, thread_state);
    libsais_final_sorting_scan_right_to_left_32s_omp(T, SA, n, &buckets[3 * k], threads, thread_state);
}

static void libsais_induce_final_order_32s_2k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                              sa_sint_t k, sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                              LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    libsais_final_sorting_scan_left_to_right_32s_omp(T, SA, n, &buckets[1 * k], threads, thread_state);
    libsais_final_sorting_scan_right_to_left_32s_omp(T, SA, n, &buckets[0 * k], threads, thread_state);
}

static void libsais_induce_final_order_32s_1k(const sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                              sa_sint_t k, sa_sint_t * RESTRICT buckets, sa_sint_t threads,
                                              LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    libsais_count_suffixes_32s(T, n, k, buckets);
    libsais_initialize_buckets_start_32s_1k(k, buckets);
    libsais_final_sorting_scan_left_to_right_32s_omp(T, SA, n, buckets, threads, thread_state);

    libsais_count_suffixes_32s(T, n, k, buckets);
    libsais_initialize_buckets_end_32s_1k(k, buckets);
    libsais_final_sorting_scan_right_to_left_32s_omp(T, SA, n, buckets, threads, thread_state);
}

static sa_sint_t libsais_renumber_unique_and_nonunique_lms_suffixes_32s(sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                                        sa_sint_t m, sa_sint_t f,
                                                                        fast_sint_t omp_block_start,
                                                                        fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT SAm = &SA[m];

    sa_sint_t i, j;
    for (i = (sa_sint_t)omp_block_start,
        j = (sa_sint_t)omp_block_start + (sa_sint_t)omp_block_size - 2 * (sa_sint_t)prefetch_distance - 3;
         i < j; i += 4) {
        prefetch(&SA[i + 3 * prefetch_distance]);

        prefetchw(&SAm[((sa_uint_t)SA[i + 2 * prefetch_distance + 0]) >> 1]);
        prefetchw(&SAm[((sa_uint_t)SA[i + 2 * prefetch_distance + 1]) >> 1]);
        prefetchw(&SAm[((sa_uint_t)SA[i + 2 * prefetch_distance + 2]) >> 1]);
        prefetchw(&SAm[((sa_uint_t)SA[i + 2 * prefetch_distance + 3]) >> 1]);

        sa_uint_t q0 = (sa_uint_t)SA[i + prefetch_distance + 0];
        const sa_sint_t * Tq0 = &T[q0];
        prefetchw(SAm[q0 >> 1] < 0 ? Tq0 : NULL);
        sa_uint_t q1 = (sa_uint_t)SA[i + prefetch_distance + 1];
        const sa_sint_t * Tq1 = &T[q1];
        prefetchw(SAm[q1 >> 1] < 0 ? Tq1 : NULL);
        sa_uint_t q2 = (sa_uint_t)SA[i + prefetch_distance + 2];
        const sa_sint_t * Tq2 = &T[q2];
        prefetchw(SAm[q2 >> 1] < 0 ? Tq2 : NULL);
        sa_uint_t q3 = (sa_uint_t)SA[i + prefetch_distance + 3];
        const sa_sint_t * Tq3 = &T[q3];
        prefetchw(SAm[q3 >> 1] < 0 ? Tq3 : NULL);

        sa_uint_t p0 = (sa_uint_t)SA[i + 0];
        sa_sint_t s0 = SAm[p0 >> 1];
        if (s0 < 0) {
            T[p0] |= SAINT_MIN;
            f++;
            s0 = i + 0 + SAINT_MIN + f;
        }
        SAm[p0 >> 1] = s0 - f;
        sa_uint_t p1 = (sa_uint_t)SA[i + 1];
        sa_sint_t s1 = SAm[p1 >> 1];
        if (s1 < 0) {
            T[p1] |= SAINT_MIN;
            f++;
            s1 = i + 1 + SAINT_MIN + f;
        }
        SAm[p1 >> 1] = s1 - f;
        sa_uint_t p2 = (sa_uint_t)SA[i + 2];
        sa_sint_t s2 = SAm[p2 >> 1];
        if (s2 < 0) {
            T[p2] |= SAINT_MIN;
            f++;
            s2 = i + 2 + SAINT_MIN + f;
        }
        SAm[p2 >> 1] = s2 - f;
        sa_uint_t p3 = (sa_uint_t)SA[i + 3];
        sa_sint_t s3 = SAm[p3 >> 1];
        if (s3 < 0) {
            T[p3] |= SAINT_MIN;
            f++;
            s3 = i + 3 + SAINT_MIN + f;
        }
        SAm[p3 >> 1] = s3 - f;
    }

    for (j += 2 * (sa_sint_t)prefetch_distance + 3; i < j; i += 1) {
        sa_uint_t p = (sa_uint_t)SA[i];
        sa_sint_t s = SAm[p >> 1];
        if (s < 0) {
            T[p] |= SAINT_MIN;
            f++;
            s = i + SAINT_MIN + f;
        }
        SAm[p >> 1] = s - f;
    }

    return f;
}

static void libsais_compact_unique_and_nonunique_lms_suffixes_32s(sa_sint_t * RESTRICT SA, sa_sint_t m,
                                                                  fast_sint_t * pl, fast_sint_t * pr,
                                                                  fast_sint_t omp_block_start,
                                                                  fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    sa_sint_t * RESTRICT SAl = &SA[0];
    sa_sint_t * RESTRICT SAr = &SA[0];

    fast_sint_t i, j, l = *pl - 1, r = *pr - 1;
    for (i = (fast_sint_t)m + omp_block_start + omp_block_size - 1, j = (fast_sint_t)m + omp_block_start + 3; i >= j;
         i -= 4) {
        prefetch(&SA[i - prefetch_distance]);

        sa_sint_t p0 = SA[i - 0];
        SAl[l] = p0 & SAINT_MAX;
        l -= p0 < 0;
        SAr[r] = p0 - 1;
        r -= p0 > 0;
        sa_sint_t p1 = SA[i - 1];
        SAl[l] = p1 & SAINT_MAX;
        l -= p1 < 0;
        SAr[r] = p1 - 1;
        r -= p1 > 0;
        sa_sint_t p2 = SA[i - 2];
        SAl[l] = p2 & SAINT_MAX;
        l -= p2 < 0;
        SAr[r] = p2 - 1;
        r -= p2 > 0;
        sa_sint_t p3 = SA[i - 3];
        SAl[l] = p3 & SAINT_MAX;
        l -= p3 < 0;
        SAr[r] = p3 - 1;
        r -= p3 > 0;
    }

    for (j -= 3; i >= j; i -= 1) {
        sa_sint_t p = SA[i];
        SAl[l] = p & SAINT_MAX;
        l -= p < 0;
        SAr[r] = p - 1;
        r -= p > 0;
    }

    *pl = l + 1;
    *pr = r + 1;
}
static sa_sint_t libsais_renumber_unique_and_nonunique_lms_suffixes_32s_omp(
    sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t m, sa_sint_t threads,
    LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t f = 0;
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (m / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : m - omp_block_start;

        if (omp_num_threads == 1) {
            f = libsais_renumber_unique_and_nonunique_lms_suffixes_32s(T, SA, m, 0, omp_block_start, omp_block_size);
        }
    }

    return f;
}

static void libsais_compact_unique_and_nonunique_lms_suffixes_32s_omp(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m,
                                                                      sa_sint_t fs, sa_sint_t f, sa_sint_t threads,
                                                                      LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (((fast_sint_t)n >> 1) / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size =
            omp_thread_num < omp_num_threads - 1 ? omp_block_stride : ((fast_sint_t)n >> 1) - omp_block_start;

        if (omp_num_threads == 1) {
            fast_sint_t l = m, r = (fast_sint_t)n + (fast_sint_t)fs;
            libsais_compact_unique_and_nonunique_lms_suffixes_32s(SA, m, &l, &r, omp_block_start, omp_block_size);
        }
    }

    memcpy(&SA[(fast_sint_t)n + (fast_sint_t)fs - (fast_sint_t)m], &SA[(fast_sint_t)m - (fast_sint_t)f],
           (size_t)f * sizeof(sa_sint_t));
}

static sa_sint_t libsais_compact_lms_suffixes_32s_omp(sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                      sa_sint_t m, sa_sint_t fs, sa_sint_t threads,
                                                      LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    sa_sint_t f = libsais_renumber_unique_and_nonunique_lms_suffixes_32s_omp(T, SA, m, threads, thread_state);
    libsais_compact_unique_and_nonunique_lms_suffixes_32s_omp(SA, n, m, fs, f, threads, thread_state);

    return f;
}

static void libsais_merge_unique_lms_suffixes_32s(sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                  sa_sint_t m, fast_sint_t l, fast_sint_t omp_block_start,
                                                  fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    const sa_sint_t * RESTRICT SAnm = &SA[(fast_sint_t)n - (fast_sint_t)m - 1 + l];

    sa_sint_t i, j;
    fast_sint_t tmp = *SAnm++;
    for (i = (sa_sint_t)omp_block_start, j = (sa_sint_t)omp_block_start + (sa_sint_t)omp_block_size - 6; i < j;
         i += 4) {
        prefetch(&T[i + prefetch_distance]);

        sa_sint_t c0 = T[i + 0];
        if (c0 < 0) {
            T[i + 0] = c0 & SAINT_MAX;
            SA[tmp] = i + 0;
            i++;
            tmp = *SAnm++;
        }
        sa_sint_t c1 = T[i + 1];
        if (c1 < 0) {
            T[i + 1] = c1 & SAINT_MAX;
            SA[tmp] = i + 1;
            i++;
            tmp = *SAnm++;
        }
        sa_sint_t c2 = T[i + 2];
        if (c2 < 0) {
            T[i + 2] = c2 & SAINT_MAX;
            SA[tmp] = i + 2;
            i++;
            tmp = *SAnm++;
        }
        sa_sint_t c3 = T[i + 3];
        if (c3 < 0) {
            T[i + 3] = c3 & SAINT_MAX;
            SA[tmp] = i + 3;
            i++;
            tmp = *SAnm++;
        }
    }

    for (j += 6; i < j; i += 1) {
        sa_sint_t c = T[i];
        if (c < 0) {
            T[i] = c & SAINT_MAX;
            SA[tmp] = i;
            i++;
            tmp = *SAnm++;
        }
    }
}

static void libsais_merge_nonunique_lms_suffixes_32s(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m, fast_sint_t l,
                                                     fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    const sa_sint_t * RESTRICT SAnm = &SA[(fast_sint_t)n - (fast_sint_t)m - 1 + l];

    fast_sint_t i, j;
    sa_sint_t tmp = *SAnm++;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - 3; i < j; i += 4) {
        prefetch(&SA[i + prefetch_distance]);

        if (SA[i + 0] == 0) {
            SA[i + 0] = tmp;
            tmp = *SAnm++;
        }
        if (SA[i + 1] == 0) {
            SA[i + 1] = tmp;
            tmp = *SAnm++;
        }
        if (SA[i + 2] == 0) {
            SA[i + 2] = tmp;
            tmp = *SAnm++;
        }
        if (SA[i + 3] == 0) {
            SA[i + 3] = tmp;
            tmp = *SAnm++;
        }
    }

    for (j += 3; i < j; i += 1) {
        if (SA[i] == 0) {
            SA[i] = tmp;
            tmp = *SAnm++;
        }
    }
}

static void libsais_merge_unique_lms_suffixes_32s_omp(sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                      sa_sint_t m, sa_sint_t threads,
                                                      LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (n / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : n - omp_block_start;

        if (omp_num_threads == 1) {
            libsais_merge_unique_lms_suffixes_32s(T, SA, n, m, 0, omp_block_start, omp_block_size);
        }
    }
}

static void libsais_merge_nonunique_lms_suffixes_32s_omp(sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t m, sa_sint_t f,
                                                         sa_sint_t threads,
                                                         LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    {
        (void)(threads);
        (void)(thread_state);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (m / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : m - omp_block_start;

        if (omp_num_threads == 1) {
            libsais_merge_nonunique_lms_suffixes_32s(SA, n, m, f, omp_block_start, omp_block_size);
        }
    }
}

static void libsais_merge_compacted_lms_suffixes_32s_omp(sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n,
                                                         sa_sint_t m, sa_sint_t f, sa_sint_t threads,
                                                         LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    libsais_merge_unique_lms_suffixes_32s_omp(T, SA, n, m, threads, thread_state);
    libsais_merge_nonunique_lms_suffixes_32s_omp(SA, n, m, f, threads, thread_state);
}

static void libsais_reconstruct_compacted_lms_suffixes_32s_2k_omp(sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                                  sa_sint_t n, sa_sint_t k, sa_sint_t m, sa_sint_t fs,
                                                                  sa_sint_t f, sa_sint_t * RESTRICT buckets,
                                                                  sa_sint_t threads,
                                                                  LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (f > 0) {
        memmove(&SA[n - m - 1], &SA[n + fs - m], (size_t)f * sizeof(sa_sint_t));

        libsais_count_and_gather_compacted_lms_suffixes_32s_2k_omp(T, SA, n, k, buckets, threads, thread_state);
        libsais_reconstruct_lms_suffixes_omp(SA, n, m - f, threads);

        memcpy(&SA[n - m - 1 + f], &SA[0], ((size_t)m - (size_t)f) * sizeof(sa_sint_t));
        memset(&SA[0], 0, (size_t)m * sizeof(sa_sint_t));

        libsais_merge_compacted_lms_suffixes_32s_omp(T, SA, n, m, f, threads, thread_state);
    } else {
        libsais_count_and_gather_lms_suffixes_32s_2k(T, SA, n, k, buckets, 0, n);
        libsais_reconstruct_lms_suffixes_omp(SA, n, m, threads);
    }
}

static void libsais_reconstruct_compacted_lms_suffixes_32s_1k_omp(sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA,
                                                                  sa_sint_t n, sa_sint_t m, sa_sint_t fs, sa_sint_t f,
                                                                  sa_sint_t threads,
                                                                  LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    if (f > 0) {
        memmove(&SA[n - m - 1], &SA[n + fs - m], (size_t)f * sizeof(sa_sint_t));

        libsais_gather_compacted_lms_suffixes_32s(T, SA, n);
        libsais_reconstruct_lms_suffixes_omp(SA, n, m - f, threads);

        memcpy(&SA[n - m - 1 + f], &SA[0], ((size_t)m - (size_t)f) * sizeof(sa_sint_t));
        memset(&SA[0], 0, (size_t)m * sizeof(sa_sint_t));

        libsais_merge_compacted_lms_suffixes_32s_omp(T, SA, n, m, f, threads, thread_state);
    } else {
        libsais_gather_lms_suffixes_32s(T, SA, n);
        libsais_reconstruct_lms_suffixes_omp(SA, n, m, threads);
    }
}

static sa_sint_t libsais_main_32s(sa_sint_t * RESTRICT T, sa_sint_t * RESTRICT SA, sa_sint_t n, sa_sint_t k,
                                  sa_sint_t fs, sa_sint_t threads, LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    fs = fs < (SAINT_MAX - n) ? fs : (SAINT_MAX - n);

    if (k > 0 && fs / k >= 6) {
        sa_sint_t alignment = (fs - 1024) / k >= 6 ? 1024 : 16;
        sa_sint_t * RESTRICT buckets =
            (fs - alignment) / k >= 6
                ? (sa_sint_t *)libsais_align_up(&SA[n + fs - 6 * k - alignment], (size_t)alignment * sizeof(sa_sint_t))
                : &SA[n + fs - 6 * k];

        sa_sint_t m = libsais_count_and_gather_lms_suffixes_32s_4k_omp(T, SA, n, k, buckets, threads, thread_state);
        if (m > 1) {
            memset(SA, 0, ((size_t)n - (size_t)m) * sizeof(sa_sint_t));

            sa_sint_t first_lms_suffix = SA[n - m];
            sa_sint_t left_suffixes_count =
                libsais_initialize_buckets_for_lms_suffixes_radix_sort_32s_6k(T, k, buckets, first_lms_suffix);

            libsais_radix_sort_lms_suffixes_32s_6k_omp(T, SA, n, m, &buckets[4 * k], threads, thread_state);
            libsais_radix_sort_set_markers_32s_6k_omp(SA, k, &buckets[4 * k], threads);

            if (threads > 1 && n >= 65536) {
                memset(&SA[(fast_sint_t)n - (fast_sint_t)m], 0, (size_t)m * sizeof(sa_sint_t));
            }

            libsais_initialize_buckets_for_partial_sorting_32s_6k(T, k, buckets, first_lms_suffix, left_suffixes_count);
            libsais_induce_partial_order_32s_6k_omp(T, SA, n, k, buckets, first_lms_suffix, left_suffixes_count,
                                                    threads, thread_state);

            sa_sint_t names =
                libsais_renumber_and_mark_distinct_lms_suffixes_32s_4k_omp(SA, n, m, threads, thread_state);
            if (names < m) {
                sa_sint_t f = libsais_compact_lms_suffixes_32s_omp(T, SA, n, m, fs, threads, thread_state);

                if (libsais_main_32s(SA + n + fs - m + f, SA, m - f, names - f, fs + n - 2 * m + f, threads,
                                     thread_state) != 0) {
                    return -2;
                }

                libsais_reconstruct_compacted_lms_suffixes_32s_2k_omp(T, SA, n, k, m, fs, f, buckets, threads,
                                                                      thread_state);
            } else {
                libsais_count_lms_suffixes_32s_2k(T, n, k, buckets);
            }

            libsais_initialize_buckets_start_and_end_32s_4k(k, buckets);
            libsais_place_lms_suffixes_histogram_32s_4k(SA, n, k, m, buckets);
            libsais_induce_final_order_32s_4k(T, SA, n, k, buckets, threads, thread_state);
        } else {
            SA[0] = SA[n - 1];

            libsais_initialize_buckets_start_and_end_32s_6k(k, buckets);
            libsais_place_lms_suffixes_histogram_32s_6k(SA, n, k, m, buckets);
            libsais_induce_final_order_32s_6k(T, SA, n, k, buckets, threads, thread_state);
        }

        return 0;
    } else if (k > 0 && fs / k >= 4) {
        sa_sint_t alignment = (fs - 1024) / k >= 4 ? 1024 : 16;
        sa_sint_t * RESTRICT buckets =
            (fs - alignment) / k >= 4
                ? (sa_sint_t *)libsais_align_up(&SA[n + fs - 4 * k - alignment], (size_t)alignment * sizeof(sa_sint_t))
                : &SA[n + fs - 4 * k];

        sa_sint_t m = libsais_count_and_gather_lms_suffixes_32s_2k_omp(T, SA, n, k, buckets, threads, thread_state);
        if (m > 1) {
            libsais_initialize_buckets_for_radix_and_partial_sorting_32s_4k(T, k, buckets, SA[n - m]);

            libsais_radix_sort_lms_suffixes_32s_2k_omp(T, SA, n, m, &buckets[1], threads, thread_state);
            libsais_radix_sort_set_markers_32s_4k_omp(SA, k, &buckets[1], threads);

            libsais_place_lms_suffixes_interval_32s_4k(SA, n, k, m - 1, buckets);
            libsais_induce_partial_order_32s_4k_omp(T, SA, n, k, buckets, threads, thread_state);

            sa_sint_t names =
                libsais_renumber_and_mark_distinct_lms_suffixes_32s_4k_omp(SA, n, m, threads, thread_state);
            if (names < m) {
                sa_sint_t f = libsais_compact_lms_suffixes_32s_omp(T, SA, n, m, fs, threads, thread_state);

                if (libsais_main_32s(SA + n + fs - m + f, SA, m - f, names - f, fs + n - 2 * m + f, threads,
                                     thread_state) != 0) {
                    return -2;
                }

                libsais_reconstruct_compacted_lms_suffixes_32s_2k_omp(T, SA, n, k, m, fs, f, buckets, threads,
                                                                      thread_state);
            } else {
                libsais_count_lms_suffixes_32s_2k(T, n, k, buckets);
            }
        } else {
            SA[0] = SA[n - 1];
        }

        libsais_initialize_buckets_start_and_end_32s_4k(k, buckets);
        libsais_place_lms_suffixes_histogram_32s_4k(SA, n, k, m, buckets);
        libsais_induce_final_order_32s_4k(T, SA, n, k, buckets, threads, thread_state);

        return 0;
    } else if (k > 0 && fs / k >= 2) {
        sa_sint_t alignment = (fs - 1024) / k >= 2 ? 1024 : 16;
        sa_sint_t * RESTRICT buckets =
            (fs - alignment) / k >= 2
                ? (sa_sint_t *)libsais_align_up(&SA[n + fs - 2 * k - alignment], (size_t)alignment * sizeof(sa_sint_t))
                : &SA[n + fs - 2 * k];

        sa_sint_t m = libsais_count_and_gather_lms_suffixes_32s_2k_omp(T, SA, n, k, buckets, threads, thread_state);
        if (m > 1) {
            libsais_initialize_buckets_for_lms_suffixes_radix_sort_32s_2k(T, k, buckets, SA[n - m]);

            libsais_radix_sort_lms_suffixes_32s_2k_omp(T, SA, n, m, &buckets[1], threads, thread_state);
            libsais_place_lms_suffixes_interval_32s_2k(SA, n, k, m - 1, buckets);

            libsais_initialize_buckets_start_and_end_32s_2k(k, buckets);
            libsais_induce_partial_order_32s_2k_omp(T, SA, n, k, buckets, threads, thread_state);

            sa_sint_t names = libsais_renumber_and_mark_distinct_lms_suffixes_32s_1k_omp(T, SA, n, m, threads);
            if (names < m) {
                sa_sint_t f = libsais_compact_lms_suffixes_32s_omp(T, SA, n, m, fs, threads, thread_state);

                if (libsais_main_32s(SA + n + fs - m + f, SA, m - f, names - f, fs + n - 2 * m + f, threads,
                                     thread_state) != 0) {
                    return -2;
                }

                libsais_reconstruct_compacted_lms_suffixes_32s_2k_omp(T, SA, n, k, m, fs, f, buckets, threads,
                                                                      thread_state);
            } else {
                libsais_count_lms_suffixes_32s_2k(T, n, k, buckets);
            }
        } else {
            SA[0] = SA[n - 1];
        }

        libsais_initialize_buckets_end_32s_2k(k, buckets);
        libsais_place_lms_suffixes_histogram_32s_2k(SA, n, k, m, buckets);

        libsais_initialize_buckets_start_and_end_32s_2k(k, buckets);
        libsais_induce_final_order_32s_2k(T, SA, n, k, buckets, threads, thread_state);

        return 0;
    } else {
        sa_sint_t * buffer =
            fs < k ? (sa_sint_t *)libsais_alloc_aligned((size_t)k * sizeof(sa_sint_t), 4096) : (sa_sint_t *)NULL;

        sa_sint_t alignment = fs - 1024 >= k ? 1024 : 16;
        sa_sint_t * RESTRICT buckets =
            fs - alignment >= k
                ? (sa_sint_t *)libsais_align_up(&SA[n + fs - k - alignment], (size_t)alignment * sizeof(sa_sint_t))
            : fs >= k ? &SA[n + fs - k]
                      : buffer;

        if (buckets == NULL) {
            return -2;
        }

        memset(SA, 0, (size_t)n * sizeof(sa_sint_t));

        libsais_count_suffixes_32s(T, n, k, buckets);
        libsais_initialize_buckets_end_32s_1k(k, buckets);

        sa_sint_t m = libsais_radix_sort_lms_suffixes_32s_1k(T, SA, n, buckets);
        if (m > 1) {
            libsais_induce_partial_order_32s_1k_omp(T, SA, n, k, buckets, threads, thread_state);

            sa_sint_t names = libsais_renumber_and_mark_distinct_lms_suffixes_32s_1k_omp(T, SA, n, m, threads);
            if (names < m) {
                if (buffer != NULL) {
                    libsais_free_aligned(buffer);
                    buckets = NULL;
                }

                sa_sint_t f = libsais_compact_lms_suffixes_32s_omp(T, SA, n, m, fs, threads, thread_state);

                if (libsais_main_32s(SA + n + fs - m + f, SA, m - f, names - f, fs + n - 2 * m + f, threads,
                                     thread_state) != 0) {
                    return -2;
                }

                libsais_reconstruct_compacted_lms_suffixes_32s_1k_omp(T, SA, n, m, fs, f, threads, thread_state);

                if (buckets == NULL) {
                    buckets = buffer = (sa_sint_t *)libsais_alloc_aligned((size_t)k * sizeof(sa_sint_t), 4096);
                }
                if (buckets == NULL) {
                    return -2;
                }
            }

            libsais_count_suffixes_32s(T, n, k, buckets);
            libsais_initialize_buckets_end_32s_1k(k, buckets);
            libsais_place_lms_suffixes_interval_32s_1k(T, SA, k, m, buckets);
        }

        libsais_induce_final_order_32s_1k(T, SA, n, k, buckets, threads, thread_state);
        libsais_free_aligned(buffer);

        return 0;
    }
}

static sa_sint_t libsais_main_8u(const u8 * T, sa_sint_t * SA, sa_sint_t n, sa_sint_t * RESTRICT buckets, sa_sint_t bwt,
                                 sa_sint_t r, sa_sint_t * RESTRICT I, sa_sint_t fs, sa_sint_t * freq, sa_sint_t threads,
                                 LIBSAIS_THREAD_STATE * RESTRICT thread_state) {
    fs = fs < (SAINT_MAX - n) ? fs : (SAINT_MAX - n);

    sa_sint_t m = libsais_count_and_gather_lms_suffixes_8u_omp(T, SA, n, buckets, threads, thread_state);

    libsais_initialize_buckets_start_and_end_8u(buckets, freq);

    if (m > 0) {
        sa_sint_t first_lms_suffix = SA[n - m];
        sa_sint_t left_suffixes_count =
            libsais_initialize_buckets_for_lms_suffixes_radix_sort_8u(T, buckets, first_lms_suffix);

        if (threads > 1 && n >= 65536) {
            memset(SA, 0, ((size_t)n - (size_t)m) * sizeof(sa_sint_t));
        }
        libsais_radix_sort_lms_suffixes_8u_omp(T, SA, n, m, buckets, threads, thread_state);
        if (threads > 1 && n >= 65536) {
            memset(&SA[(fast_sint_t)n - (fast_sint_t)m], 0, (size_t)m * sizeof(sa_sint_t));
        }

        libsais_initialize_buckets_for_partial_sorting_8u(T, buckets, first_lms_suffix, left_suffixes_count);
        libsais_induce_partial_order_8u_omp(T, SA, n, buckets, first_lms_suffix, left_suffixes_count, threads,
                                            thread_state);

        sa_sint_t names = libsais_renumber_and_gather_lms_suffixes_8u_omp(SA, n, m, fs, threads, thread_state);
        if (names < m) {
            if (libsais_main_32s(SA + n + fs - m, SA, m, names, fs + n - 2 * m, threads, thread_state) != 0) {
                return -2;
            }

            libsais_gather_lms_suffixes_8u_omp(T, SA, n, threads, thread_state);
            libsais_reconstruct_lms_suffixes_omp(SA, n, m, threads);
        }

        libsais_place_lms_suffixes_interval_8u(SA, n, m, buckets);
    } else {
        memset(SA, 0, (size_t)n * sizeof(sa_sint_t));
    }

    return libsais_induce_final_order_8u_omp(T, SA, n, bwt, r, I, buckets, threads, thread_state);
}

static sa_sint_t libsais_main(const u8 * T, sa_sint_t * SA, sa_sint_t n, sa_sint_t bwt, sa_sint_t r, sa_sint_t * I,
                              sa_sint_t fs, sa_sint_t * freq, sa_sint_t threads) {
    LIBSAIS_THREAD_STATE * RESTRICT thread_state = threads > 1 ? libsais_alloc_thread_state(threads) : NULL;
    sa_sint_t * RESTRICT buckets = (sa_sint_t *)libsais_alloc_aligned(8 * ALPHABET_SIZE * sizeof(sa_sint_t), 4096);

    sa_sint_t index = buckets != NULL && (thread_state != NULL || threads == 1)
                          ? libsais_main_8u(T, SA, n, buckets, bwt, r, I, fs, freq, threads, thread_state)
                          : -2;

    libsais_free_aligned(buckets);
    libsais_free_thread_state(thread_state);

    return index;
}

static s32 libsais_main_int(sa_sint_t * T, sa_sint_t * SA, sa_sint_t n, sa_sint_t k, sa_sint_t fs, sa_sint_t threads) {
    LIBSAIS_THREAD_STATE * RESTRICT thread_state = threads > 1 ? libsais_alloc_thread_state(threads) : NULL;

    sa_sint_t index =
        thread_state != NULL || threads == 1 ? libsais_main_32s(T, SA, n, k, fs, threads, thread_state) : -2;

    libsais_free_thread_state(thread_state);

    return index;
}

static sa_sint_t libsais_main_ctx(const LIBSAIS_CONTEXT * ctx, const u8 * T, sa_sint_t * SA, sa_sint_t n, sa_sint_t bwt,
                                  sa_sint_t r, sa_sint_t * I, sa_sint_t fs, sa_sint_t * freq) {
    return ctx != NULL && (ctx->buckets != NULL && (ctx->thread_state != NULL || ctx->threads == 1))
               ? libsais_main_8u(T, SA, n, ctx->buckets, bwt, r, I, fs, freq, (sa_sint_t)ctx->threads,
                                 ctx->thread_state)
               : -2;
}

static void libsais_bwt_copy_8u(u8 * RESTRICT U, sa_sint_t * RESTRICT A, sa_sint_t n) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = 0, j = (fast_sint_t)n - 7; i < j; i += 8) {
        prefetch(&A[i + prefetch_distance]);

        U[i + 0] = (u8)A[i + 0];
        U[i + 1] = (u8)A[i + 1];
        U[i + 2] = (u8)A[i + 2];
        U[i + 3] = (u8)A[i + 3];
        U[i + 4] = (u8)A[i + 4];
        U[i + 5] = (u8)A[i + 5];
        U[i + 6] = (u8)A[i + 6];
        U[i + 7] = (u8)A[i + 7];
    }

    for (j += 7; i < j; i += 1) {
        U[i] = (u8)A[i];
    }
}
static void * libsais_create_ctx(void) { return (void *)libsais_create_ctx_main(1); }

static void libsais_free_ctx(void * ctx) { libsais_free_ctx_main((LIBSAIS_CONTEXT *)ctx); }

static s32 libsais(const u8 * T, s32 * SA, s32 n, s32 fs, s32 * freq) {
    if ((T == NULL) || (SA == NULL) || (n < 0) || (fs < 0)) {
        return -1;
    } else if (n < 2) {
        if (freq != NULL) {
            memset(freq, 0, ALPHABET_SIZE * sizeof(s32));
        }
        if (n == 1) {
            SA[0] = 0;
            if (freq != NULL) {
                freq[T[0]]++;
            }
        }
        return 0;
    }

    return libsais_main(T, SA, n, 0, 0, NULL, fs, freq, 1);
}

static s32 libsais_int(s32 * T, s32 * SA, s32 n, s32 k, s32 fs) {
    if ((T == NULL) || (SA == NULL) || (n < 0) || (fs < 0)) {
        return -1;
    } else if (n < 2) {
        if (n == 1) {
            SA[0] = 0;
        }
        return 0;
    }

    return libsais_main_int(T, SA, n, k, fs, 1);
}

static s32 libsais_ctx(const void * ctx, const u8 * T, s32 * SA, s32 n, s32 fs, s32 * freq) {
    if ((ctx == NULL) || (T == NULL) || (SA == NULL) || (n < 0) || (fs < 0)) {
        return -1;
    } else if (n < 2) {
        if (freq != NULL) {
            memset(freq, 0, ALPHABET_SIZE * sizeof(s32));
        }
        if (n == 1) {
            SA[0] = 0;
            if (freq != NULL) {
                freq[T[0]]++;
            }
        }
        return 0;
    }

    return libsais_main_ctx((const LIBSAIS_CONTEXT *)ctx, T, SA, n, 0, 0, NULL, fs, freq);
}

static s32 libsais_bwt(const u8 * T, u8 * U, s32 * A, s32 n, s32 fs, s32 * freq) {
    if ((T == NULL) || (U == NULL) || (A == NULL) || (n < 0) || (fs < 0)) {
        return -1;
    } else if (n <= 1) {
        if (freq != NULL) {
            memset(freq, 0, ALPHABET_SIZE * sizeof(s32));
        }
        if (n == 1) {
            U[0] = T[0];
            if (freq != NULL) {
                freq[T[0]]++;
            }
        }
        return n;
    }

    sa_sint_t index = libsais_main(T, A, n, 1, 0, NULL, fs, freq, 1);
    if (index >= 0) {
        index++;

        U[0] = T[n - 1];
        libsais_bwt_copy_8u(U + 1, A, index - 1);
        libsais_bwt_copy_8u(U + index, A + index, n - index);
    }

    return index;
}

static s32 libsais_bwt_aux(const u8 * T, u8 * U, s32 * A, s32 n, s32 fs, s32 * freq, s32 r, s32 * I) {
    if ((T == NULL) || (U == NULL) || (A == NULL) || (n < 0) || (fs < 0) || (r < 2) || ((r & (r - 1)) != 0) ||
        (I == NULL)) {
        return -1;
    } else if (n <= 1) {
        if (freq != NULL) {
            memset(freq, 0, ALPHABET_SIZE * sizeof(s32));
        }
        if (n == 1) {
            U[0] = T[0];
            if (freq != NULL) {
                freq[T[0]]++;
            }
        }
        I[0] = n;
        return 0;
    }

    if (libsais_main(T, A, n, 1, r, I, fs, freq, 1) != 0) {
        return -2;
    }

    U[0] = T[n - 1];
    libsais_bwt_copy_8u(U + 1, A, I[0] - 1);
    libsais_bwt_copy_8u(U + I[0], A + I[0], n - I[0]);

    return 0;
}

static s32 libsais_bwt_ctx(const void * ctx, const u8 * T, u8 * U, s32 * A, s32 n, s32 fs, s32 * freq) {
    if ((ctx == NULL) || (T == NULL) || (U == NULL) || (A == NULL) || (n < 0) || (fs < 0)) {
        return -1;
    } else if (n <= 1) {
        if (freq != NULL) {
            memset(freq, 0, ALPHABET_SIZE * sizeof(s32));
        }
        if (n == 1) {
            U[0] = T[0];
            if (freq != NULL) {
                freq[T[0]]++;
            }
        }
        return n;
    }

    sa_sint_t index = libsais_main_ctx((const LIBSAIS_CONTEXT *)ctx, T, A, n, 1, 0, NULL, fs, freq);
    if (index >= 0) {
        index++;

        U[0] = T[n - 1];

        libsais_bwt_copy_8u(U + 1, A, index - 1);
        libsais_bwt_copy_8u(U + index, A + index, n - index);
    }

    return index;
}

static s32 libsais_bwt_aux_ctx(const void * ctx, const u8 * T, u8 * U, s32 * A, s32 n, s32 fs, s32 * freq, s32 r,
                               s32 * I) {
    if ((ctx == NULL) || (T == NULL) || (U == NULL) || (A == NULL) || (n < 0) || (fs < 0) || (r < 2) ||
        ((r & (r - 1)) != 0) || (I == NULL)) {
        return -1;
    } else if (n <= 1) {
        if (freq != NULL) {
            memset(freq, 0, ALPHABET_SIZE * sizeof(s32));
        }
        if (n == 1) {
            U[0] = T[0];
            if (freq != NULL) {
                freq[T[0]]++;
            }
        }
        I[0] = n;
        return 0;
    }

    if (libsais_main_ctx((const LIBSAIS_CONTEXT *)ctx, T, A, n, 1, r, I, fs, freq) != 0) {
        return -2;
    }

    U[0] = T[n - 1];
    libsais_bwt_copy_8u(U + 1, A, I[0] - 1);
    libsais_bwt_copy_8u(U + I[0], A + I[0], n - I[0]);
    return 0;
}
static LIBSAIS_UNBWT_CONTEXT * libsais_unbwt_create_ctx_main(sa_sint_t threads) {
    LIBSAIS_UNBWT_CONTEXT * RESTRICT ctx =
        (LIBSAIS_UNBWT_CONTEXT *)libsais_alloc_aligned(sizeof(LIBSAIS_UNBWT_CONTEXT), 64);
    sa_uint_t * RESTRICT bucket2 =
        (sa_uint_t *)libsais_alloc_aligned(ALPHABET_SIZE * ALPHABET_SIZE * sizeof(sa_uint_t), 4096);
    u16 * RESTRICT fastbits = (u16 *)libsais_alloc_aligned((1 + (1 << UNBWT_FASTBITS)) * sizeof(u16), 4096);
    sa_uint_t * RESTRICT buckets =
        threads > 1 ? (sa_uint_t *)libsais_alloc_aligned(
                          (size_t)threads * (ALPHABET_SIZE + (ALPHABET_SIZE * ALPHABET_SIZE)) * sizeof(sa_uint_t), 4096)
                    : NULL;

    if (ctx != NULL && bucket2 != NULL && fastbits != NULL && (buckets != NULL || threads == 1)) {
        ctx->bucket2 = bucket2;
        ctx->fastbits = fastbits;
        ctx->buckets = buckets;
        ctx->threads = threads;

        return ctx;
    }

    libsais_free_aligned(buckets);
    libsais_free_aligned(fastbits);
    libsais_free_aligned(bucket2);
    libsais_free_aligned(ctx);

    return NULL;
}

static void libsais_unbwt_free_ctx_main(LIBSAIS_UNBWT_CONTEXT * ctx) {
    if (ctx != NULL) {
        libsais_free_aligned(ctx->buckets);
        libsais_free_aligned(ctx->fastbits);
        libsais_free_aligned(ctx->bucket2);
        libsais_free_aligned(ctx);
    }
}

static void libsais_unbwt_compute_histogram(const u8 * RESTRICT T, fast_sint_t n, sa_uint_t * RESTRICT count) {
    const fast_sint_t prefetch_distance = 256;

    const u8 * RESTRICT T_p = T;

    if (n >= 1024) {
        sa_uint_t copy[4 * (ALPHABET_SIZE + 16)];

        memset(copy, 0, 4 * (ALPHABET_SIZE + 16) * sizeof(sa_uint_t));

        sa_uint_t * RESTRICT copy0 = copy + 0 * (ALPHABET_SIZE + 16);
        sa_uint_t * RESTRICT copy1 = copy + 1 * (ALPHABET_SIZE + 16);
        sa_uint_t * RESTRICT copy2 = copy + 2 * (ALPHABET_SIZE + 16);
        sa_uint_t * RESTRICT copy3 = copy + 3 * (ALPHABET_SIZE + 16);

        for (; T_p < (u8 *)((ptrdiff_t)(T + 63) & (-64)); T_p += 1) {
            copy0[T_p[0]]++;
        }

        fast_uint_t x = ((const u32 *)(const void *)T_p)[0], y = ((const u32 *)(const void *)T_p)[1];

        for (; T_p < (u8 *)((ptrdiff_t)(T + n - 8) & (-64)); T_p += 64) {
            prefetch(&T_p[prefetch_distance]);

            fast_uint_t z = ((const u32 *)(const void *)T_p)[2], w = ((const u32 *)(const void *)T_p)[3];
            copy0[(u8)x]++;
            x >>= 8;
            copy1[(u8)x]++;
            x >>= 8;
            copy2[(u8)x]++;
            x >>= 8;
            copy3[x]++;
            copy0[(u8)y]++;
            y >>= 8;
            copy1[(u8)y]++;
            y >>= 8;
            copy2[(u8)y]++;
            y >>= 8;
            copy3[y]++;

            x = ((const u32 *)(const void *)T_p)[4];
            y = ((const u32 *)(const void *)T_p)[5];
            copy0[(u8)z]++;
            z >>= 8;
            copy1[(u8)z]++;
            z >>= 8;
            copy2[(u8)z]++;
            z >>= 8;
            copy3[z]++;
            copy0[(u8)w]++;
            w >>= 8;
            copy1[(u8)w]++;
            w >>= 8;
            copy2[(u8)w]++;
            w >>= 8;
            copy3[w]++;

            z = ((const u32 *)(const void *)T_p)[6];
            w = ((const u32 *)(const void *)T_p)[7];
            copy0[(u8)x]++;
            x >>= 8;
            copy1[(u8)x]++;
            x >>= 8;
            copy2[(u8)x]++;
            x >>= 8;
            copy3[x]++;
            copy0[(u8)y]++;
            y >>= 8;
            copy1[(u8)y]++;
            y >>= 8;
            copy2[(u8)y]++;
            y >>= 8;
            copy3[y]++;

            x = ((const u32 *)(const void *)T_p)[8];
            y = ((const u32 *)(const void *)T_p)[9];
            copy0[(u8)z]++;
            z >>= 8;
            copy1[(u8)z]++;
            z >>= 8;
            copy2[(u8)z]++;
            z >>= 8;
            copy3[z]++;
            copy0[(u8)w]++;
            w >>= 8;
            copy1[(u8)w]++;
            w >>= 8;
            copy2[(u8)w]++;
            w >>= 8;
            copy3[w]++;

            z = ((const u32 *)(const void *)T_p)[10];
            w = ((const u32 *)(const void *)T_p)[11];
            copy0[(u8)x]++;
            x >>= 8;
            copy1[(u8)x]++;
            x >>= 8;
            copy2[(u8)x]++;
            x >>= 8;
            copy3[x]++;
            copy0[(u8)y]++;
            y >>= 8;
            copy1[(u8)y]++;
            y >>= 8;
            copy2[(u8)y]++;
            y >>= 8;
            copy3[y]++;

            x = ((const u32 *)(const void *)T_p)[12];
            y = ((const u32 *)(const void *)T_p)[13];
            copy0[(u8)z]++;
            z >>= 8;
            copy1[(u8)z]++;
            z >>= 8;
            copy2[(u8)z]++;
            z >>= 8;
            copy3[z]++;
            copy0[(u8)w]++;
            w >>= 8;
            copy1[(u8)w]++;
            w >>= 8;
            copy2[(u8)w]++;
            w >>= 8;
            copy3[w]++;

            z = ((const u32 *)(const void *)T_p)[14];
            w = ((const u32 *)(const void *)T_p)[15];
            copy0[(u8)x]++;
            x >>= 8;
            copy1[(u8)x]++;
            x >>= 8;
            copy2[(u8)x]++;
            x >>= 8;
            copy3[x]++;
            copy0[(u8)y]++;
            y >>= 8;
            copy1[(u8)y]++;
            y >>= 8;
            copy2[(u8)y]++;
            y >>= 8;
            copy3[y]++;

            x = ((const u32 *)(const void *)T_p)[16];
            y = ((const u32 *)(const void *)T_p)[17];
            copy0[(u8)z]++;
            z >>= 8;
            copy1[(u8)z]++;
            z >>= 8;
            copy2[(u8)z]++;
            z >>= 8;
            copy3[z]++;
            copy0[(u8)w]++;
            w >>= 8;
            copy1[(u8)w]++;
            w >>= 8;
            copy2[(u8)w]++;
            w >>= 8;
            copy3[w]++;
        }

        copy0[(u8)x]++;
        x >>= 8;
        copy1[(u8)x]++;
        x >>= 8;
        copy2[(u8)x]++;
        x >>= 8;
        copy3[x]++;
        copy0[(u8)y]++;
        y >>= 8;
        copy1[(u8)y]++;
        y >>= 8;
        copy2[(u8)y]++;
        y >>= 8;
        copy3[y]++;

        T_p += 8;

        fast_uint_t i;
        for (i = 0; i < ALPHABET_SIZE; i++) {
            count[i] += copy0[i] + copy1[i] + copy2[i] + copy3[i];
        }
    }

    for (; T_p < T + n; T_p += 1) {
        count[T_p[0]]++;
    }
}

static void libsais_unbwt_transpose_bucket2(sa_uint_t * RESTRICT bucket2) {
    fast_uint_t x, y, c, d;
    for (x = 0; x != ALPHABET_SIZE; x += 16) {
        for (c = x; c != x + 16; ++c) {
            for (d = c + 1; d != x + 16; ++d) {
                sa_uint_t tmp = bucket2[(d << 8) + c];
                bucket2[(d << 8) + c] = bucket2[(c << 8) + d];
                bucket2[(c << 8) + d] = tmp;
            }
        }

        for (y = x + 16; y != ALPHABET_SIZE; y += 16) {
            for (c = x; c != x + 16; ++c) {
                sa_uint_t * bucket2_yc = &bucket2[(y << 8) + c];
                sa_uint_t * bucket2_cy = &bucket2[(c << 8) + y];

                sa_uint_t tmp00 = bucket2_yc[0 * 256];
                bucket2_yc[0 * 256] = bucket2_cy[0];
                bucket2_cy[0] = tmp00;
                sa_uint_t tmp01 = bucket2_yc[1 * 256];
                bucket2_yc[1 * 256] = bucket2_cy[1];
                bucket2_cy[1] = tmp01;
                sa_uint_t tmp02 = bucket2_yc[2 * 256];
                bucket2_yc[2 * 256] = bucket2_cy[2];
                bucket2_cy[2] = tmp02;
                sa_uint_t tmp03 = bucket2_yc[3 * 256];
                bucket2_yc[3 * 256] = bucket2_cy[3];
                bucket2_cy[3] = tmp03;
                sa_uint_t tmp04 = bucket2_yc[4 * 256];
                bucket2_yc[4 * 256] = bucket2_cy[4];
                bucket2_cy[4] = tmp04;
                sa_uint_t tmp05 = bucket2_yc[5 * 256];
                bucket2_yc[5 * 256] = bucket2_cy[5];
                bucket2_cy[5] = tmp05;
                sa_uint_t tmp06 = bucket2_yc[6 * 256];
                bucket2_yc[6 * 256] = bucket2_cy[6];
                bucket2_cy[6] = tmp06;
                sa_uint_t tmp07 = bucket2_yc[7 * 256];
                bucket2_yc[7 * 256] = bucket2_cy[7];
                bucket2_cy[7] = tmp07;
                sa_uint_t tmp08 = bucket2_yc[8 * 256];
                bucket2_yc[8 * 256] = bucket2_cy[8];
                bucket2_cy[8] = tmp08;
                sa_uint_t tmp09 = bucket2_yc[9 * 256];
                bucket2_yc[9 * 256] = bucket2_cy[9];
                bucket2_cy[9] = tmp09;
                sa_uint_t tmp10 = bucket2_yc[10 * 256];
                bucket2_yc[10 * 256] = bucket2_cy[10];
                bucket2_cy[10] = tmp10;
                sa_uint_t tmp11 = bucket2_yc[11 * 256];
                bucket2_yc[11 * 256] = bucket2_cy[11];
                bucket2_cy[11] = tmp11;
                sa_uint_t tmp12 = bucket2_yc[12 * 256];
                bucket2_yc[12 * 256] = bucket2_cy[12];
                bucket2_cy[12] = tmp12;
                sa_uint_t tmp13 = bucket2_yc[13 * 256];
                bucket2_yc[13 * 256] = bucket2_cy[13];
                bucket2_cy[13] = tmp13;
                sa_uint_t tmp14 = bucket2_yc[14 * 256];
                bucket2_yc[14 * 256] = bucket2_cy[14];
                bucket2_cy[14] = tmp14;
                sa_uint_t tmp15 = bucket2_yc[15 * 256];
                bucket2_yc[15 * 256] = bucket2_cy[15];
                bucket2_cy[15] = tmp15;
            }
        }
    }
}

static void libsais_unbwt_compute_bigram_histogram_single(const u8 * RESTRICT T, sa_uint_t * RESTRICT bucket1,
                                                          sa_uint_t * RESTRICT bucket2, fast_uint_t index) {
    fast_uint_t sum, c;
    for (sum = 1, c = 0; c < ALPHABET_SIZE; ++c) {
        fast_uint_t prev = sum;
        sum += bucket1[c];
        bucket1[c] = (sa_uint_t)prev;
        if (prev != sum) {
            sa_uint_t * RESTRICT bucket2_p = &bucket2[c << 8];

            {
                fast_uint_t hi = index;
                if (sum < hi) {
                    hi = sum;
                }
                libsais_unbwt_compute_histogram(&T[prev], (fast_sint_t)(hi - prev), bucket2_p);
            }

            {
                fast_uint_t lo = index + 1;
                if (prev > lo) {
                    lo = prev;
                }
                libsais_unbwt_compute_histogram(&T[lo - 1], (fast_sint_t)(sum - lo), bucket2_p);
            }
        }
    }

    libsais_unbwt_transpose_bucket2(bucket2);
}

static void libsais_unbwt_calculate_fastbits(sa_uint_t * RESTRICT bucket2, u16 * RESTRICT fastbits, fast_uint_t lastc,
                                             fast_uint_t shift) {
    fast_uint_t v, w, sum, c, d;
    for (v = 0, w = 0, sum = 1, c = 0; c < ALPHABET_SIZE; ++c) {
        if (c == lastc) {
            sum += 1;
        }

        for (d = 0; d < ALPHABET_SIZE; ++d, ++w) {
            fast_uint_t prev = sum;
            sum += bucket2[w];
            bucket2[w] = (sa_uint_t)prev;
            if (prev != sum) {
                for (; v <= ((sum - 1) >> shift); ++v) {
                    fastbits[v] = (u16)w;
                }
            }
        }
    }
}

static void libsais_unbwt_calculate_biPSI(const u8 * RESTRICT T, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket1,
                                          sa_uint_t * RESTRICT bucket2, fast_uint_t index, fast_sint_t omp_block_start,
                                          fast_sint_t omp_block_end) {
    {
        fast_sint_t i = omp_block_start, j = (fast_sint_t)index;
        if (omp_block_end < j) {
            j = omp_block_end;
        }
        for (; i < j; ++i) {
            fast_uint_t c = T[i];
            fast_uint_t p = bucket1[c]++;
            fast_sint_t t = (fast_sint_t)(index - p);

            if (t != 0) {
                fast_uint_t w = (((fast_uint_t)T[p + (fast_uint_t)(t >> ((sizeof(fast_sint_t) * 8) - 1))]) << 8) + c;
                P[bucket2[w]++] = (sa_uint_t)i;
            }
        }
    }

    {
        fast_sint_t i = (fast_sint_t)index, j = omp_block_end;
        if (omp_block_start > i) {
            i = omp_block_start;
        }
        for (i += 1; i <= j; ++i) {
            fast_uint_t c = T[i - 1];
            fast_uint_t p = bucket1[c]++;
            fast_sint_t t = (fast_sint_t)(index - p);

            if (t != 0) {
                fast_uint_t w = (((fast_uint_t)T[p + (fast_uint_t)(t >> ((sizeof(fast_sint_t) * 8) - 1))]) << 8) + c;
                P[bucket2[w]++] = (sa_uint_t)i;
            }
        }
    }
}

static void libsais_unbwt_init_single(const u8 * RESTRICT T, sa_uint_t * RESTRICT P, sa_sint_t n,
                                      const sa_sint_t * freq, const sa_uint_t * RESTRICT I,
                                      sa_uint_t * RESTRICT bucket2, u16 * RESTRICT fastbits) {
    sa_uint_t bucket1[ALPHABET_SIZE];

    fast_uint_t index = I[0];
    fast_uint_t lastc = T[0];
    fast_uint_t shift = 0;
    while ((n >> shift) > (1 << UNBWT_FASTBITS)) {
        shift++;
    }

    if (freq != NULL) {
        memcpy(bucket1, freq, ALPHABET_SIZE * sizeof(sa_uint_t));
    } else {
        memset(bucket1, 0, ALPHABET_SIZE * sizeof(sa_uint_t));
        libsais_unbwt_compute_histogram(T, n, bucket1);
    }

    memset(bucket2, 0, ALPHABET_SIZE * ALPHABET_SIZE * sizeof(sa_uint_t));
    libsais_unbwt_compute_bigram_histogram_single(T, bucket1, bucket2, index);

    libsais_unbwt_calculate_fastbits(bucket2, fastbits, lastc, shift);
    libsais_unbwt_calculate_biPSI(T, P, bucket1, bucket2, index, 0, n);
}
static void libsais_unbwt_decode_1(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket2,
                                   u16 * RESTRICT fastbits, fast_uint_t shift, fast_uint_t * i0, fast_uint_t k) {
    u16 * RESTRICT U0 = (u16 *)(void *)U;

    fast_uint_t i, p0 = *i0;

    for (i = 0; i != k; ++i) {
        u16 c0 = fastbits[p0 >> shift];
        if (bucket2[c0] <= p0) {
            do {
                c0++;
            } while (bucket2[c0] <= p0);
        }
        p0 = P[p0];
        U0[i] = bswap16(c0);
    }

    *i0 = p0;
}

static void libsais_unbwt_decode_2(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket2,
                                   u16 * RESTRICT fastbits, fast_uint_t shift, fast_uint_t r, fast_uint_t * i0,
                                   fast_uint_t * i1, fast_uint_t k) {
    u16 * RESTRICT U0 = (u16 *)(void *)U;
    u16 * RESTRICT U1 = (u16 *)(void *)(((u8 *)U0) + r);

    fast_uint_t i, p0 = *i0, p1 = *i1;

    for (i = 0; i != k; ++i) {
        u16 c0 = fastbits[p0 >> shift];
        if (bucket2[c0] <= p0) {
            do {
                c0++;
            } while (bucket2[c0] <= p0);
        }
        p0 = P[p0];
        U0[i] = bswap16(c0);
        u16 c1 = fastbits[p1 >> shift];
        if (bucket2[c1] <= p1) {
            do {
                c1++;
            } while (bucket2[c1] <= p1);
        }
        p1 = P[p1];
        U1[i] = bswap16(c1);
    }

    *i0 = p0;
    *i1 = p1;
}

static void libsais_unbwt_decode_3(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket2,
                                   u16 * RESTRICT fastbits, fast_uint_t shift, fast_uint_t r, fast_uint_t * i0,
                                   fast_uint_t * i1, fast_uint_t * i2, fast_uint_t k) {
    u16 * RESTRICT U0 = (u16 *)(void *)U;
    u16 * RESTRICT U1 = (u16 *)(void *)(((u8 *)U0) + r);
    u16 * RESTRICT U2 = (u16 *)(void *)(((u8 *)U1) + r);

    fast_uint_t i, p0 = *i0, p1 = *i1, p2 = *i2;

    for (i = 0; i != k; ++i) {
        u16 c0 = fastbits[p0 >> shift];
        if (bucket2[c0] <= p0) {
            do {
                c0++;
            } while (bucket2[c0] <= p0);
        }
        p0 = P[p0];
        U0[i] = bswap16(c0);
        u16 c1 = fastbits[p1 >> shift];
        if (bucket2[c1] <= p1) {
            do {
                c1++;
            } while (bucket2[c1] <= p1);
        }
        p1 = P[p1];
        U1[i] = bswap16(c1);
        u16 c2 = fastbits[p2 >> shift];
        if (bucket2[c2] <= p2) {
            do {
                c2++;
            } while (bucket2[c2] <= p2);
        }
        p2 = P[p2];
        U2[i] = bswap16(c2);
    }

    *i0 = p0;
    *i1 = p1;
    *i2 = p2;
}

static void libsais_unbwt_decode_4(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket2,
                                   u16 * RESTRICT fastbits, fast_uint_t shift, fast_uint_t r, fast_uint_t * i0,
                                   fast_uint_t * i1, fast_uint_t * i2, fast_uint_t * i3, fast_uint_t k) {
    u16 * RESTRICT U0 = (u16 *)(void *)U;
    u16 * RESTRICT U1 = (u16 *)(void *)(((u8 *)U0) + r);
    u16 * RESTRICT U2 = (u16 *)(void *)(((u8 *)U1) + r);
    u16 * RESTRICT U3 = (u16 *)(void *)(((u8 *)U2) + r);

    fast_uint_t i, p0 = *i0, p1 = *i1, p2 = *i2, p3 = *i3;

    for (i = 0; i != k; ++i) {
        u16 c0 = fastbits[p0 >> shift];
        if (bucket2[c0] <= p0) {
            do {
                c0++;
            } while (bucket2[c0] <= p0);
        }
        p0 = P[p0];
        U0[i] = bswap16(c0);
        u16 c1 = fastbits[p1 >> shift];
        if (bucket2[c1] <= p1) {
            do {
                c1++;
            } while (bucket2[c1] <= p1);
        }
        p1 = P[p1];
        U1[i] = bswap16(c1);
        u16 c2 = fastbits[p2 >> shift];
        if (bucket2[c2] <= p2) {
            do {
                c2++;
            } while (bucket2[c2] <= p2);
        }
        p2 = P[p2];
        U2[i] = bswap16(c2);
        u16 c3 = fastbits[p3 >> shift];
        if (bucket2[c3] <= p3) {
            do {
                c3++;
            } while (bucket2[c3] <= p3);
        }
        p3 = P[p3];
        U3[i] = bswap16(c3);
    }

    *i0 = p0;
    *i1 = p1;
    *i2 = p2;
    *i3 = p3;
}

static void libsais_unbwt_decode_5(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket2,
                                   u16 * RESTRICT fastbits, fast_uint_t shift, fast_uint_t r, fast_uint_t * i0,
                                   fast_uint_t * i1, fast_uint_t * i2, fast_uint_t * i3, fast_uint_t * i4,
                                   fast_uint_t k) {
    u16 * RESTRICT U0 = (u16 *)(void *)U;
    u16 * RESTRICT U1 = (u16 *)(void *)(((u8 *)U0) + r);
    u16 * RESTRICT U2 = (u16 *)(void *)(((u8 *)U1) + r);
    u16 * RESTRICT U3 = (u16 *)(void *)(((u8 *)U2) + r);
    u16 * RESTRICT U4 = (u16 *)(void *)(((u8 *)U3) + r);

    fast_uint_t i, p0 = *i0, p1 = *i1, p2 = *i2, p3 = *i3, p4 = *i4;

    for (i = 0; i != k; ++i) {
        u16 c0 = fastbits[p0 >> shift];
        if (bucket2[c0] <= p0) {
            do {
                c0++;
            } while (bucket2[c0] <= p0);
        }
        p0 = P[p0];
        U0[i] = bswap16(c0);
        u16 c1 = fastbits[p1 >> shift];
        if (bucket2[c1] <= p1) {
            do {
                c1++;
            } while (bucket2[c1] <= p1);
        }
        p1 = P[p1];
        U1[i] = bswap16(c1);
        u16 c2 = fastbits[p2 >> shift];
        if (bucket2[c2] <= p2) {
            do {
                c2++;
            } while (bucket2[c2] <= p2);
        }
        p2 = P[p2];
        U2[i] = bswap16(c2);
        u16 c3 = fastbits[p3 >> shift];
        if (bucket2[c3] <= p3) {
            do {
                c3++;
            } while (bucket2[c3] <= p3);
        }
        p3 = P[p3];
        U3[i] = bswap16(c3);
        u16 c4 = fastbits[p4 >> shift];
        if (bucket2[c4] <= p4) {
            do {
                c4++;
            } while (bucket2[c4] <= p4);
        }
        p4 = P[p4];
        U4[i] = bswap16(c4);
    }

    *i0 = p0;
    *i1 = p1;
    *i2 = p2;
    *i3 = p3;
    *i4 = p4;
}

static void libsais_unbwt_decode_6(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket2,
                                   u16 * RESTRICT fastbits, fast_uint_t shift, fast_uint_t r, fast_uint_t * i0,
                                   fast_uint_t * i1, fast_uint_t * i2, fast_uint_t * i3, fast_uint_t * i4,
                                   fast_uint_t * i5, fast_uint_t k) {
    u16 * RESTRICT U0 = (u16 *)(void *)U;
    u16 * RESTRICT U1 = (u16 *)(void *)(((u8 *)U0) + r);
    u16 * RESTRICT U2 = (u16 *)(void *)(((u8 *)U1) + r);
    u16 * RESTRICT U3 = (u16 *)(void *)(((u8 *)U2) + r);
    u16 * RESTRICT U4 = (u16 *)(void *)(((u8 *)U3) + r);
    u16 * RESTRICT U5 = (u16 *)(void *)(((u8 *)U4) + r);

    fast_uint_t i, p0 = *i0, p1 = *i1, p2 = *i2, p3 = *i3, p4 = *i4, p5 = *i5;

    for (i = 0; i != k; ++i) {
        u16 c0 = fastbits[p0 >> shift];
        if (bucket2[c0] <= p0) {
            do {
                c0++;
            } while (bucket2[c0] <= p0);
        }
        p0 = P[p0];
        U0[i] = bswap16(c0);
        u16 c1 = fastbits[p1 >> shift];
        if (bucket2[c1] <= p1) {
            do {
                c1++;
            } while (bucket2[c1] <= p1);
        }
        p1 = P[p1];
        U1[i] = bswap16(c1);
        u16 c2 = fastbits[p2 >> shift];
        if (bucket2[c2] <= p2) {
            do {
                c2++;
            } while (bucket2[c2] <= p2);
        }
        p2 = P[p2];
        U2[i] = bswap16(c2);
        u16 c3 = fastbits[p3 >> shift];
        if (bucket2[c3] <= p3) {
            do {
                c3++;
            } while (bucket2[c3] <= p3);
        }
        p3 = P[p3];
        U3[i] = bswap16(c3);
        u16 c4 = fastbits[p4 >> shift];
        if (bucket2[c4] <= p4) {
            do {
                c4++;
            } while (bucket2[c4] <= p4);
        }
        p4 = P[p4];
        U4[i] = bswap16(c4);
        u16 c5 = fastbits[p5 >> shift];
        if (bucket2[c5] <= p5) {
            do {
                c5++;
            } while (bucket2[c5] <= p5);
        }
        p5 = P[p5];
        U5[i] = bswap16(c5);
    }

    *i0 = p0;
    *i1 = p1;
    *i2 = p2;
    *i3 = p3;
    *i4 = p4;
    *i5 = p5;
}

static void libsais_unbwt_decode_7(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket2,
                                   u16 * RESTRICT fastbits, fast_uint_t shift, fast_uint_t r, fast_uint_t * i0,
                                   fast_uint_t * i1, fast_uint_t * i2, fast_uint_t * i3, fast_uint_t * i4,
                                   fast_uint_t * i5, fast_uint_t * i6, fast_uint_t k) {
    u16 * RESTRICT U0 = (u16 *)(void *)U;
    u16 * RESTRICT U1 = (u16 *)(void *)(((u8 *)U0) + r);
    u16 * RESTRICT U2 = (u16 *)(void *)(((u8 *)U1) + r);
    u16 * RESTRICT U3 = (u16 *)(void *)(((u8 *)U2) + r);
    u16 * RESTRICT U4 = (u16 *)(void *)(((u8 *)U3) + r);
    u16 * RESTRICT U5 = (u16 *)(void *)(((u8 *)U4) + r);
    u16 * RESTRICT U6 = (u16 *)(void *)(((u8 *)U5) + r);

    fast_uint_t i, p0 = *i0, p1 = *i1, p2 = *i2, p3 = *i3, p4 = *i4, p5 = *i5, p6 = *i6;

    for (i = 0; i != k; ++i) {
        u16 c0 = fastbits[p0 >> shift];
        if (bucket2[c0] <= p0) {
            do {
                c0++;
            } while (bucket2[c0] <= p0);
        }
        p0 = P[p0];
        U0[i] = bswap16(c0);
        u16 c1 = fastbits[p1 >> shift];
        if (bucket2[c1] <= p1) {
            do {
                c1++;
            } while (bucket2[c1] <= p1);
        }
        p1 = P[p1];
        U1[i] = bswap16(c1);
        u16 c2 = fastbits[p2 >> shift];
        if (bucket2[c2] <= p2) {
            do {
                c2++;
            } while (bucket2[c2] <= p2);
        }
        p2 = P[p2];
        U2[i] = bswap16(c2);
        u16 c3 = fastbits[p3 >> shift];
        if (bucket2[c3] <= p3) {
            do {
                c3++;
            } while (bucket2[c3] <= p3);
        }
        p3 = P[p3];
        U3[i] = bswap16(c3);
        u16 c4 = fastbits[p4 >> shift];
        if (bucket2[c4] <= p4) {
            do {
                c4++;
            } while (bucket2[c4] <= p4);
        }
        p4 = P[p4];
        U4[i] = bswap16(c4);
        u16 c5 = fastbits[p5 >> shift];
        if (bucket2[c5] <= p5) {
            do {
                c5++;
            } while (bucket2[c5] <= p5);
        }
        p5 = P[p5];
        U5[i] = bswap16(c5);
        u16 c6 = fastbits[p6 >> shift];
        if (bucket2[c6] <= p6) {
            do {
                c6++;
            } while (bucket2[c6] <= p6);
        }
        p6 = P[p6];
        U6[i] = bswap16(c6);
    }

    *i0 = p0;
    *i1 = p1;
    *i2 = p2;
    *i3 = p3;
    *i4 = p4;
    *i5 = p5;
    *i6 = p6;
}

static void libsais_unbwt_decode_8(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_uint_t * RESTRICT bucket2,
                                   u16 * RESTRICT fastbits, fast_uint_t shift, fast_uint_t r, fast_uint_t * i0,
                                   fast_uint_t * i1, fast_uint_t * i2, fast_uint_t * i3, fast_uint_t * i4,
                                   fast_uint_t * i5, fast_uint_t * i6, fast_uint_t * i7, fast_uint_t k) {
    u16 * RESTRICT U0 = (u16 *)(void *)U;
    u16 * RESTRICT U1 = (u16 *)(void *)(((u8 *)U0) + r);
    u16 * RESTRICT U2 = (u16 *)(void *)(((u8 *)U1) + r);
    u16 * RESTRICT U3 = (u16 *)(void *)(((u8 *)U2) + r);
    u16 * RESTRICT U4 = (u16 *)(void *)(((u8 *)U3) + r);
    u16 * RESTRICT U5 = (u16 *)(void *)(((u8 *)U4) + r);
    u16 * RESTRICT U6 = (u16 *)(void *)(((u8 *)U5) + r);
    u16 * RESTRICT U7 = (u16 *)(void *)(((u8 *)U6) + r);

    fast_uint_t i, p0 = *i0, p1 = *i1, p2 = *i2, p3 = *i3, p4 = *i4, p5 = *i5, p6 = *i6, p7 = *i7;

    for (i = 0; i != k; ++i) {
        u16 c0 = fastbits[p0 >> shift];
        if (bucket2[c0] <= p0) {
            do {
                c0++;
            } while (bucket2[c0] <= p0);
        }
        p0 = P[p0];
        U0[i] = bswap16(c0);
        u16 c1 = fastbits[p1 >> shift];
        if (bucket2[c1] <= p1) {
            do {
                c1++;
            } while (bucket2[c1] <= p1);
        }
        p1 = P[p1];
        U1[i] = bswap16(c1);
        u16 c2 = fastbits[p2 >> shift];
        if (bucket2[c2] <= p2) {
            do {
                c2++;
            } while (bucket2[c2] <= p2);
        }
        p2 = P[p2];
        U2[i] = bswap16(c2);
        u16 c3 = fastbits[p3 >> shift];
        if (bucket2[c3] <= p3) {
            do {
                c3++;
            } while (bucket2[c3] <= p3);
        }
        p3 = P[p3];
        U3[i] = bswap16(c3);
        u16 c4 = fastbits[p4 >> shift];
        if (bucket2[c4] <= p4) {
            do {
                c4++;
            } while (bucket2[c4] <= p4);
        }
        p4 = P[p4];
        U4[i] = bswap16(c4);
        u16 c5 = fastbits[p5 >> shift];
        if (bucket2[c5] <= p5) {
            do {
                c5++;
            } while (bucket2[c5] <= p5);
        }
        p5 = P[p5];
        U5[i] = bswap16(c5);
        u16 c6 = fastbits[p6 >> shift];
        if (bucket2[c6] <= p6) {
            do {
                c6++;
            } while (bucket2[c6] <= p6);
        }
        p6 = P[p6];
        U6[i] = bswap16(c6);
        u16 c7 = fastbits[p7 >> shift];
        if (bucket2[c7] <= p7) {
            do {
                c7++;
            } while (bucket2[c7] <= p7);
        }
        p7 = P[p7];
        U7[i] = bswap16(c7);
    }

    *i0 = p0;
    *i1 = p1;
    *i2 = p2;
    *i3 = p3;
    *i4 = p4;
    *i5 = p5;
    *i6 = p6;
    *i7 = p7;
}

static void libsais_unbwt_decode(u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_sint_t n, sa_sint_t r,
                                 const sa_uint_t * RESTRICT I, sa_uint_t * RESTRICT bucket2, u16 * RESTRICT fastbits,
                                 fast_sint_t blocks, fast_uint_t reminder) {
    fast_uint_t shift = 0;
    while ((n >> shift) > (1 << UNBWT_FASTBITS)) {
        shift++;
    }
    fast_uint_t offset = 0;

    while (blocks > 8) {
        fast_uint_t i0 = I[0], i1 = I[1], i2 = I[2], i3 = I[3], i4 = I[4], i5 = I[5], i6 = I[6], i7 = I[7];
        libsais_unbwt_decode_8(U + offset, P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1, &i2, &i3, &i4, &i5,
                               &i6, &i7, (fast_uint_t)r >> 1);
        I += 8;
        blocks -= 8;
        offset += 8 * (fast_uint_t)r;
    }

    if (blocks == 1) {
        fast_uint_t i0 = I[0];
        libsais_unbwt_decode_1(U + offset, P, bucket2, fastbits, shift, &i0, reminder >> 1);
    } else if (blocks == 2) {
        fast_uint_t i0 = I[0], i1 = I[1];
        libsais_unbwt_decode_2(U + offset, P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1, reminder >> 1);
        libsais_unbwt_decode_1(U + offset + 2 * (reminder >> 1), P, bucket2, fastbits, shift, &i0,
                               ((fast_uint_t)r >> 1) - (reminder >> 1));
    } else if (blocks == 3) {
        fast_uint_t i0 = I[0], i1 = I[1], i2 = I[2];
        libsais_unbwt_decode_3(U + offset, P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1, &i2, reminder >> 1);
        libsais_unbwt_decode_2(U + offset + 2 * (reminder >> 1), P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1,
                               ((fast_uint_t)r >> 1) - (reminder >> 1));
    } else if (blocks == 4) {
        fast_uint_t i0 = I[0], i1 = I[1], i2 = I[2], i3 = I[3];
        libsais_unbwt_decode_4(U + offset, P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1, &i2, &i3,
                               reminder >> 1);
        libsais_unbwt_decode_3(U + offset + 2 * (reminder >> 1), P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1,
                               &i2, ((fast_uint_t)r >> 1) - (reminder >> 1));
    } else if (blocks == 5) {
        fast_uint_t i0 = I[0], i1 = I[1], i2 = I[2], i3 = I[3], i4 = I[4];
        libsais_unbwt_decode_5(U + offset, P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1, &i2, &i3, &i4,
                               reminder >> 1);
        libsais_unbwt_decode_4(U + offset + 2 * (reminder >> 1), P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1,
                               &i2, &i3, ((fast_uint_t)r >> 1) - (reminder >> 1));
    } else if (blocks == 6) {
        fast_uint_t i0 = I[0], i1 = I[1], i2 = I[2], i3 = I[3], i4 = I[4], i5 = I[5];
        libsais_unbwt_decode_6(U + offset, P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1, &i2, &i3, &i4, &i5,
                               reminder >> 1);
        libsais_unbwt_decode_5(U + offset + 2 * (reminder >> 1), P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1,
                               &i2, &i3, &i4, ((fast_uint_t)r >> 1) - (reminder >> 1));
    } else if (blocks == 7) {
        fast_uint_t i0 = I[0], i1 = I[1], i2 = I[2], i3 = I[3], i4 = I[4], i5 = I[5], i6 = I[6];
        libsais_unbwt_decode_7(U + offset, P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1, &i2, &i3, &i4, &i5,
                               &i6, reminder >> 1);
        libsais_unbwt_decode_6(U + offset + 2 * (reminder >> 1), P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1,
                               &i2, &i3, &i4, &i5, ((fast_uint_t)r >> 1) - (reminder >> 1));
    } else {
        fast_uint_t i0 = I[0], i1 = I[1], i2 = I[2], i3 = I[3], i4 = I[4], i5 = I[5], i6 = I[6], i7 = I[7];
        libsais_unbwt_decode_8(U + offset, P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1, &i2, &i3, &i4, &i5,
                               &i6, &i7, reminder >> 1);
        libsais_unbwt_decode_7(U + offset + 2 * (reminder >> 1), P, bucket2, fastbits, shift, (fast_uint_t)r, &i0, &i1,
                               &i2, &i3, &i4, &i5, &i6, ((fast_uint_t)r >> 1) - (reminder >> 1));
    }
}

static void libsais_unbwt_decode_omp(const u8 * RESTRICT T, u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_sint_t n,
                                     sa_sint_t r, const sa_uint_t * RESTRICT I, sa_uint_t * RESTRICT bucket2,
                                     u16 * RESTRICT fastbits, sa_sint_t threads) {
    fast_uint_t lastc = T[0];
    fast_sint_t blocks = 1 + (((fast_sint_t)n - 1) / (fast_sint_t)r);
    fast_uint_t reminder = (fast_uint_t)n - ((fast_uint_t)r * ((fast_uint_t)blocks - 1));

    {
        (void)(threads);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;
        fast_sint_t omp_block_stride = blocks / omp_num_threads;
        fast_sint_t omp_block_reminder = blocks % omp_num_threads;
        fast_sint_t omp_block_size = omp_block_stride + (omp_thread_num < omp_block_reminder);
        fast_sint_t omp_block_start = omp_block_stride * omp_thread_num +
                                      (omp_thread_num < omp_block_reminder ? omp_thread_num : omp_block_reminder);

        libsais_unbwt_decode(U + r * omp_block_start, P, n, r, I + omp_block_start, bucket2, fastbits, omp_block_size,
                             omp_thread_num < omp_num_threads - 1 ? (fast_uint_t)r : reminder);
    }

    U[n - 1] = (u8)lastc;
}

static sa_sint_t libsais_unbwt_core(const u8 * RESTRICT T, u8 * RESTRICT U, sa_uint_t * RESTRICT P, sa_sint_t n,
                                    const sa_sint_t * freq, sa_sint_t r, const sa_uint_t * RESTRICT I,
                                    sa_uint_t * RESTRICT bucket2, u16 * RESTRICT fastbits, sa_uint_t * RESTRICT buckets,
                                    sa_sint_t threads) {
    (void)(buckets);

    { libsais_unbwt_init_single(T, P, n, freq, I, bucket2, fastbits); }

    libsais_unbwt_decode_omp(T, U, P, n, r, I, bucket2, fastbits, threads);
    return 0;
}

static sa_sint_t libsais_unbwt_main(const u8 * T, u8 * U, sa_uint_t * P, sa_sint_t n, const sa_sint_t * freq,
                                    sa_sint_t r, const sa_uint_t * I, sa_sint_t threads) {
    fast_uint_t shift = 0;
    while ((n >> shift) > (1 << UNBWT_FASTBITS)) {
        shift++;
    }

    sa_uint_t * RESTRICT bucket2 =
        (sa_uint_t *)libsais_alloc_aligned(ALPHABET_SIZE * ALPHABET_SIZE * sizeof(sa_uint_t), 4096);
    u16 * RESTRICT fastbits = (u16 *)libsais_alloc_aligned(((size_t)1 + (size_t)(n >> shift)) * sizeof(u16), 4096);
    memset(fastbits, 0, ((size_t)1 + (size_t)(n >> shift)) * sizeof(u16));
    sa_uint_t * RESTRICT buckets =
        threads > 1 && n >= 262144
            ? (sa_uint_t *)libsais_alloc_aligned(
                  (size_t)threads * (ALPHABET_SIZE + (ALPHABET_SIZE * ALPHABET_SIZE)) * sizeof(sa_uint_t), 4096)
            : NULL;

    sa_sint_t index = bucket2 != NULL && fastbits != NULL && (buckets != NULL || threads == 1 || n < 262144)
                          ? libsais_unbwt_core(T, U, P, n, freq, r, I, bucket2, fastbits, buckets, threads)
                          : -2;

    libsais_free_aligned(buckets);
    libsais_free_aligned(fastbits);
    libsais_free_aligned(bucket2);

    return index;
}

static sa_sint_t libsais_unbwt_main_ctx(const LIBSAIS_UNBWT_CONTEXT * ctx, const u8 * T, u8 * U, sa_uint_t * P,
                                        sa_sint_t n, const sa_sint_t * freq, sa_sint_t r, const sa_uint_t * I) {
    return ctx != NULL && ctx->bucket2 != NULL && ctx->fastbits != NULL && (ctx->buckets != NULL || ctx->threads == 1)
               ? libsais_unbwt_core(T, U, P, n, freq, r, I, ctx->bucket2, ctx->fastbits, ctx->buckets,
                                    (sa_sint_t)ctx->threads)
               : -2;
}

static void * libsais_unbwt_create_ctx(void) { return (void *)libsais_unbwt_create_ctx_main(1); }

static void libsais_unbwt_free_ctx(void * ctx) { libsais_unbwt_free_ctx_main((LIBSAIS_UNBWT_CONTEXT *)ctx); }

static s32 libsais_unbwt_aux(const u8 * T, u8 * U, s32 * A, s32 n, const s32 * freq, s32 r, const s32 * I) {
    if ((T == NULL) || (U == NULL) || (A == NULL) || (n < 0) || ((r != n) && ((r < 2) || ((r & (r - 1)) != 0))) ||
        (I == NULL)) {
        return -1;
    } else if (n <= 1) {
        if (I[0] != n) {
            return -1;
        }
        if (n == 1) {
            U[0] = T[0];
        }
        return 0;
    }

    fast_sint_t t;
    for (t = 0; t <= (n - 1) / r; ++t) {
        if (I[t] <= 0 || I[t] > n) {
            return -1;
        }
    }

    return libsais_unbwt_main(T, U, (sa_uint_t *)A, n, freq, r, (const sa_uint_t *)I, 1);
}

static s32 libsais_unbwt_aux_ctx(const void * ctx, const u8 * T, u8 * U, s32 * A, s32 n, const s32 * freq, s32 r,
                                 const s32 * I) {
    if ((T == NULL) || (U == NULL) || (A == NULL) || (n < 0) || ((r != n) && ((r < 2) || ((r & (r - 1)) != 0))) ||
        (I == NULL)) {
        return -1;
    } else if (n <= 1) {
        if (I[0] != n) {
            return -1;
        }
        if (n == 1) {
            U[0] = T[0];
        }
        return 0;
    }

    fast_sint_t t;
    for (t = 0; t <= (n - 1) / r; ++t) {
        if (I[t] <= 0 || I[t] > n) {
            return -1;
        }
    }

    return libsais_unbwt_main_ctx((const LIBSAIS_UNBWT_CONTEXT *)ctx, T, U, (sa_uint_t *)A, n, freq, r,
                                  (const sa_uint_t *)I);
}

static s32 libsais_unbwt(const u8 * T, u8 * U, s32 * A, s32 n, const s32 * freq, s32 i) {
    return libsais_unbwt_aux(T, U, A, n, freq, n, &i);
}

static s32 libsais_unbwt_ctx(const void * ctx, const u8 * T, u8 * U, s32 * A, s32 n, const s32 * freq, s32 i) {
    return libsais_unbwt_aux_ctx(ctx, T, U, A, n, freq, n, &i);
}

static void libsais_compute_phi(const sa_sint_t * RESTRICT SA, sa_sint_t * RESTRICT PLCP, sa_sint_t n,
                                fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    sa_sint_t k = omp_block_start > 0 ? SA[omp_block_start - 1] : n;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 3; i < j; i += 4) {
        prefetchw(&PLCP[SA[i + prefetch_distance + 0]]);
        prefetchw(&PLCP[SA[i + prefetch_distance + 1]]);

        PLCP[SA[i + 0]] = k;
        k = SA[i + 0];
        PLCP[SA[i + 1]] = k;
        k = SA[i + 1];

        prefetchw(&PLCP[SA[i + prefetch_distance + 2]]);
        prefetchw(&PLCP[SA[i + prefetch_distance + 3]]);

        PLCP[SA[i + 2]] = k;
        k = SA[i + 2];
        PLCP[SA[i + 3]] = k;
        k = SA[i + 3];
    }

    for (j += prefetch_distance + 3; i < j; i += 1) {
        PLCP[SA[i]] = k;
        k = SA[i];
    }
}

static void libsais_compute_phi_omp(const sa_sint_t * RESTRICT SA, sa_sint_t * RESTRICT PLCP, sa_sint_t n,
                                    sa_sint_t threads) {
    {
        (void)(threads);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (n / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : n - omp_block_start;

        libsais_compute_phi(SA, PLCP, n, omp_block_start, omp_block_size);
    }
}

static void libsais_compute_plcp(const u8 * RESTRICT T, sa_sint_t * RESTRICT PLCP, fast_sint_t n,
                                 fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j, l = 0;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance; i < j; i += 1) {
        prefetch(&T[PLCP[i + prefetch_distance] + l]);

        fast_sint_t k = PLCP[i], m = n - (i > k ? i : k);
        while (l < m && T[i + l] == T[k + l]) {
            l++;
        }

        PLCP[i] = (sa_sint_t)l;
        l -= (l != 0);
    }

    for (j += prefetch_distance; i < j; i += 1) {
        fast_sint_t k = PLCP[i], m = n - (i > k ? i : k);
        while (l < m && T[i + l] == T[k + l]) {
            l++;
        }

        PLCP[i] = (sa_sint_t)l;
        l -= (l != 0);
    }
}

static void libsais_compute_plcp_omp(const u8 * RESTRICT T, sa_sint_t * RESTRICT PLCP, sa_sint_t n, sa_sint_t threads) {
    {
        (void)(threads);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (n / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : n - omp_block_start;

        libsais_compute_plcp(T, PLCP, n, omp_block_start, omp_block_size);
    }
}

static void libsais_compute_lcp(const sa_sint_t * RESTRICT PLCP, const sa_sint_t * RESTRICT SA,
                                sa_sint_t * RESTRICT LCP, fast_sint_t omp_block_start, fast_sint_t omp_block_size) {
    const fast_sint_t prefetch_distance = 32;

    fast_sint_t i, j;
    for (i = omp_block_start, j = omp_block_start + omp_block_size - prefetch_distance - 3; i < j; i += 4) {
        prefetch(&PLCP[SA[i + prefetch_distance + 0]]);
        prefetch(&PLCP[SA[i + prefetch_distance + 1]]);

        LCP[i + 0] = PLCP[SA[i + 0]];
        LCP[i + 1] = PLCP[SA[i + 1]];

        prefetch(&PLCP[SA[i + prefetch_distance + 2]]);
        prefetch(&PLCP[SA[i + prefetch_distance + 3]]);

        LCP[i + 2] = PLCP[SA[i + 2]];
        LCP[i + 3] = PLCP[SA[i + 3]];
    }

    for (j += prefetch_distance + 3; i < j; i += 1) {
        LCP[i] = PLCP[SA[i]];
    }
}

static void libsais_compute_lcp_omp(const sa_sint_t * RESTRICT PLCP, const sa_sint_t * RESTRICT SA,
                                    sa_sint_t * RESTRICT LCP, sa_sint_t n, sa_sint_t threads) {
    {
        (void)(threads);

        fast_sint_t omp_thread_num = 0;
        fast_sint_t omp_num_threads = 1;

        fast_sint_t omp_block_stride = (n / omp_num_threads) & (-16);
        fast_sint_t omp_block_start = omp_thread_num * omp_block_stride;
        fast_sint_t omp_block_size = omp_thread_num < omp_num_threads - 1 ? omp_block_stride : n - omp_block_start;

        libsais_compute_lcp(PLCP, SA, LCP, omp_block_start, omp_block_size);
    }
}

static s32 libsais_plcp(const u8 * T, const s32 * SA, s32 * PLCP, s32 n) {
    if ((T == NULL) || (SA == NULL) || (PLCP == NULL) || (n < 0)) {
        return -1;
    } else if (n <= 1) {
        if (n == 1) {
            PLCP[0] = 0;
        }
        return 0;
    }

    libsais_compute_phi_omp(SA, PLCP, n, 1);
    libsais_compute_plcp_omp(T, PLCP, n, 1);

    return 0;
}

static s32 libsais_lcp(const s32 * PLCP, const s32 * SA, s32 * LCP, s32 n) {
    if ((PLCP == NULL) || (SA == NULL) || (LCP == NULL) || (n < 0)) {
        return -1;
    } else if (n <= 1) {
        if (n == 1) {
            LCP[0] = PLCP[SA[0]];
        }
        return 0;
    }

    libsais_compute_lcp_omp(PLCP, SA, LCP, n, 1);

    return 0;
}

#endif
