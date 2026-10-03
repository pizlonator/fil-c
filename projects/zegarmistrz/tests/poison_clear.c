// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: poison cannot-index with clear-on-store; storing sanitizes, so a
// later load + dereference succeeds. Exit 0.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static char target = 'T';

int main(void) {
    static char* volatile ptr = 0;
    // Poison cannot-index (bit3) + clear-index-on-store (bit5).
    magic(1, (void*)&ptr, sizeof(ptr), 8u | 32u);
    // Store a clean pointer: must succeed and sanitize.
    char* clean = &target;
    ptr = clean; // volatile store
    // Load + dereference must now succeed.
    char* q = ptr; // volatile load
    if (*q != 'T')
        return 1;
    write(1, "clear-ok\n", 9);
    return 0;
}
