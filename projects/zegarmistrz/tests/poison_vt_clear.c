// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: with clear-var-time-op-on-store, storing sanitizes -- poison
// cannot-var-time-op + clear-var-time-op-on-store, store clean data through
// it (the store must clear the poison), load it back, then divide by what
// was loaded -> must succeed (the DIV operand is clean). Exit 0.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t clean_data = 0x1122334455667788ULL;

int main(void) {
    static uint64_t volatile slot = 0;
    // Poison CANNOT_VAR_TIME_OP + CLEAR_VAR_TIME_OP_ON_STORE.
    magic(1, (void*)&slot, sizeof(slot), 64u | 128u);
    // Store clean data: must succeed AND sanitize (clear the poison).
    slot = clean_data;
    // Load it back: must be clean now (and cannot be 0).
    uint64_t d = slot;
    // Divide by it: must succeed.
    if (d != clean_data)
        return 1;
    volatile uint64_t q = clean_data / d;
    (void)q;
    write(1, "vt-clear-ok\n", 12);
    return 0;
}