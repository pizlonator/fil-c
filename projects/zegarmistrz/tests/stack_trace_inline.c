// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: inlined calls must expand in the crash-time guest stack trace.
// inner_boom is always-inlined into outer_boom, so the faulting rip lands
// in outer_boom's body while the DWARF inline chain says inner_boom ->
// outer_boom; the trace must show both.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static inline __attribute__((always_inline)) void inner_boom(
    volatile char* buf) {
    magic(1, (void*)buf, 64, 1u); // poison cannot-load
    char c = buf[0];              // must fault (physically inside outer_boom)
    (void)c;
}

static __attribute__((noinline)) void outer_boom(volatile char* buf) {
    inner_boom(buf);
}

static volatile char buf[64];

int main(void) {
    outer_boom((volatile char*)buf);
    write(1, "NOTREACHED\n", 11);
    return 0;
}
