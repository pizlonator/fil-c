// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Test: carry/flag and BMI2/ADX taint propagation (sbb, adc, rcl, adcx, mulx,
// pushf). Each mode starts from clean destination registers so taint reaches
// the final branch solely via the tested instruction. Expect exit 139 with
// cannot-branch for every mode.
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static uint64_t magic(uint64_t op, void* ptr, size_t size, uint64_t flags) {
    return ((uint64_t (*)(uint64_t, void*, size_t, uint64_t))0x1410141014101410ULL)(
        op, ptr, size, flags);
}

int main(int argc, char** argv) {
    if (argc != 2)
        return 2;
    const char* mode = argv[1];
    static volatile uint64_t secret = 3;
    magic(1, (void*)&secret, sizeof(secret), 4u); // CANNOT_BRANCH
    uint64_t s = secret;
    uint64_t res = 0;

    if (strcmp(mode, "sbb") == 0) {
        // cmp sets CF from tainted s; sbb reg,reg materializes -CF into clean res.
        __asm__ volatile(
            "xor %0, %0\n\t"
            "cmp $5, %1\n\t"
            "sbb %0, %0\n\t"
            : "=&r"(res) : "r"(s) : "cc");
    } else if (strcmp(mode, "adc") == 0) {
        __asm__ volatile(
            "xor %0, %0\n\t"
            "cmp $5, %1\n\t"
            "adc $0, %0\n\t"
            : "=&r"(res) : "r"(s) : "cc");
    } else if (strcmp(mode, "rcl") == 0) {
        __asm__ volatile(
            "xor %0, %0\n\t"
            "cmp $5, %1\n\t"
            "rcl $1, %0\n\t"
            : "=&r"(res) : "r"(s) : "cc");
    } else if (strcmp(mode, "adcx") == 0) {
        uint64_t zero = 0;
        __asm__ volatile(
            "xor %0, %0\n\t"
            "cmp $5, %1\n\t"
            "adcx %2, %0\n\t"
            : "=&r"(res) : "r"(s), "r"(zero) : "cc");
    } else if (strcmp(mode, "mulx") == 0) {
        uint64_t hi = 0, lo = 0, factor = 7;
        __asm__ volatile(
            "mulx %3, %1, %0\n\t"
            : "=&r"(hi), "=&r"(lo) : "d"(s), "r"(factor));
        res = lo ^ hi;
    } else if (strcmp(mode, "pushf") == 0) {
        // Save tainted flags with pushfq, clear flags, restore with popfq,
        // then materialize CF via sbb (or setc).
        __asm__ volatile(
            "cmp $5, %1\n\t"
            "pushfq\n\t"
            "xor %0, %0\n\t"
            "popfq\n\t"
            "setc %b0\n\t"
            : "=&q"(res) : "r"(s) : "cc");
    } else {
        return 2;
    }

    if (res == 0)
        return 1;
    if (write(1, "missed-carry\n", 13) < 0)
        return 2;
    return 0;
}
