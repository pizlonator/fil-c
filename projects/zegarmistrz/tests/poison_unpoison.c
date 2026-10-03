// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: poison then unpoison -> access succeeds, exit 0.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile char buf[64];

int main(void) {
    buf[0] = 'q';
    magic(1, (void*)buf, sizeof(buf), 1u | 2u | 4u | 8u);
    magic(2, (void*)buf, sizeof(buf), 0u); // unpoison
    if (buf[0] != 'q')
        return 1;
    buf[1] = 'w';
    if (buf[1] != 'w')
        return 2;
    if (magic(0, 0, 0, 0) != 0)
        return 3; // query no-op returns 0
    write(1, "unpoison-ok\n", 12);
    return 0;
}
