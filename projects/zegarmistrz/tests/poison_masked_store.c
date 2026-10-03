// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: per-byte precise taint for EVEX masked stores.
// Mask 0x5555... (even bytes enabled, odd disabled). Tainted vector stored
// with mask: odd (disabled) dest bytes untouched (clean), even (enabled)
// dest bytes gain taint. Verified via Jcc on loaded bytes (tainted flags
// fault) and via value checks.
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

__attribute__((aligned(64))) static uint8_t srcb[64];
__attribute__((aligned(64))) static volatile uint8_t dst[64];

int main(void) {
    for (int i = 0; i < 64; i++)
        srcb[i] = (uint8_t)(0xC0 + i);
    memset((void*)dst, 0, sizeof(dst));
    const __mmask64 k = 0x5555555555555555ULL;
    // Poison src with cannot-branch, load tainted vector (load legal).
    magic(1, (void*)srcb, 64, 4u);
    __m512i t = _mm512_loadu_si512(srcb);
    magic(2, (void*)srcb, 64, 0u);
    // Masked store: even dest bytes gain taint, odd stay clean (zero).
    _mm512_mask_storeu_epi8((void*)dst, k, t);
    __asm__ volatile("" ::: "memory");
    // Disabled bytes untouched (still zero).
    for (int i = 1; i < 64; i += 2) {
        if (dst[i] != 0)
            return 10;
    }
    // Disabled dest byte (odd, clean) used in branch must NOT fault.
    volatile uint8_t clean_b = dst[3];
    __asm__ volatile("" ::: "memory");
    if (clean_b != 0)
        return 11; // dst[3] should be 0 (untouched)
    // Enabled dest byte (even, tainted) used in branch must fault
    // (tainted flags -> Jcc fault, core dump like poison_branch).
    volatile uint8_t tainted_b = dst[0];
    __asm__ volatile("" ::: "memory");
    // tainted_b is 0xC0 (non-zero), tainted cannot-branch. Compare + branch.
    if (tainted_b == 0xC0) {
        // Taken (tainted condition): faults before reaching here? Actually
        // fault occurs on Jcc with tainted flags. If emulator is precise,
        // this branch faults (core). If imprecise (disabled taint leaked or
        // enabled missing), behavior differs (return code distinguishes).
        write(1, "NOTREACHED\n", 11);
        return 0;
    }
    return 12;
}
