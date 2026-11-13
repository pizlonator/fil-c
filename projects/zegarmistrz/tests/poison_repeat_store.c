// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Repeated secret spills must retain taint. Clean overwrites must clear
// eligible dimensions even when the new value has a different taint.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static char target = 'T';
static volatile uintptr_t secret, other, slot;

int main(int argc, char** argv) {
    if (argc != 4)
        return 2;
    unsigned dim = (unsigned)atoi(argv[1]);
    unsigned count = (unsigned)atoi(argv[2]);
    int clean = !strcmp(argv[3], "clean");
    int mixed = !strcmp(argv[3], "mixed");
    const unsigned bits[] = {4u, 8u, 64u};
    if (dim >= 3 || count < 1 || count > 3)
        return 2;

    secret = other = (uintptr_t)&target;
    magic(1, (void*)&secret, sizeof(secret), bits[dim]);
    magic(1, (void*)&other, sizeof(other), bits[(dim + 1) % 3]);
    for (unsigned i = 0; i < count; ++i)
        slot = secret;
    if (!strcmp(argv[3], "reuse")) {
        slot = (uintptr_t)&target;
        slot = secret;
    }
    if (clean)
        slot = (uintptr_t)&target;
    if (mixed)
        slot = other;

    // Probe only the original dimension. A different dimension on the
    // replacement value must not prevent this use of the overwritten slot.
    uintptr_t value = slot;
    switch (dim) {
    case 0:
        __asm__ volatile("testq %0, %0; jnz 1f; 1:" : : "r"(value) : "cc");
        break;
    case 1:
        __asm__ volatile("movzbq (%0), %%rax" : : "r"(value) : "rax", "memory");
        break;
    case 2:
        __asm__ volatile("xorl %%edx, %%edx; movl $100, %%eax; divq %0"
                         : : "r"(value) : "rax", "rdx", "cc");
        break;
    }
    if (clean || mixed)
        write(1, "store-ok\n", 9);
    else
        write(1, "NOTREACHED\n", 11);
    return 0;
}
