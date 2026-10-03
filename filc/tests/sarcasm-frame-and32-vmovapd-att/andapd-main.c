#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void andapd_test(void* in, void* out);
extern void andapd_rbp_test(void* in, void* out);

// Canary wall against cluster escape, swept through a full alignment period by
// C recursion (64 levels — same pattern as sarcasm-frame-and32-att, shorter):
// each level owns a 256-byte pattern-filled wall segment in its own frame,
// directly above the test call's entry rsp, calls both test functions, and
// verifies the segment plus both data round-trips. The wall segments are
// `volatile` with their addresses escaped through an asm barrier, so they stay
// real live across-the-call stack objects. The aligned vmovapd/vmovaps/vmovdqa
// accesses are real materialized accesses: any misalignment raises #GP (an
// uncatchable fault) and any cluster escape corrupts verified wall bytes, so
// neither a layout nor a bounds regression can pass silently. Bytes [0..16) of
// each segment are never verified (frame-bottom call scratch may legally live
// there).
static unsigned char* buf;
static unsigned char* out1;
static unsigned char* out2;
static int ok = 1;

static void run(int k) {
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 64) run(k + 1);
    andapd_test(buf, out1);
    andapd_rbp_test(buf, out2);
    __asm__ volatile("" ::: "memory");
    // andapd_test: buf[0..32) round-trips through the 0(%rsp) vmovapd slot into
    // out1[0..32); buf[0..16) round-trips through the 64(%rsp) vmovaps slot
    // into out1[32..48). out1[48..64) must still be zero.
    for (int i = 0; i < 32; ++i) ok &= (out1[i] == (unsigned char)(100 + i));
    for (int i = 32; i < 48; ++i) ok &= (out1[i] == (unsigned char)(100 + i - 32));
    for (int i = 48; i < 64; ++i) ok &= (out1[i] == 0);
    // andapd_rbp_test: buf[0..32) round-trips through the rbp-relative
    // vmovdqa slot into out2[0..32).
    for (int i = 0; i < 32; ++i) ok &= (out2[i] == (unsigned char)(100 + i));
    for (int i = 16; i < 256; ++i) ok &= (wallseg[i] == (char)(0xA5 + i));
}

int main() {
    buf = aligned_alloc(32, 32);
    out1 = aligned_alloc(32, 64);
    out2 = aligned_alloc(32, 32);
    for (int i = 0; i < 32; ++i) buf[i] = (unsigned char)(100 + i);
    memset(out1, 0, 64);
    memset(out2, 0, 32);
    run(0);
    printf("andapd %s\n", ok ? "ok" : "BAD");
    return 0;
}
