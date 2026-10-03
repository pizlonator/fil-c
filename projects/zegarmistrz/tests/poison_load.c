// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: poison cannot-load, then load -> guest error + core dump.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile char buf[64];

int main(void) {
    buf[0] = 'x';
    magic(1, (void*)buf, sizeof(buf), 1u); // op=poison, cannot-load
    // This load must fault.
    char c = buf[0];
    (void)c;
    write(1, "NOTREACHED\n", 11);
    return 0;
}
