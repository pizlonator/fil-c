#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void midand_test(void* in, void* out);

// Canary wall against cluster escape, swept through a full alignment period by
// C recursion (64 levels — same pattern as sarcasm-frame-and32-att, shorter):
// each level owns a 256-byte pattern-filled wall segment in its own frame,
// directly above the test call's entry rsp, calls the test, and verifies the
// segment plus the data round-trip. The segment is `volatile` with its address
// escaped through an asm barrier, so it stays a real live across-the-call
// stack object. The aligned vmovdqa pair is real materialized traffic: any
// misalignment raises #GP (an uncatchable fault) and any cluster escape
// corrupts verified wall bytes, so neither a layout nor a bounds regression
// can pass silently. Bytes [0..16) of each segment are never verified
// (frame-bottom call scratch may legally live there).
static unsigned char* buf;
static unsigned char* out;
static int ok = 1;

static void run(int k) {
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 64) run(k + 1);
    midand_test(buf, out);
    __asm__ volatile("" ::: "memory");
    // The assembly loads buf[0..32) with vmovupd, round-trips it through one
    // 32-byte aligned stack slot (vmovdqa pair after the mid-function and),
    // and stores it to out with vmovupd. Idempotent across levels.
    for (int i = 0; i < 32; ++i) ok &= (out[i] == (unsigned char)(100 + i));
    for (int i = 16; i < 256; ++i) ok &= (wallseg[i] == (char)(0xA5 + i));
}

int main() {
    buf = aligned_alloc(32, 32);
    out = aligned_alloc(32, 32);
    for (int i = 0; i < 32; ++i) buf[i] = (unsigned char)(100 + i);
    memset(out, 0, 32);
    run(0);
    printf("midand align %s\n", ok ? "ok" : "BAD");
    return 0;
}
