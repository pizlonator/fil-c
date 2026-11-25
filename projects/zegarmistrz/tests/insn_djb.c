// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Instruction-semantics guest for the djb-supercop opcode batch (opcodes his
// crypto workloads hit that the emulator lacked, plus the two fixed forms):
//   - VCVTDQ2PD/CVTDQ2PD (F3 0F E6): packed int32 -> double widening convert
//   - VPSRLVD/VPSLLVD/VPSRAVD (VEX variable shifts; per-lane counts, a count
//     >= the element width yields 0, NOT masked)
//   - VPMULHRSW/PMULHRSW (SSSE3 rounded-high multiply)
//   - VMOVDDUP/MOVDDUP (even 64-bit lane duplicated into the odd lane)
//   - PEXTRD to MEMORY (gcc emits it constantly; errored "bad pextr form")
//   - MOVQ xmm,xmm reg-reg: the load form (F3 0F 7E, what gcc emits for
//     movq %xmm,%xmm) and the store form (66 0F D6 mod=11, raw bytes); both
//     zero the destination's upper 64 bits and leave the upper YMM unchanged
//     (VEX additionally zeroes the upper YMM) -- verified on this host.
// Every group computes its expected bytes in C with the ISA formula and
// memcmps them against the instruction result, so the native run validates
// the formulas against real silicon; the emulator run must then produce the
// exact same output.  Groups gate on runtime CPUID.  Prints insn-djb-ok.
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s\n", (name)); failures++; } \
} while (0)

static int eq(const void* a, const void* b, size_t n) {
    return memcmp(a, b, n) == 0;
}

static unsigned cpuid1_ecx(void) {
    unsigned a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    return c;
}

static unsigned cpuid7_ebx(void) {
    unsigned a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(7), "c"(0));
    return b;
}

static unsigned char buf[32];

