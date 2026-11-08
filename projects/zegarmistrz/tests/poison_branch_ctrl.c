// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: the cannot-BRANCH control-flow battery. One guest, one argv subtest
// per control-flow check site; every subtest MUST fault (exit 139 + core
// dump) with "cannot-branch" in stderr. Orthogonality: cannot-branch is the
// SOLE control-flow dimension -- branch conditions (flags), branch targets
// (indirect call/jmp), loop/jcxz counters (RCX), rep counts (RCX), and
// return addresses (ret). cannot-index must never fire at these sites (see
// tests/poison_ortho.c, which pins the other direction: cannot-index-
// poisoned values must run cleanly through all of them).
//
// Subtests (see run-tests for the per-subtest case list):
//   jcc            if (value) on a cannot-branch-poisoned int
//                  -> check_flags_taint ("branch condition")
//   jcxz           jrcxz with a cannot-branch-poisoned RCX
//                  -> exec_jcxz ("RCX in jcxz")
//   loop           loop with a cannot-branch-poisoned RCX
//                  -> exec_loop ("RCX in loop")
//   rep-movs       rep movsb with a cannot-branch-poisoned RCX count
//                  -> check_rep_taint ("RCX in rep string op")
//   indirect-call  call through a cannot-branch-poisoned function pointer
//                  -> branch_target ("branch target")
//   indirect-jmp   jmp through a cannot-branch-poisoned target
//                  -> branch_target ("branch target")
//   ret-addr       push a cannot-branch-poisoned value, then ret
//                  -> exec_ret ("return address")
//
// Same tainted-slot pattern as poison_index_addr.c: poison the SLOT and
// volatile-load the value (the taint rides the value); every tainted value
// points at a real function, so a regressed check shows up as exit 0,
// NOTREACHED, or a wrong-reason stderr -- in every case the subtest fails.

#include <stdint.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

// Real no-op function: a regressed call/jmp/ret survives it (safe).
static void ok_fn(void) {}
static void (*volatile fslot)(void) = ok_fn;
static volatile uint64_t slot;
static volatile int islot = 666;

// CANNOT_BRANCH only (flags 4u): the control-flow dimension.
static uint64_t tainted_u64(uint64_t v) {
    slot = v;
    magic(1, (void*)&slot, sizeof(slot), 4u); // op=poison, cannot-branch
    return slot; // volatile load: the value acquires the taint
}

static char src_buf[64] = {1, 2, 3, 4, 5, 6, 7, 8};
static char dst_buf[64];

int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "jcc") == 0) {
        magic(1, (void*)&islot, sizeof(islot), 4u); // cannot-branch
        int value = islot; // volatile load: the value acquires the taint
        if (value) asm volatile("" : : : "memory");
    } else if (strcmp(which, "jcxz") == 0) {
        // jcxz is not encodable in 64-bit mode; jrcxz is its 64-bit form and
        // takes the same RCX taint path. Count 1: a regressed check sees
        // CX != 0 and falls through (safe).
        uint64_t n = tainted_u64(1);
        asm volatile("jrcxz 1f\n1:" :: "c"(n) : "cc", "memory");
    } else if (strcmp(which, "loop") == 0) {
        // Count 1: a regressed check decrements 1 -> 0 and falls through
        // (safe).
        uint64_t n = tainted_u64(1);
        asm volatile("1: loop 1b" :: "c"(n) : "cc", "memory");
    } else if (strcmp(which, "rep-movs") == 0) {
        // Valid static buffers: only the count is tainted, so a regressed
        // rep runs safely to completion (the control-flow check is the one
        // under test).
        const char* s = src_buf;
        char* d = dst_buf;
        size_t n = (size_t)tainted_u64(4);
        asm volatile("rep movsb" : "+S"(s), "+D"(d), "+c"(n) : : "memory");
    } else if (strcmp(which, "indirect-call") == 0) {
        magic(1, (void*)&fslot, sizeof(fslot), 4u); // cannot-branch
        void (*g)(void) = fslot; // volatile load: the pointer is tainted
        g();
    } else if (strcmp(which, "indirect-jmp") == 0) {
        uint64_t tv = tainted_u64((uint64_t)(uintptr_t)ok_fn);
        asm volatile("jmpq *%0" :: "r"(tv) : "memory");
    } else if (strcmp(which, "ret-addr") == 0) {
        // push propagates the value taint onto the stack slot; the ret then
        // faults on the tainted return address (never actually returns).
        uint64_t tv = tainted_u64((uint64_t)(uintptr_t)ok_fn);
        asm volatile("pushq %0\n\tret" :: "r"(tv) : "memory");
    } else {
        return 2;
    }
    // Unreachable when the emulator is correct: every subtest faults.
    write(1, "NOTREACHED\n", 11);
    return 0;
}
