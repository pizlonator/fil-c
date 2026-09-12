#include <stdio.h>

long sum9align64(long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9);

// Sweep the entry rsp across a full alignment period by C recursion: each of
// 128 levels calls the test and verifies the 1+2+...+9 = 45 sum. The five
// incoming stack words must read correctly on EVERY mod-64 entry residue; a
// static-displacement entry load is correct on at most one residue class and
// the check fails deterministically on the rest. The zmm round-trip
// contributes 0 (a live use keeping the aligned cluster from being dead-code
// eliminated; a misaligned frame would #GP on the vmovdqa64 itself).
static int ok = 1;
static void run(int k) {
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 128) run(k + 1);
    long r = sum9align64(1, 2, 3, 4, 5, 6, 7, 8, 9);
    __asm__ volatile("" ::: "memory");
    ok &= (r == 45);
    for (int i = 16; i < 256; ++i) ok &= (wallseg[i] == (char)(0xA5 + i));
}
int main() {
    run(0);
    printf("and64stackargs-int %s\n", ok ? "ok" : "BAD");
    return 0;
}
