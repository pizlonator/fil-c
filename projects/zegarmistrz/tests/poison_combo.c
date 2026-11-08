// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: mirrors the repo-root test.c / test2.c repro flows as argv subtests
// (those files are the user's repro cases and must not change). The slot is
// poisoned with ALL THREE bits (0x4Cu = cannot-branch | cannot-index |
// cannot-var-time-op) exactly like test.c/test2.c, then the loaded value is
// used by one of the flows:
//
//   jcc          test.c's commented-out `if (value)` branch
//                -> cannot-branch (CTRL check on the flags).
//   array-store  test.c's commented-out `array[value] = 42`
//                -> cannot-index (either a tainted-index store or a
//                lea+store; the lea exemption must not swallow this one --
//                the store itself still touches memory).
//   div          test.c's commented-out `42 / value`
//                -> var-time (doc 4.1: DIV's operand is a VAL dependency).
//   lea-div      test.c's ACTIVE flow (`x = value + 42` then `42 / x`):
//                the add is pure arithmetic, so NO cannot-index may appear
//                in stderr; the idiv faults with var-time. The LEA is
//                forced via inline asm because GCC -O2 compiles the plain
//                C `value + 42` to an ADD here (`add $0x2a,%ecx`), which
//                would leave this subtest with no LEA to guard
//                (poison-index-lea remains the broad LEA-form guard; this
//                subtest pins test.c's exact flow, whose codegen was
//                `lea 0x2a(%rax),%ecx`).
//   mul-div      test2.c's ACTIVE flow (`x = value * y` then `42 / x`):
//                same contract as lea-div.
//
// Unknown subtest -> exit 2. Faulting subtests end in NOTREACHED.

#include <stdint.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

static volatile int vslot = 666;

// Poison with ALL THREE bits, then volatile-load: the value carries
// cannot-branch + cannot-index + cannot-var-time-op.
static int load_tainted(void) {
    magic(1, (void*)&vslot, sizeof(vslot), 0x4Cu);
    return vslot;
}

int main(int argc, char** argv) {
    const char* which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "jcc") == 0) {
        int value = load_tainted();
        // test.c's commented-out test: a branch on the tainted value.
        if (value)
            asm volatile("" : : : "memory");
    } else if (strcmp(which, "array-store") == 0) {
        int value = load_tainted();
        // test.c's commented-out test: a store indexed by the tainted value.
        int array[1000];
        asm volatile("" : : "r"(array) : "memory");
        array[value] = 42;
        asm volatile("" : : "r"(array) : "memory");
    } else if (strcmp(which, "div") == 0) {
        int value = load_tainted();
        // test.c's commented-out test: a division by the tainted value.
        asm volatile("" : : "r"(42 / value) : "memory");
    } else if (strcmp(which, "lea-div") == 0) {
        int value = load_tainted();
        // test.c's ACTIVE flow: the +42 is pure address arithmetic (test.c
        // compiles to `lea 0x2a(%rax),%ecx`), so it must NOT trip
        // cannot-index; the idiv then faults with the var-time reason only.
        // The LEA is forced via inline asm: GCC -O2 compiles the plain C
        // `value + 42` to `add $0x2a,%ecx` here, which would make this
        // subtest contain no LEA at all (poison-index-lea remains the broad
        // LEA-form guard; this case pins test.c's exact flow).
        int x;
        asm volatile("lea 0x2a(%1), %k0" : "=r"(x) : "r"((uint64_t)value));
        asm volatile("" : "+r"(x) : : "memory");
        asm volatile("" : : "r"(42 / x) : "memory");
    } else if (strcmp(which, "mul-div") == 0) {
        int value = load_tainted();
        // test2.c's ACTIVE flow: mul then div; the idiv faults with the
        // var-time reason only.
        int y = 4;
        asm volatile("" : "+r"(y) : : "memory");
        int x = value * y;
        asm volatile("" : "+r"(x) : : "memory");
        asm volatile("" : : "r"(42 / x) : "memory");
    } else {
        return 2;
    }
    // Unreachable when the emulator is correct: every subtest faults.
    write(1, "NOTREACHED\n", 11);
    return 0;
}
