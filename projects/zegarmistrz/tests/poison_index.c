// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: poison cannot-index, load address, dereference -> guest error.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static char target = 'T';

int main(void) {
    static char* volatile ptr = &target;
    magic(1, (void*)&ptr, sizeof(ptr), 8u); // op=poison, cannot-index
    char* q;
    q = ptr; // volatile load through poisoned memory
    // Dereference through tainted pointer must fault.
    volatile char c = *q;
    (void)c;
    write(1, "NOTREACHED\n", 11);
    return 0;
}
