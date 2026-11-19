// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test for --history: poison a value (cannot-index), load it, derive an
// address from it with arithmetic, then use it as a memory index -> fault.
// Under --history the dump prints the BFD-symbolicated trail of the
// offending value (the load and the math ops); without it, it does not.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t idx_src;
static volatile char buf[64];
static volatile char sink;

int main(void) {
    magic(1, (void*)&idx_src, sizeof(idx_src), 1u << 3); // cannot-index poison
    uint64_t x = idx_src;            // load: trail += load PC
    x = (x & 7) + 1;                 // math: trail += math PC(s)
    sink = *(volatile char*)((volatile char*)buf + x); // index use: FAULT
    write(1, "NOTREACHED\n", 11);
    return 0;
}
