#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void and16d136_test(void* in, void* out);

// Canary wall against cluster escape, swept through a full alignment period by
// C recursion: each of 128 levels owns a 256-byte pattern-filled wall segment
// in its own frame, directly above the test call's entry rsp, then calls the
// test and verifies the segment. The segment is `volatile` with its address
// escaped through an asm barrier, so it is guaranteed to be a real live
// across-the-call stack object (never optimized, reordered, or promoted). 128
// levels walk the entry across ~16KB of stack depths (every alignment residue
// many times over). With the correct emission the 16-byte `and $-16,%rsp` note
// at depth 136 keys its movaps cluster at the note's own base (off ≡ D0 - 136
// ≡ 0 (mod 16)), the cluster stays inside the callee frame on every level, and
// no segment is ever touched; keying the note at the wrong base (depth 0, the
// 16-byte-note regression) misplaces every movaps by 8 mod 16 — a #GP (an
// uncatchable fault) on the first call. (Reading the aligned rsp back into C
// is not an option: the frame pass rejects any rsp-as-value read —
// `movq %rsp, %rax`, `movl %esp, %eax`, `pushq %rsp` — as a frame-address
// escape, so the bounds proof is this wall, and the width proof is the #GP on
// the real aligned accesses.)
static unsigned char* buf;
static unsigned char* out;
static int ok = 1;

static void run(int k) {
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 128) run(k + 1);
    and16d136_test(buf, out);
    __asm__ volatile("" ::: "memory");
    // The assembly loads buf[0..16) with movupd, round-trips it through four
    // 16-byte aligned stack slots (movaps at 0, 16, 48, and 112 against the
    // post-`andq $-16` rsp), and stores it to out with movupd. A misaligned
    // frame faults on the movaps itself; overlapping slots, clobbered roots,
    // or a cluster escape corrupt this check. Idempotent across levels.
    for (int i = 0; i < 16; ++i) ok &= (out[i] == (unsigned char)(100 + i));
    for (int i = 16; i < 32; ++i) ok &= (out[i] == 0);
    for (int i = 16; i < 256; ++i) ok &= (wallseg[i] == (char)(0xA5 + i));
}

int main() {
    buf = aligned_alloc(16, 16);
    out = aligned_alloc(16, 32);
    for (int i = 0; i < 16; ++i) buf[i] = (unsigned char)(100 + i);
    memset(out, 0, 32);
    run(0);
    printf("and16d136 %s\n", ok ? "ok" : "BAD");
    return 0;
}
