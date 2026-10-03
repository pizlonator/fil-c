#include <stdio.h>

long mbteardown_test(long a, long b);

// Sweep the entry rsp across a full 128-byte alignment period by C
// recursion: the `and $-128` slack (0..127) varies per residue, and any
// static-displacement miscompilation — of the post-and slots, of the carrier
// restores, or of the rsp recovery — fails deterministically. Six keepers
// stay live across the call (forcing the caller's callee-saved registers to
// hold them), so wrong register restores corrupt the checksum; the wallseg
// canary plus the return address itself catch a wrong rsp recovery. The
// variable-size wallseg spreads the 128 levels across every mod-128 entry
// residue.
static int ok = 1;
static void run(int k) {
    volatile char wallseg[256 + (k % 64)];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 128) run(k + 1);
    long a = 40 + k, b = 2 + k;
    long k1 = 0x123456789abcdef0L ^ k, k2 = 0x0fedcba987654321L ^ (k * 3);
    long k3 = 0x1111111111111111L + k, k4 = 0x2222222222222222L - k;
    long k5 = 0x3333333333333333L ^ (k * 7), k6 = 0x4444444444444444L + (k * 11);
    long r = mbteardown_test(a, b);
    long chk = k1 ^ k2 ^ k3 ^ k4 ^ k5 ^ k6;
    long want = (0x123456789abcdef0L ^ k) ^ (0x0fedcba987654321L ^ (k * 3))
        ^ (0x1111111111111111L + k) ^ (0x2222222222222222L - k)
        ^ (0x3333333333333333L ^ (k * 7)) ^ (0x4444444444444444L + (k * 11));
    ok &= (r == a + b);
    ok &= (chk == want);
    for (int i = 0; i < 256; ++i) ok &= (wallseg[i] == (char)(0xA5 + i));
    __asm__ volatile("" ::: "memory");
}
int main() {
    run(0);
    printf("mbteardown %s\n", ok ? "ok" : "BAD");
    return 0;
}
