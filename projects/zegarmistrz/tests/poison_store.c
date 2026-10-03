// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: poison cannot-store, then store -> guest error + core dump.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile char buf[64];

int main(void) {
    magic(1, (void*)buf, sizeof(buf), 2u); // op=poison, cannot-store
    // This store must fault.
    buf[0] = 'y';
    write(1, "NOTREACHED\n", 11);
    return 0;
}
