// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: the cannot-index / cannot-branch ORTHOGONALITY battery. The two
// taint dimensions are orthogonal: cannot-index guards ONLY memory-address
// computations, cannot-branch guards ONLY control flow (branch conditions,
// branch targets, loop/jcxz counters, return addresses). Every subtest here
// uses a poisoned value at a site of the WRONG dimension and must run to
// completion, print "ortho-ok", and exit 0.
//
// Both directions are pinned:
//   jcc            cannot-INDEX-poisoned int branched on -> must NOT fault
//                  (THE regression case: branching on an address-tainted
//                  value trapped here before the orthogonality fix)
//   jcxz           cannot-INDEX-poisoned RCX via jrcxz -> must NOT fault
//   loop           cannot-INDEX-poisoned RCX via loop  -> must NOT fault
//   rep-movs       cannot-INDEX-poisoned RCX rep movsb  -> must NOT fault
//                  (and the bytes must actually move)
//   indirect-call  cannot-INDEX-poisoned function pointer -> must NOT fault
//                  (and the call must actually happen)
//   deref-cbranch  cannot-BRANCH-poisoned pointer loaded through
//                  -> must NOT fault (memory addresses are cannot-index)
//   index-cbranch  cannot-BRANCH-poisoned index in a load address
//                  -> must NOT fault (memory addresses are cannot-index)
//   ret            cannot-INDEX-poisoned return address -> must NOT fault
//                  (and control must actually transfer)
//
// The faulting side of each site lives in poison_index_addr.c (cannot-index,
// memory sites) and poison_branch_ctrl.c (cannot-branch, control-flow
// sites).

#include <stdint.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile uint64_t counter;
static void ok_inc(void) { counter++; } // real function: proves the call ran
static void (*volatile fslot)(void) = ok_inc;
static volatile uint64_t slot;
static volatile int islot = 666;
static char target_buf[64] = {0x5a, 0xa5, 0x11, 0x77, 0xde, 0xad, 0xbe, 0xef,
                              0x21, 0x43, 0x65, 0x87};
static char src_buf[64] = {1, 2, 3, 4, 5, 6, 7, 8};
static char dst_buf[64];

// CANNOT_INDEX only (flags 8u): the memory-address dimension.
static uint64_t tainted_u64(uint64_t v) {
    slot = v;
    magic(1, (void*)&slot, sizeof(slot), 8u); // op=poison, cannot-index
    return slot; // volatile load: the value acquires the taint
}

int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "jcc") == 0) {
        magic(1, (void*)&islot, sizeof(islot), 8u); // cannot-index
        int value = islot; // volatile load: the value acquires the taint
        // Branch on an address-tainted value: legal. (This is exactly the
        // shape that trapped inside glibc memcmp in the reporter repro:
        // test %eax,%eax; jne.) Single-arm if on purpose: an if/else with
        // two empty-template asm arms gets its branch collapsed by GCC -O2
        // (no test/jne at all), which would make this subtest vacuous.
        if (value)
            asm volatile("" : : : "memory");
    } else if (strcmp(which, "jcxz") == 0) {
        uint64_t n = tainted_u64(1);
        asm volatile("jrcxz 1f\n1:" :: "c"(n) : "cc", "memory");
    } else if (strcmp(which, "loop") == 0) {
        // loop writes RCX, so the count is an in+out operand.
        uint64_t n = tainted_u64(1);
        asm volatile("1: loop 1b" : "+c"(n) : : "cc", "memory");
    } else if (strcmp(which, "rep-movs") == 0) {
        const char* s = src_buf;
        char* d = dst_buf;
        size_t n = (size_t)tainted_u64(4);
        asm volatile("rep movsb" : "+S"(s), "+D"(d), "+c"(n) : : "memory");
        // Prove the copy really happened with the tainted count.
        if (memcmp(dst_buf, src_buf, 4) != 0) return 1;
    } else if (strcmp(which, "indirect-call") == 0) {
        magic(1, (void*)&fslot, sizeof(fslot), 8u); // cannot-index
        void (*g)(void) = fslot; // volatile load: the pointer is tainted
        g();
        // Prove the call really happened through the tainted pointer.
        if (counter != 1) return 1;
    } else if (strcmp(which, "deref-cbranch") == 0) {
        // Reverse direction: a cannot-BRANCH-poisoned pointer is a fine
        // memory address (only cannot-index guards those).
        slot = (uint64_t)(uintptr_t)target_buf;
        magic(1, (void*)&slot, sizeof(slot), 4u); // cannot-branch
        uint64_t tp = slot; // volatile load: the pointer acquires the taint
        uint64_t out;
        asm volatile("movq (%1), %0" : "=r"(out) : "r"(tp) : "memory");
        if (out != 0xefbeadde7711a55aULL) return 1;
    } else if (strcmp(which, "index-cbranch") == 0) {
        // Reverse direction: a cannot-BRANCH-poisoned index in a load
        // address is fine (only cannot-index guards those).
        slot = 2;
        magic(1, (void*)&slot, sizeof(slot), 4u); // cannot-branch
        uint64_t ti = slot; // volatile load: the index acquires the taint
        uint32_t out32;
        asm volatile("movl (%1,%2,4), %0"
                     : "=r"(out32)
                     : "r"((uint64_t)target_buf), "r"(ti)
                     : "memory");
        // Index 2, scale 4 -> bytes 8..11 of target_buf = 21 43 65 87.
        if (out32 != 0x87654321U) return 1;
    } else if (strcmp(which, "ret") == 0) {
        // Computed-goto label as a return address: the ret transfers to
        // ortho_ret_done just above the common tail. Keep this subtest LAST
        // in the dispatch chain.
        static void* volatile ret_slot;
        ret_slot = &&ortho_ret_done;
        magic(1, (void*)&ret_slot, sizeof(ret_slot), 8u); // cannot-index
        uint64_t tv = (uint64_t)ret_slot; // volatile load: tainted
        asm volatile("pushq %0\n\tret" :: "r"(tv) : "memory");
    } else {
        return 2;
    }
ortho_ret_done:;
    // Every subtest reaches here: poison at a wrong-dimension site must run
    // to completion.
    write(1, "ortho-ok\n", 9);
    return 0;
}
