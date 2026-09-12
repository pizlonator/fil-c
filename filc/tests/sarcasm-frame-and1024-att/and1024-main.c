#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
long and1024_test(long* a, long* b, long n);
// Canary wall against cluster escape, swept through a full 1024-byte period
// by C recursion: each level owns a 256-byte pattern-filled wall segment in
// its own frame, directly above the test call's entry rsp, then calls the
// test and verifies the segment. The segment is `volatile` and its address
// escapes through an asm barrier, so it is guaranteed to be a real live
// across-the-call stack object (never optimized, reordered, or promoted). 128 levels walk the entry across ~16KB of
// stack depths (every 32-byte residue class mod 1024 many times over). The
// raw note max is 1024 but the materialized cluster needs only 32: with the
// correct effective-32 emission the ~96-byte cluster sits at 576..672(%rsp),
// inside the 736-byte frame, on every level and no segment is ever touched.
// With the old raw-width shift the same cluster lands ~992 bytes past the
// frame end — aligned and self-consistent, so no data check and no #GP can
// see it — but across the full period many levels necessarily land raw pads
// that throw it upward across their own segments. The data swap is
// re-verified on every level too. Bytes [0..16) of each segment are never
// verified (frame-bottom call scratch, including the fifth-word outgoing
// stack slot, may legally live there); the remaining 240 bytes admit no
// 96-byte hiding place. (Reading the aligned rsp back into C is not an
// option: the frame pass rejects any rsp-as-value read as a frame-address
// escape.)
static long* a;
static long* b;
static int ok = 1;
static void refill(void) {
    a[0] = 11; a[1] = 22; a[2] = 33; a[3] = 44;
    b[0] = 55; b[1] = 66; b[2] = 77; b[3] = 88;
}
static void run(int k) {
    // VOLATILE + address-taken: the wall must be a real 256-byte stack object
    // directly above the test call's entry rsp. A plain `char[256]` could be
    // reordered, partially promoted, or have its fill/verify optimized away
    // (the post-call read is the only use, and the compiler sees the callee
    // only as a prototype). `volatile` forces every fill byte to memory and
    // every verify byte from memory, and the asm escape takes the address
    // (opaque to the optimizer) with a compiler barrier on each side of the
    // call, so the segment stays a live across-the-call stack object that no
    // pass may delete, hoist, or sink past the call.
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0x5A + i);
    char step[96];
    memset(step, k, sizeof step);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 128) run(k + 1);
    refill();
    and1024_test(a, b, 4);
    __asm__ volatile("" ::: "memory");
    // The assembly swaps the two vectors through 32-byte-aligned stack
    // slots (re-verified every level: the swap is idempotent up to the
    // refill above).
    ok &= (a[0] == 55 && a[1] == 66 && a[2] == 77 && a[3] == 88
        && b[0] == 11 && b[1] == 22 && b[2] == 33 && b[3] == 44);
    for (int i = 16; i < 256; ++i) ok &= (wallseg[i] == (char)(0x5A + i));
    ok &= (step[0] == (char)k);
}
int main() {
    a = aligned_alloc(32, 32);
    b = aligned_alloc(32, 32);
    run(0);
    printf("and1024 %s\n", ok ? "ok" : "BAD");
    return 0;
}
