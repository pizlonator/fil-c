// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Regression guest for the zegarmistrz BZHI fix and the glibc-heap-destroying
// memset pattern it broke:
//
//   glibc's __memset_avx512_unaligned_erms finishes every memset whose length
//   is a multiple of 64 with
//       mov  $-1, %rcx
//       bzhi %rdx, %rcx, %rcx      ; rdx == len & 63 == 0 here
//       kmovq %rcx, %k1
//       vmovdqu8 %zmm16, (%rax) {%k1}
//   i.e. BZHI with index 0 must produce mask 0 so the masked store writes
//   nothing.  The old emulator computed the 64-bit mask as ~0ULL >> (64 - k),
//   which for k == 0 is a shift by 64 (undefined in C, executed as a shift by
//   0 on x86), leaving the mask all-ones: the store then wrote 64 zero bytes
//   past the end of the buffer, zeroing glibc's top-chunk header whenever the
//   buffer sat at the top of the heap, and the next sysmalloc died with
//   "malloc.c:2599 (sysmalloc): assertion failed".  That aborted every yolo
//   (system-glibc) OpenSSL test under the emulator.
//
// Part 1 checks BZHI semantics directly (index 0, small, >= 64, and the
// SRC2[7:0] masking rule), gated on runtime CPUID so it runs everywhere.
// Part 2 is the triggering workload: malloc/calloc/realloc/free churn where
// every fill is a memset (or calloc) of a 64-multiple length at the top of
// the heap, interleaved with an mmap-backed large allocation, plus
// heap-shrink/grow cycles.  Prints "malloc-churn-ok" and exits 0 on success
// (same output native and emulated); on the old emulator part 1 FAILs and
// part 2 aborts inside glibc.
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, name)                          \
    do {                                           \
        if (!(cond)) {                             \
            printf("FAIL %s\n", (name));           \
            failures++;                            \
        }                                          \
    } while (0)

static unsigned cpuid7_ebx(void) {
    unsigned a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(7), "c"(0));
    return b;
}

#define CPUID7_BMI2 (1u << 8)

static void part1_bzhi(void) {
    if (!(cpuid7_ebx() & CPUID7_BMI2)) {
        printf("skip bzhi (no BMI2)\n");
        return;
    }
    /* Runtime values so the compiler cannot constant-fold the intrinsics. */
    static volatile const uint64_t indices64[] = {0, 1, 63, 100, 256};
    static volatile const uint32_t indices32[] = {0, 1, 31};
    volatile uint64_t src = ~0ULL;
    CHECK(_bzhi_u64(src, indices64[0]) == 0, "bzhi64 idx=0 zeroes all bits");
    CHECK(_bzhi_u64(src, indices64[1]) == 1, "bzhi64 idx=1 keeps bit 0");
    CHECK(_bzhi_u64(src, indices64[2]) == 0x7fffffffffffffffULL,
          "bzhi64 idx=63 keeps bits 0..62");
    CHECK(_bzhi_u64(src, indices64[3]) == ~0ULL, "bzhi64 idx>=64 unchanged");
    /* Intel uses only SRC2[7:0]: index 256 behaves like index 0. */
    CHECK(_bzhi_u64(src, indices64[4]) == 0, "bzhi64 idx=256 -> idx&255=0");
    volatile uint32_t src32 = 0xffffffffu;
    CHECK(_bzhi_u32(src32, indices32[0]) == 0, "bzhi32 idx=0 zeroes all bits");
    CHECK(_bzhi_u32(src32, indices32[1]) == 1, "bzhi32 idx=1 keeps bit 0");
    CHECK(_bzhi_u32(src32, indices32[2]) == 0x7fffffffu, "bzhi32 idx=31");
}

/* Fill with a 64-multiple memset (the pattern that hits BZHI index 0 inside
 * glibc's evex memset), then verify the fill survived. */
static int fill_verify(unsigned char* p, size_t n, unsigned char seed) {
    memset(p, seed, n);
    for (size_t i = 0; i < n; i++) {
        if (p[i] != seed) {
            printf("FAIL churn data %zu (slot value %d != %d)\n", i, p[i], seed);
            return 0;
        }
    }
    return 1;
}

