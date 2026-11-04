// Copyright (c) 2026 Filip Pizlo.  All Rights Reserved.
// Regression guest for the zegarmistrz thread-local decode cache: the cache
// (keyed by guest RIP, byte-validated) must never be instruction-visible.
// A function at a FIXED address is called repeatedly while its immediate
// operand is rewritten between calls, so every call re-executes the same RIP
// with DIFFERENT instruction bytes.  A cache that served a stale decode (or
// any emulator that stopped re-reading guest code bytes) would return the
// old immediate; this guest requires the new one.  Also exercises the cache
// hit path (same RIP, same bytes, back to back) and two functions landing in
// the same direct-mapped slot.  Prints "dec-cache-ok" and exits 0.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef unsigned long (*fn1_t)(void);

// mov eax, imm32; ret  (b8 dd dd dd dd c3)
static const unsigned char kMovRet[6] = {0xb8, 0, 0, 0, 0, 0xc3};

static void emit(unsigned char* code, unsigned long value) {
    unsigned char buf[6];
    memcpy(buf, kMovRet, 6);
    buf[1] = (unsigned char)(value & 0xff);
    buf[2] = (unsigned char)((value >> 8) & 0xff);
    buf[3] = (unsigned char)((value >> 16) & 0xff);
    buf[4] = (unsigned char)((value >> 24) & 0xff);
    memcpy(code, buf, 6);
}

int main(void) {
    unsigned char* code = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    fn1_t f1 = (fn1_t)code;
    fn1_t f2 = (fn1_t)(code + 0x400);  // same page, different RIP

    // Same RIP, same bytes, many times in a row: exercise hits.
    emit(code, 0x11111111);
    for (int i = 0; i < 1000; i++) {
        if (f1() != 0x11111111UL) {
            printf("FAIL dec-cache hit path\n");
            return 1;
        }
    }

    // Same RIP, DIFFERENT bytes, repeatedly: the decode cache must validate
    // against the actual bytes.  Alternate values so any stale entry fails.
    for (int i = 0; i < 1000; i++) {
        unsigned long want = (i & 1) ? 0xcafef00dUL + (unsigned long)i
                                     : 0xbeefcafeUL - (unsigned long)i;
        emit(code, want);
        if (f1() != want) {
            printf("FAIL dec-cache self-modifying (i=%d)\n", i);
            return 1;
        }
    }

    // Two functions whose RIPs collide in a direct-mapped cache (same slot)
    // but carry different bytes; interleave calls and rewrite both.
    for (int i = 0; i < 1000; i++) {
        unsigned long w1 = 0x1000UL + (unsigned long)i;
        unsigned long w2 = 0x2000000UL - (unsigned long)i;
        emit(code, w1);
        emit(code + 0x400, w2);
        if (f1() != w1 || f2() != w2) {
            printf("FAIL dec-cache slot collision (i=%d)\n", i);
            return 1;
        }
    }

    // A longer stub whose middle is rewritten: mov eax, imm32; nop; ret.
    // (Different length/prefix mix than the 6-byte stub above, so a different
    // instruction length is decoded at the same RIP over time.)
    unsigned char* g = code + 0x800;
    g[0] = 0xb8;
    g[1] = 0x42;
    g[2] = 0x00;
    g[3] = 0x00;
    g[4] = 0x00;
    g[5] = 0x90;  // nop
    g[6] = 0xc3;  // ret
    if (((fn1_t)g)() != 0x42) {
        printf("FAIL dec-cache long stub\n");
        return 1;
    }
    g[1] = 0x43;
    if (((fn1_t)g)() != 0x43) {
        printf("FAIL dec-cache long stub rewrite\n");
        return 1;
    }

    printf("dec-cache-ok\n");
    munmap(code, 4096);
    return 0;
}
