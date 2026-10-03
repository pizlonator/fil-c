// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: tainted value passed as syscall arg -> guest error + core dump.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

int main(void) {
    static volatile uint64_t len = 0;
    len = 11;
    magic(1, (void*)&len, sizeof(len), 8u); // cannot-index
    uint64_t n = len; // volatile load, tainted
    // write(1, msg, tainted_len) must fault on the tainted RDX.
    write(1, "NOTREACHED\n", n);
    return 0;
}
