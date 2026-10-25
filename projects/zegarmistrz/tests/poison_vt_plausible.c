// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: cannot-var-time-op mode gating for the doc-5 suspects -- poison a
// slot with CANNOT_VAR_TIME_OP, load the tainted value, mask it down to a
// rotate count, then RCR by CL (microcoded shift, doc 5). In the default
// "known" mode this must NOT trap (exit 0); under
// --var-time-op=plausible it must fault with the var-time reason.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t clean_data = 0x0123456789abcdefULL;

int main(void) {
    static uint64_t volatile slot = 0;
    // Poison CANNOT_VAR_TIME_OP only.
    magic(1, (void*)&slot, sizeof(slot), 64u);
    // Load the tainted value and derive the rotate count from it.
    uint64_t v = slot;
    unsigned c = (unsigned)(v & 63); // tainted CL
    // RCR by CL: a doc-5 suspect (mode-gated, not a mode-1 trap).
    uint64_t x = 0x0123456789abcdefULL;
    __asm__ volatile("rcrq %b1, %0" : "+r"(x) : "c"(c));
    volatile uint64_t sink = x;
    (void)sink;
    write(1, "plausible-ok\n", 13);
    return 0;
}