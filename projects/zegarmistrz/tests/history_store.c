// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test for --history store recording: poison a value (cannot-index), load it
// through an xmm register (MOVQ), store it back to memory from the xmm
// (MOVQ -- the store note lands durably on the live xmm sidecar), copy the
// value back to a GPR (CVTTSD2SI -- the legacy MOVD/MOVQ GPR path drops the
// sidecar), then use it as a memory index -> fault. The dumped poison
// history must contain the load, the store, and the copy PCs.
//
// The three data ops are written as inline asm because at -O2 gcc dissolves
// _mm_loadl_epi64/_mm_storel_epi64 into plain GPR moves (no MOVQ xmm store
// to exercise) and the legacy MOVD/MOVQ GPR path drops the sidecar; the asm
// pins the exact forms this test needs, each on its own source line so the
// dumped trail symbolicates to the load, the store, and the copy.
#include <emmintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile double idx_src; // holds the double 3.0 (so CVTTSD2SI gives 3)
static volatile uint64_t mirror;
static volatile char buf[64];
static volatile char sink;

int main(void) {
    idx_src = 3.0;
    magic(1, (void*)&idx_src, sizeof(idx_src), 1u << 3); // cannot-index poison
    __m128i v;
    __asm__ __volatile__("movq %1, %0" : "=x"(v) : "m"(idx_src)); // load: trail += load PC
    __asm__ __volatile__("movq %0, %1" : : "x"(v), "m"(mirror)); // store: trail += store PC (durable, live xmm sidecar)
    // xmm -> GPR: trail follows (CVTTSD2SI propagates the sidecar).
    uint64_t x;
    __asm__ __volatile__("cvttsd2si %1, %0" : "=r"(x) : "x"(_mm_castsi128_pd(v)));
    sink = *(volatile char*)((volatile char*)buf + x); // index use: FAULT
    write(1, "NOTREACHED\n", 11);
    return 0;
}
