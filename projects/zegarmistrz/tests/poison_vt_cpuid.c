// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: CPUID with a cannot-var-time-op-tainted leaf -- the leaf/subleaf
// select the returned feature set, so CPUID's execution time depends on the
// operand values (doc 4.9). Poison a slot, load the tainted value into EAX,
// then CPUID -> guest fault with the var-time reason + core dump (a CPUID
// with clean EAX/ECX must keep working; the trap is on the tainted leaf
// only).
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <cpuid.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t clean_data = 0x4000000dULL;

int main(void) {
    static uint64_t volatile slot = 0;
    // Poison CANNOT_VAR_TIME_OP only.
    magic(1, (void*)&slot, sizeof(slot), 64u);
    // Load the tainted leaf.
    uint64_t v = slot;
    unsigned leaf = (unsigned)v;
    // CPUID with a tainted leaf: doc 4.9 -> guest error in every mode.
    unsigned a, b, c, d;
    __cpuid_count(leaf, 0, a, b, c, d);
    volatile unsigned sink = a + b + c + d;
    (void)sink;
    write(1, "NOTREACHED\n", 11);
    return 0;
}