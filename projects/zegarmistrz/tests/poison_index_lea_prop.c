// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: LEA must not LAUNDER taint either. LEA itself accepts tainted
// base/index registers (it never touches memory), but the taint of the
// address registers still propagates into the result -- the computed
// address is derived from them. So dereferencing the LEA result must fault
// with the cannot-index reason (argv subtest "base": tainted base;
// "index": clean base with a tainted index). A regressed emulator either
// drops the taint (exit 0) or faults for the wrong reason.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static char buf[64];
static volatile uint64_t slot = 8;

int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "base";
    magic(1, (void*)&slot, sizeof(slot), 8u); // op=poison, cannot-index
    uint64_t p = slot; // tainted 8
    uint64_t q;
    if (strcmp(which, "base") == 0) {
        // LEA from the tainted base: must NOT fault.
        asm volatile("lea 0x40(%1), %0" : "=r"(q) : "r"(p));
    } else {
        // LEA from a clean base + the tainted index: must NOT fault.
        asm volatile("lea (%1,%2,4), %0" : "=r"(q) : "r"((uint64_t)buf), "r"(p));
    }
    // The result is derived from a tainted register -> still tainted:
    // dereferencing it must fault with the cannot-index reason.
    volatile uint32_t v = *(volatile uint32_t*)q;
    (void)v;
    write(1, "NOTREACHED\n", 11);
    return 0;
}
