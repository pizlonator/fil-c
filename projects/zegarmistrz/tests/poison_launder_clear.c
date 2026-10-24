// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: with clear-branch-on-store, storing sanitizes -- poison
// cannot-branch + clear-branch-on-store, store clean data through it (the
// store must clear the poison), load it back, then branch on what was
// loaded -> must succeed. Exit 0.

#include <stdint.h>

#include <stdio.h>

#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static uint64_t clean_data = 0x1122334455667788ULL;

int main(void) {
    static uint64_t volatile slot = 0;
    // Poison CANNOT_BRANCH + CLEAR_BRANCH_ON_STORE.
    magic(1, (void*)&slot, sizeof(slot), 4u | 16u);
    // Store clean data: must succeed AND sanitize (clear the poison).
    slot = clean_data;
    // Load it back: must be clean now.
    uint64_t v = slot;
    // Branch on it: must succeed.
    if (v != clean_data)
        return 1;
    write(1, "launder-clear-ok\n", 17);
    return 0;
}
