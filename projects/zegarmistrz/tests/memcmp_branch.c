// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: memcmp between a poisoned array and a clean twin — the branch test
// djb suggested. Two 256-byte arrays with identical content; the first 127
// bytes of one are poisoned CANNOT_BRANCH (the classic shape: a secret
// prefix inside a bigger buffer fed to memcmp). memcmp dispatches through
// the glibc IFUNC to the AVX2 implementation, whose vpcmpeqb / vpmovmskb /
// branch chain must carry the taint: the emulator must fault with the
// cannot-branch reason (exit 139) before the program reaches abort().
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

#define N 256

static unsigned char x[N];
static unsigned char y[N];

__attribute__((noinline))
static void fill(void) {
    for (int i = 0; i < N; ++i)
        x[i] = y[i] = (unsigned char)(i * 7 + 3);
}

int main(void) {
    fill();
    // Poison the first 127 bytes of x with CANNOT_BRANCH (flag 4).
    magic(1, x, 127, 4u);
    // The contents are identical, so memcmp returns 0 and abort() would not
    // fire: every fault must come from taint tracking inside the memcmp
    // implementation. The branch on the memcmp result (both the internal
    // jnz chains and the if below) depends on poisoned bytes.
    if (memcmp(x, y, N))
        abort();
    if (write(1, "missed-memcmp-branch\n", 21) < 0)
        return 2;
    return 0;
}
