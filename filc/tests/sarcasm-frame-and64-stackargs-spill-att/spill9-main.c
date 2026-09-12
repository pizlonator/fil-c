#include <stdio.h>

long sum9_spill(long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8, long a9);

long helper9(long p0, long p1, long p2, long p3, long p4, long p5, long p6, long p7, long p8) {
    return p0 + p1 + p2 + p3 + p4 + p5 + p6 + p7 + p8;
}

// In-test saveOff assert (see spill9.s header for the .yolo.s grep recipe,
// which run-tests cannot execute): sweep the entry rsp across a full
// alignment period by C recursion. Each of 128 levels calls sum9_spill and
// verifies the exact checksum. The three incoming stack words (a6..a8) must
// read correctly on EVERY mod-64 entry residue; a static-displacement entry
// load — or a wrong prologue saveOff base — is correct on at most one
// residue class and the check fails deterministically on the rest. The
// outgoing call result R is computed by the real helper9, so any
// spillShift mis-rebasing inside the outgoing window (a wrong frame slot
// address) corrupts the marshalling sources and fails the check the same
// way.
static int ok = 1;
static void run(int k) {
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 128) run(k + 1);
    long r = sum9_spill(1, 2, 3, 4, 5, 6, 7, 8, 9);
    __asm__ volatile("" ::: "memory");
    long expect = helper9(45, 1001, 1002, 1007, 1008, 1003, 1004, 1005, 1006)
        + 45 + 1001 + 1002 + 1003 + 1004 + 1009 + 1010 + 1011 + 1012;
    ok &= (r == expect);
    for (int i = 16; i < 256; ++i) ok &= (wallseg[i] == (char)(0xA5 + i));
}
int main() {
    run(0);
    printf("and64stackargs-spill %s\n", ok ? "ok" : "BAD");
    return 0;
}
