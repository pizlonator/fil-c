// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: PEXTRD extracting a tainted lane from an xmm register into a GPR must
// carry the vector sidecar into the GPR (the old code set out.v but left
// out.taint clean, laundering vector poison into a clean GPR).  Poison a
// cannot-index value, load it into an xmm via MOVQ from the poisoned memory,
// PEXTRD the lane into a GPR, then use the GPR as a memory index -> fault.
// Expect exit 139 with cannot-index.
#include <emmintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t idx_src;
static volatile char buf[64];
static volatile char sink;

int main(void) {
    idx_src = 3;
    magic(1, (void*)&idx_src, sizeof(idx_src), 1u << 3); // cannot-index poison
    __m128i v;
    __asm__ __volatile__("movq %1, %0" : "=x"(v) : "m"(idx_src)); // poisoned bytes -> xmm
    unsigned idx;
    // The fixed path: PEXTRD r32/r64, xmm, imm8 must move the sidecar.
    __asm__ __volatile__("pextrd $0, %1, %k0" : "=r"(idx) : "x"(v));
    sink = *(volatile char*)((volatile char*)buf + idx); // index use: FAULT
    write(1, "NOTREACHED\n", 11);
    return 0;
}