static void part2_churn(void) {
    static const size_t sizes[] = {
        64, 192, 320, 576, 1088, 2048, 4096, 6144, 8256, 12288, 16384, 24576,
        32768, 40960, 49216, 65536,
    };
    static const size_t nsz = sizeof(sizes) / sizeof(sizes[0]);
    unsigned char* keep[nsz];
    size_t ksz[nsz];
    memset(keep, 0, sizeof(keep));
    memset(ksz, 0, sizeof(ksz));
    unsigned char seed = 0x41;
    for (int iter = 0; iter < 240; iter++) {
        size_t i = (size_t)(iter * 5) % nsz;
        if (keep[i]) {
            if (!fill_verify(keep[i], ksz[i], seed)) {
                return; /* CHECK already reported */
            }
            free(keep[i]);
            keep[i] = NULL;
            ksz[i] = 0;
        }
        size_t n = sizes[(i + 1) % nsz];
        unsigned char* p;
        if (iter % 7 == 3) {
            /* calloc: zero-fill of a 64-multiple length, same BZHI path. */
            p = calloc(1, n);
        } else if (iter % 7 == 5) {
            p = realloc(keep[(i + 2) % nsz], n);
            keep[(i + 2) % nsz] = NULL;
            ksz[(i + 2) % nsz] = 0;
        } else {
            p = malloc(n);
        }
        if (!p) {
            printf("FAIL churn alloc %zu\n", n);
            failures++;
            return;
        }
        memset(p, seed, n);
        keep[i] = p;
        ksz[i] = n;
        seed = (unsigned char)(seed * 31 + 7);
        if (iter % 23 == 0) {
            /* mmap-backed allocation interleaved with the brk churn, and a
             * heap grow/shrink cycle around it. */
            unsigned char* big = calloc(1, 1u << 20);
            if (!big) {
                printf("FAIL churn big alloc\n");
                failures++;
                return;
            }
            big[0] = 0x5a;
            big[(1u << 20) - 64] = 0xa5;
            if (big[0] != 0x5a || big[(1u << 20) - 64] != 0xa5) {
                printf("FAIL churn big data\n");
                failures++;
                free(big);
                return;
            }
            free(big);
        }
    }
    for (size_t i = 0; i < nsz; i++)
        free(keep[i]);
    /* Final headroom check: extend the heap, then fire zero-length memsets at
     * every 8-byte offset around the end of the usable area.  glibc's evex
     * memset routes len < 64 through
     *     mov $-1, %rcx; bzhi %rdx, %rcx, %rcx; kmovq %rcx, %k1;
     *     vmovdqu8 %zmm16, (%rax) {%k1}      ; rax = dst + len - 1
     * with rdx == len, so a ZERO-length memset still executes the masked
     * store with mask (1 << 0) - 1 = 0 (no bytes written natively).  On the
     * pre-fix emulator the mask was all-ones and the store wrote 64 zero
     * bytes starting one byte below dst; with dst just below the heap top
     * chunk that zeroes the top-chunk header.  The large allocation that
     * follows (without freeing anything in between) forces sysmalloc to
     * extend the heap from the top chunk, which on the pre-fix emulator died
     * with glibc's "malloc.c:2599 (sysmalloc): assertion failed". */
    unsigned char* p = malloc(1u << 20);
    if (!p) {
        printf("FAIL churn final alloc\n");
        failures++;
        return;
    }
    size_t usable = malloc_usable_size(p);
    static volatile const size_t zero = 0; /* keep gcc from eliding the call */
    for (size_t off = usable - 64; off <= usable + 56; off += 8)
        memset(p + off, 0x33, zero);
    memset(p, 0x33, 128);
    for (size_t i = 0; i < 128; i++) {
        if (p[i] != 0x33) {
            printf("FAIL churn top data %zu\n", i);
            failures++;
            free(p);
            return;
        }
    }
    unsigned char* q = malloc(2u << 20);
    if (!q) {
        printf("FAIL churn sysmalloc-force alloc\n");
        failures++;
        free(p);
        return;
    }
    memset(q, 0x44, 128);
    for (size_t i = 0; i < 128; i++) {
        if (q[i] != 0x44) {
            printf("FAIL churn sysmalloc data %zu\n", i);
            failures++;
            free(q);
            free(p);
            return;
        }
    }
    free(q);
    free(p);
}

int main(void) {
    part1_bzhi();
    part2_churn();
    if (failures == 0)
        printf("malloc-churn-ok\n");
    else
        printf("malloc-churn: %d failures\n", failures);
    return failures ? 1 : 0;
}
