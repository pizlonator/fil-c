// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: doc 6+7 exemptions -- poison a slot with CANNOT_VAR_TIME_OP, load
// the tainted value, then run a chain of provably constant-time
// instructions while the taint is live in a register: PAUSE, LFENCE,
// MFENCE, SFENCE, RDTSC, RDTSCP, RDRAND, RDSEED, RDPID, an ADD and a MOV.
// Even in --var-time-op=everything mode none of these may trap. Exit 0.
#include <stdint.h>
#include <stdio.h>
#include <immintrin.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t clean_data = 0x1122334455667788ULL;
static volatile uint64_t sink;

int main(void) {
    static uint64_t volatile slot = 0;
    // Poison CANNOT_VAR_TIME_OP only.
    magic(1, (void*)&slot, sizeof(slot), 64u);
    // Load the tainted value and materialize it in a GPR (kept live).
    uint64_t t = slot;
    __asm__ volatile("" : "+r"(t));
    // Exempt (guaranteed constant-time) sequence, t live throughout.
    _mm_pause();
    _mm_lfence();
    _mm_mfence();
    _mm_sfence();
    unsigned lo, hi, aux, aux2;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi) : "r"((unsigned)t));
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux2) : : );
    unsigned long long r = 0, s = 0;
    _rdrand64_step(&r);
    _rdseed64_step(&s); // may fail; ignore
    unsigned long pid;
    __asm__ volatile("rdpid %q0" : "=r"(pid));
    uint64_t y = t + 1; // ADD: doc 7.1 guaranteed
    uint64_t t2 = t;    // MOV: guaranteed
    sink = y + t2 + lo + hi + (uint64_t)aux + aux2 + (uint64_t)pid + r + s;
    write(1, "exempt-ok\n", 10);
    return 0;
}