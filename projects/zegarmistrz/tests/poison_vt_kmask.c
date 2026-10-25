// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: EVEX opmask on a masked memory load (doc 4.7) -- poison a slot with
// CANNOT_VAR_TIME_OP, load the tainted value into a GPR, KMOVW it into an
// opmask register (the taint propagates into the k-register sidecar), then
// use it as the write mask of a {k}-masked load: the mask decides which
// lanes/pages are touched, so the load's timing is data-dependent -> guest
// fault with the var-time reason in every mode.
#include <stdint.h>
#include <stdio.h>
#include <immintrin.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t clean_data = 0x0102030405060708ULL;
static float valid[16];
static volatile float sink[16];

int main(void) {
    static uint64_t volatile slot = 0;
    for (int i = 0; i < 16; i++)
        valid[i] = (float)i;
    // Poison CANNOT_VAR_TIME_OP only.
    magic(1, (void*)&slot, sizeof(slot), 64u);
    // Load the tainted value into a GPR, then KMOVW it into k1.
    uint64_t v = slot;
    __mmask16 k = _cvtu32_mask16((unsigned)v);
    // EVEX masked load with the tainted mask: doc 4.7 -> guest error.
    __m512 d = _mm512_mask_loadu_ps(_mm512_set1_ps(0.f), k,
                                    (const float*)valid);
    _mm512_storeu_ps((float*)sink, d); // sink the result
    write(1, "NOTREACHED\n", 11);
    return 0;
}