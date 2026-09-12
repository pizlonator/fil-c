#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
long and32_test(int* p);
// Canary wall against cluster escape, swept through a full alignment period
// by C recursion: each level owns a 256-byte pattern-filled wall segment in
// its own frame, directly above the test call's entry rsp, then calls the
// test and verifies the segment. The segment is `volatile` with its address
// escaped through an asm barrier, so it is guaranteed to be a real live
// across-the-call stack object (never optimized, reordered, or promoted). 128 levels walk the entry across ~16KB of
// stack depths (every alignment residue many times over). With the correct
// emission the aligned cluster stays inside the callee frame on every level
// and no segment is ever touched; an escaping cluster (the raw-width bug
// class: offsets ~992 too high — aligned and self-consistent, so no data
// check and no #GP can see it) crosses the segments of the levels whose
// alignment slack points it upward, corrupting verified bytes. The data
// round-trip is verified on every level too. Bytes [0..16) of each segment
// are never verified (frame-bottom call scratch may legally live there);
// the remaining 240 bytes admit no 96-byte hiding place. (Reading the
// aligned rsp back into C is not an option: the frame pass rejects any
// rsp-as-value read — `movq %rsp, %rax`, `movl %esp, %eax`, `pushq %rsp` —
// as a frame-address escape, so the bounds proof is this wall, and the
// width proof is the #GP on the real aligned accesses.)
static int* buf;
static int ok = 1;
static void run(int k) {
    // VOLATILE + address-taken: see and1024-main.c — the wall must stay a real
    // 256-byte across-the-call stack object. `volatile` forces every byte
    // through memory and the asm escape + barriers keep the optimizer from
    // deleting, reordering, or promoting it past the test call.
    volatile char wallseg[256];
    for (int i = 0; i < 256; ++i) wallseg[i] = (char)(0xA5 + i);
    char step[96];
    memset(step, k, sizeof step);
    __asm__ volatile("" : : "r"(&wallseg[0]) : "memory");
    if (k + 1 < 128) run(k + 1);
    and32_test(buf);
    __asm__ volatile("" ::: "memory");
    // The assembly round-trips buf[0..8) through one aligned stack slot and
    // writes zeros (a second live pattern in a neighboring aligned slot,
    // copied through the %rax alias) to buf[8..16): overlapping slots,
    // clobbered roots, or a misaligned frame corrupt this check (or fault
    // on the vmovdqa itself). Idempotent across levels.
    for (int i = 0; i < 8; ++i) ok &= (buf[i] == 100 + i);
    for (int i = 8; i < 16; ++i) ok &= (buf[i] == 0);
    for (int i = 16; i < 256; ++i) ok &= (wallseg[i] == (char)(0xA5 + i));
    ok &= (step[0] == (char)k);
}
int main() {
    buf = aligned_alloc(32, 64);
    for (int i = 0; i < 8; ++i) buf[i] = 100 + i;
    for (int i = 8; i < 16; ++i) buf[i] = 0;
    run(0);
    printf("and32 %s\n", ok ? "ok" : "BAD");
    return 0;
}
