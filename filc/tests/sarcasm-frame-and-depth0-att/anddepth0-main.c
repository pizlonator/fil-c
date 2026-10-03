#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void anddepth0_test(void* in, void* out);

// Canary wall against cluster escape, swept through a full alignment period by
// C recursion: each of 128 levels owns a 256-byte pattern-filled wall segment
// in its own frame, directly above the test call's entry rsp, then calls the
// test and verifies the segment. The segment is `volatile` with its address
// escaped through an asm barrier, so it is guaranteed to be a real live
// across-the-call stack object (never optimized, reordered, or promoted). 128
// levels walk the entry across ~16KB of stack depths (every alignment residue
// many times over). With the correct emission the aligned vmovapd cluster
// stays inside the callee frame on every level and no segment is ever touched;
// an escaping cluster crosses the segments of the levels whose alignment slack
// points it upward, corrupting verified bytes. The data round-trip is verified
// on every level too: a regression of the depth-0 sentinel bug does not even
// get here — the misaligned vmovapd raises #GP (an uncatchable fault) on the
// first call. (Reading the aligned rsp back into C is not an option: the frame
// pass rejects any rsp-as-value read — `movq %rsp, %rax`, `movl %esp, %eax`,
// `pushq %rsp` — as a frame-address escape, so the bounds proof is this wall,
// and the width proof is the #GP on the real aligned accesses.)
static unsigned char* buf;
static unsigned char* out;
static int ok = 1;

static void run(int k) {
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 128) run(k + 1);
    anddepth0_test(buf, out);
    __asm__ volatile("" ::: "memory");
    // The assembly loads buf[0..32) with vmovupd, round-trips it through one
    // 32-byte aligned stack slot (vmovapd), and stores it to out with
    // vmovupd. A misaligned frame faults on the vmovapd itself; overlapping
    // slots, clobbered roots, or a cluster escape corrupt this check. Idempotent
    // across levels.
    for (int i = 0; i < 32; ++i) ok &= (out[i] == (unsigned char)(100 + i));
    for (int i = 16; i < 256; ++i) ok &= (wallseg[i] == (char)(0xA5 + i));
}

int main() {
    buf = aligned_alloc(32, 32);
    out = aligned_alloc(32, 32);
    for (int i = 0; i < 32; ++i) buf[i] = (unsigned char)(100 + i);
    memset(out, 0, 32);
    run(0);
    printf("anddepth0 %s\n", ok ? "ok" : "BAD");
    return 0;
}
