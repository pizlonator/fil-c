// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: CTRL dependencies keep their own dimension -- poison two buffers
// with cannot-branch only (no clear bit), store clean bytes through them,
// then REPE CMPSB across both: the loop stops at the first mismatch, so the
// iteration count would be data-dependent (doc 4.4 CTRL). The per-iteration
// cannot-branch check must fault -- the reason is cannot-branch, NOT
// var-time.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile char a[16];
static volatile char b[16];

int main(void) {
    // Poison both buffers CANNOT_BRANCH only: no clear-branch-on-store.
    magic(1, (void*)a, sizeof(a), 4u);
    magic(1, (void*)b, sizeof(b), 4u);
    // Store clean bytes through both: must not fault and must not clear the
    // poison (equal-or-distinct does not matter for the trap).
    for (int i = 0; i < 16; i++) {
        a[i] = (char)('a' + i);
        b[i] = (char)('a' + i);
    }
    // REPE CMPSB: the compared bytes are cannot-branch-tainted, so the
    // per-iteration check must fault. RSI/RDI/RCX are updated by the
    // instruction, so they are in+out operands (not clobbers).
    const char* pa = (const char*)a;
    const char* pb = (const char*)b;
    unsigned long n = 16;
    __asm__ volatile("repz cmpsb" : "+S"(pa), "+D"(pb), "+c"(n)
                     : : "cc", "memory");
    write(1, "NOTREACHED\n", 11);
    return 0;
}