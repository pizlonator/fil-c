// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: mode-3 (OTHER) classification -- poison a slot with
// CANNOT_VAR_TIME_OP, load the tainted value, move it into an XMM (MOVQ
// propagates the taint), then VPERMILPS with the tainted control vector.
// VPERMILPS is deliberately unclassified: it traps only under
// --var-time-op=everything, and must run cleanly (exit 0) in the default
// and plausible modes.
#include <stdint.h>
#include <stdio.h>
#include <immintrin.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t clean_data = 0x0102030405060708ULL;
static volatile float sink[4];

int main(void) {
    static uint64_t volatile slot = 0;
    // Poison CANNOT_VAR_TIME_OP only.
    magic(1, (void*)&slot, sizeof(slot), 64u);
    // Load the tainted value and move it into an XMM (MOVQ keeps the taint).
    uint64_t v = slot;
    __m128i ctrl = _mm_cvtsi64_si128(v);
    // VPERMILPS with a tainted control operand: unclassified => OTHER.
    __m128 r = _mm_permutevar_ps(_mm_set1_ps(1.0f), ctrl);
    _mm_storeu_ps((float*)sink, r); // sink the result
    write(1, "mode3-ok\n", 9);
    return 0;
}