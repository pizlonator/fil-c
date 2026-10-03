// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: per-byte precise taint for EVEX masked loads (VMOVDQU8 with {k}).
// 512-bit (64-byte) masked load where mask includes every other byte
// starting with byte 0 (mask 0x5555...: even bytes enabled, odd disabled).
// Byte 3 (odd, disabled) poisoned with cannot-load must NOT fault and must
// NOT taint the result. Then byte 0 (even, enabled) poisoned with
// cannot-load must fault (guest error + core dump, like poison_load).
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

__attribute__((aligned(64))) static volatile uint8_t buf[64];
__attribute__((aligned(64))) static volatile uint8_t out[64];

int main(void) {
    for (int i = 0; i < 64; i++)
        buf[i] = (uint8_t)(i + 1);
    // Mask: even bytes enabled (0,2,4...), odd disabled. Bit i = byte i.
    const __mmask64 k = 0x5555555555555555ULL;
    // Poison byte 3 (disabled, odd) with cannot-load (bit0) + cannot-branch
    // (bit2). Masked load must NOT fault (disabled bytes untouched) and
    // must NOT taint.
    magic(1, (void*)&buf[3], 1, 1u | 4u);
    __m512i v = _mm512_maskz_loadu_epi8(k, (void*)buf);
    _mm512_storeu_si512((void*)out, v);
    __asm__ volatile("" ::: "memory");
    // Verify enabled bytes loaded correctly (disabled are zero from maskz).
    for (int i = 0; i < 64; i++) {
        uint8_t expect = (i & 1) ? 0 : (uint8_t)(i + 1);
        if (out[i] != expect)
            return 10;
    }
    // Verify no taint from disabled poisoned byte: use loaded out[0]
    // (enabled, clean, value 1) in branch (should NOT fault).
    volatile uint8_t c0 = out[0];
    __asm__ volatile("" ::: "memory");
    if (c0 != 1)
        return 11;
    // No fault so far: disabled poison ignored. Now poison byte 0 (enabled)
    // with cannot-load: masked load must fault.
    magic(1, (void*)&buf[0], 1, 1u);
    __m512i w = _mm512_maskz_loadu_epi8(k, (void*)buf);
    _mm512_storeu_si512(out, w);
    (void)w;
    write(1, "NOTREACHED\n", 11);
    return 0;
}
