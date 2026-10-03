// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: zegarmistrz reveals its presence via CPUID (cpuid.txt): gate on
// CPUID.1:ECX bit 31 ("hypervisor present"), then match the 12-byte vendor
// signature "Zegarmistrz\0" at CPUID.0x40000000. Prints
// "cpuid-zegarmistrz" (exit 0) under the emulator; prints "cpuid-native"
// (exit 0) anywhere else — the signature must never match there, even if
// bit 31 is set by a foreign hypervisor.
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define ZEG_HYP_LEAF 0x40000000u
#define ZEG_SIG_EBX 0x6167655Au /* "Zega" */
#define ZEG_SIG_ECX 0x73696D72u /* "rmis" */
#define ZEG_SIG_EDX 0x007A7274u /* "trz\0" */

static void do_cpuid(unsigned leaf, unsigned* a, unsigned* b, unsigned* c,
                     unsigned* d) {
    __asm__ __volatile__("cpuid"
                         : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                         : "a"(leaf), "c"(0u));
}

int main(void) {
    unsigned a, b, c, d;

    // Detection always uses both cpuid calls: the gate, then the signature.
    do_cpuid(1u, &a, &b, &c, &d);
    unsigned hv_bit = (c >> 31) & 1u;
    do_cpuid(ZEG_HYP_LEAF, &a, &b, &c, &d);

    char vendor[13];
    memcpy(vendor, &b, 4);
    memcpy(vendor + 4, &c, 4);
    memcpy(vendor + 8, &d, 4);
    vendor[12] = '\0';

    if (hv_bit && !strcmp(vendor, "Zegarmistrz")) {
        // Under zegarmistrz: EAX must report at least the vendor leaf, and
        // undefined hypervisor leaves must read as zeros (the emulator must
        // not leak host hypervisor leaves through).
        if (a < ZEG_HYP_LEAF)
            return 2;
        unsigned a2, b2, c2, d2;
        do_cpuid(ZEG_HYP_LEAF + 1, &a2, &b2, &c2, &d2);
        if (a2 || b2 || c2 || d2)
            return 3;
        write(1, "cpuid-zegarmistrz\n", 18);
        return 0;
    }

    // Native (or foreign hypervisor): bit 31 alone must not fool us — the
    // signature decides, and it must not be ours.
    write(1, "cpuid-native\n", 13);
    return 0;
}
