// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: poison cannot-branch, load function pointer, call it -> guest error.
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static void ok(void) { write(1, "called\n", 7); }

int main(void) {
    static void (*volatile fp)(void) = ok;
    magic(1, (void*)&fp, sizeof(fp), 4u); // op=poison, cannot-branch
    void (*g)(void);
    // Load the tainted pointer (load itself is legal).
    g = fp; // volatile load
    // Indirect call through it must fault.
    g();
    write(1, "NOTREACHED\n", 11);
    return 0;
}