int main(void) {
    unsigned ecx1 = cpuid1_ecx();
    int sse3 = (ecx1 >> 1) & 1, ssse3 = (ecx1 >> 9) & 1;
    int sse41 = (ecx1 >> 19) & 1, avx2 = (cpuid7_ebx() >> 5) & 1;

    // ---- CVTDQ2PD legacy (SSE2, F3 0F E6) --------------------------------
    {
        int32_t v[2] = {-12345, 3};
        __m128d r;
        __m128i iv = _mm_setr_epi32(v[0], v[1], 0, 0);
        asm("cvtdq2pd %1, %0" : "=&x"(r) : "x"(iv));
        double want[2] = {(double)v[0], (double)v[1]};
        memcpy(buf, &r, 16);
        CHECK(eq(buf, want, 16), "cvtdq2pd legacy");
        printf("cvtdq2pd-legacy done\n");
    }

    // ---- MOVDDUP legacy (SSE3, F2 0F 12) ----------------------------------
    if (sse3) {
        unsigned long long lv[2] = {0x1111111111111111ULL, 0x2222222222222222ULL};
        __m128d r;
        asm volatile("movddup %1, %0" : "=&x"(r) : "m"(lv));
        unsigned long long ur[2];
        memcpy(ur, &r, 16);
        CHECK(ur[0] == lv[0] && ur[1] == lv[0], "movddup legacy dup");
        printf("movddup-legacy done\n");
    } else {
        printf("skip sse3 movddup\n");
    }

    // ---- PMULHRSW legacy (SSSE3) ------------------------------------------
    if (ssse3) {
        short a[8] = {3, (short)0x3fff, (short)0x4000, -1,
                      32767, (short)-32768, 12345, 1000};
        short b[8] = {1, 1, 1, 1, 32767, (short)-32768, -6789, -1000};
        __m128i ma, mb;
        memcpy(&ma, a, 16);
        memcpy(&mb, b, 16);
        asm("pmulhrsw %1, %0" : "+x"(ma) : "x"(mb));
        for (int i = 0; i < 8; i++) {
            int32_t prod = (int32_t)a[i] * (int32_t)b[i];
            short want = (short)(((prod >> 14) + 1) >> 1);
            CHECK(((short*)&ma)[i] == want, "pmulhrsw lane round");
        }
        printf("pmulhrsw-legacy done\n");
    } else {
        printf("skip ssse3 pmulhrsw\n");
    }

    if (sse41) {
        // ---- PEXTRD to MEMORY (SSE4.1) --------------------------------------
        unsigned slot[4] = {1, 2, 3, 4};
        __m128i v = _mm_setr_epi32(0xCAFEBABEu, 0x1234, 3, 4);
        asm volatile("pextrd $1, %1, %0" : "=m"(slot[3]) : "x"(v));
        CHECK(slot[3] == 0x1234u && slot[0] == 1 && slot[1] == 2 && slot[2] == 3,
              "pextrd m32 lane 1");
        asm volatile("pextrd $3, %1, %0" : "=m"(slot[0]) : "x"(v));
        CHECK(slot[0] == 4u, "pextrd m32 lane 3");
        printf("pextrd-mem done\n");
    } else {
        printf("skip sse41 pextrd-mem\n");
    }

    if (avx2) {
        // ---- VCVTDQ2PD (VEX 128 + 256) --------------------------------------
        {
            __m128i iv = _mm_setr_epi32(-7, 1, 1000000, -2147483647 - 1);
            __m256d d4;
            asm("vcvtdq2pd %1, %0" : "=&x"(d4) : "x"(iv));
            double want[4] = {-7.0, 1.0, 1000000.0, -2147483648.0};
            memcpy(buf, &d4, 32);
            CHECK(eq(buf, want, 32), "vcvtdq2pd ymm");
            __m128d d2;
            asm("vcvtdq2pd %1, %0" : "=&x"(d2) : "x"(iv));
            double want2[2] = {-7.0, 1.0};
            memcpy(buf, &d2, 16);
            CHECK(eq(buf, want2, 16), "vcvtdq2pd xmm");
            printf("vcvtdq2pd done\n");
        }

        // ---- VPSRLVD/VPSLLVD/VPSRAVD (per-lane counts, count 32/33 -> 0) ----
        {
            unsigned av[8] = {0xf0f0f0f0u, 0x80000000u, 1234567u,
                              0xffffffffu, 5u, 6u, 7u, 8u};
            unsigned cv[8] = {4u, 32u, 33u, 1u, 0u, 3u, 4u, 5u};
            unsigned rl[8], rr[8], ra[8];
            __m256i o, a, c;
            memcpy(&a, av, 32);
            memcpy(&c, cv, 32);
            asm("vpsllvd %2, %1, %0" : "=&x"(o) : "x"(a), "x"(c));
            memcpy(rl, &o, 32);
            asm("vpsrlvd %2, %1, %0" : "=&x"(o) : "x"(a), "x"(c));
            memcpy(rr, &o, 32);
            asm("vpsravd %2, %1, %0" : "=&x"(o) : "x"(a), "x"(c));
            memcpy(ra, &o, 32);
            for (int i = 0; i < 8; i++) {
                unsigned v = av[i], cnt = cv[i];
                unsigned wl = cnt >= 32 ? 0 : v << cnt;
                unsigned wr = cnt >= 32 ? 0 : v >> cnt;
                unsigned wa = cnt >= 32 ? ((int32_t)v < 0 ? 0xffffffffu : 0)
                                        : (unsigned)((int32_t)v >> cnt);
                CHECK(rl[i] == wl, "vpsllvd lane");
                CHECK(rr[i] == wr, "vpsrlvd lane");
                CHECK(ra[i] == wa, "vpsravd lane");
            }
            printf("vpslv done\n");
        }

        // ---- VPMULHRSW (VEX) -------------------------------------------------
        {
            short a[8] = {16384, -16385, 2, -2, 100, -100, 256, 1};
            short b[8] = {1, 1, 16384, -16384, -256, 256, 129, -1};
            __m128i o, ma, mb;
            memcpy(&ma, a, 16);
            memcpy(&mb, b, 16);
            asm("vpmulhrsw %2, %1, %0" : "=&x"(o) : "x"(ma), "x"(mb));
            for (int i = 0; i < 8; i++) {
                int32_t prod = (int32_t)a[i] * (int32_t)b[i];
                short want = (short)(((prod >> 14) + 1) >> 1);
                CHECK(((short*)&o)[i] == want, "vpmulhrsw lane round");
            }
            printf("vpmulhrsw done\n");
        }

        // ---- VMOVDDUP (VEX 128 + 256) ----------------------------------------
        {
            static const unsigned long long sv[2] = {0x1111111111111111ULL,
                                                     0x2222222222222222ULL};
            static const unsigned long long sv4[4] = {1ULL, 2ULL, 3ULL, 4ULL};
            __m128d r2;
            asm volatile("vmovddup %1, %0" : "=&x"(r2) : "m"(sv));
            unsigned long long ur[2];
            memcpy(ur, &r2, 16);
            CHECK(ur[0] == sv[0] && ur[1] == sv[0], "vmovddup xmm");
            __m256d r4;
            asm volatile("vmovddup %1, %0" : "=&x"(r4) : "m"(sv4));
            unsigned long long ur4[4];
            memcpy(ur4, &r4, 32);
            CHECK(ur4[0] == 1 && ur4[1] == 1 && ur4[2] == 3 && ur4[3] == 3,
                  "vmovddup ymm");
            printf("vmovddup done\n");
        }

        // ---- VPEXTRD to MEMORY -----------------------------------------------
        {
            unsigned slot[4] = {1, 2, 3, 4};
            __m128i v = _mm_setr_epi32(0xCAFEBABEu, 0x5678, 3, 4);
            asm volatile("vpextrd $2, %1, %0" : "=m"(slot[0]) : "x"(v));
            CHECK(slot[0] == 3u && slot[3] == 4u, "vpextrd m32 lane 2");
            printf("vpextrd-mem done\n");
        }

        // ---- MOVQ xmm,xmm reg-reg: load + store forms, upper lanes -----------
        {
            static const unsigned long long d0[4] = {
                0xDEAD0000DEAD1111ULL, 0xCAFEBABECAFEBABEULL,
                0x1122334411223344ULL, 0x5566778855667788ULL};
            unsigned long long s = 0xAAAAAAAAAAAAAAAAULL;
            __m256i d0v;
            unsigned long long r[4];
            memcpy(&d0v, d0, 32);
            // gcc's movq %xmm1,%xmm0 is the F3 0F 7E load form: zeroes upper
            // XMM, keeps upper YMM.
            asm volatile("vmovups %1, %%ymm0\n\t"
                         "vmovq %2, %%xmm1\n\t"
                         "movq %%xmm1, %%xmm0\n\t"
                         "vmovups %%ymm0, %0"
                         : "=m"(r) : "x"(d0v), "m"(s) : "%ymm0", "%xmm1");
            CHECK(r[0] == s && r[1] == 0 && r[2] == d0[2] && r[3] == d0[3],
                  "movq reg-reg load form zeroes upper xmm, keeps upper ymm");
            // The 66 0F D6 store form (raw bytes; dest=r/m=xmm0, src=reg=xmm1):
            // same behavior on this hardware.
            asm volatile("vmovups %1, %%ymm0\n\t"
                         "vmovq %2, %%xmm1\n\t"
                         ".byte 0x66, 0x0f, 0xd6, 0xc8\n\t"
                         "vmovups %%ymm0, %0"
                         : "=m"(r) : "x"(d0v), "m"(s) : "%ymm0", "%xmm1");
            CHECK(r[0] == s && r[1] == 0 && r[2] == d0[2] && r[3] == d0[3],
                  "movq reg-reg store form zeroes upper xmm, keeps upper ymm");
            // VMOVQ reg-reg zeroes the whole upper YMM (VEX zeroing rule).
            asm volatile("vmovups %1, %%ymm0\n\t"
                         "vmovq %2, %%xmm1\n\t"
                         "vmovq %%xmm1, %%xmm0\n\t"
                         "vmovups %%ymm0, %0"
                         : "=m"(r) : "x"(d0v), "m"(s) : "%ymm0", "%xmm1");
            CHECK(r[0] == s && r[1] == 0 && r[2] == 0 && r[3] == 0,
                  "vmovq reg-reg zeroes upper ymm");
            printf("movq-regreg done\n");
        }
    } else {
        printf("skip avx2 groups\n");
    }

    if (failures == 0) {
        printf("insn-djb-ok\n");
        return 0;
    }
    printf("insn-djb FAILED: %d\n", failures);
    return 1;
}
