// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// tests/shufpd.c -- the SHUFPD-family regression guest, AVX2-level part:
// legacy SHUFPD (66 0F C6) reg-reg and m128, the SHUFPS companions whose
// roles the PD form must NOT borrow, VSHUFPD (VEX.128/VEX.256), the
// VPERMIL imm-form control rules (PD global vs PS per-lane), and the
// end-to-end Keccak reference workload.  This guest is the
// native-vs-emulated mirror (run-tests case `shufpd`) and is compiled
// WITHOUT -mavx512f -- which is the whole point of the split: with
// -mavx512f in the compile line, gcc is entitled to auto-vectorize ANY of
// this file's plain C (the expected-array setup loops, the memcmp
// compares, the Keccak rounds) with 512-bit EVEX instructions, and on a
// host without AVX-512 the native leg then dies with SIGILL before
// printing a single line -- observed on the i9-14900HX host (megadeth)
// whose gcc vectorized the non-gated check code to zmm while this
// container's gcc 11.4 happened not to (a gcc-version-dependent
// landmine).  No -mavx512f means gcc CANNOT emit 512-bit code at all:
// the ISA baseline is the union of the -m flags (plus -march), so
// auto-vectorization tops out at the -mavx2 ymm width, and ymm runs
// natively on the AVX2 hosts this suite targets -- dropping the flag is
// sufficient by construction, no -fno-tree-vectorize insurance needed.
// The emulator-only AVX-512 SDM-conformance battery (masked {k1}{z}
// VSHUFPD/VPSHUFD/VPERMILPD, the {1toN} embedded broadcasts, ternlog,
// the converts, the VPERMQ/VPERMPD vector-control forms, and the
// have_avx512f()-gated unmasked EVEX.512 VSHUFPD imm group) lives in
// tests/shufpd_sdm.c, which the suite never runs natively (run-tests
// case `shufpd-sdm`).
//
// The serious bug this pins: gcc -O3 builds of djb supercop crypto_core/
// keccakf160064bits/reference1600 emit eight shufpd $1 sites per Keccak
// round (the theta column D values are built with punpcklqdq + shufpd $1),
// and the emulator computed [imm0?SRC.q0:DEST.q0, imm1?SRC.q1:DEST.q1] --
// picking the SOURCE by the imm bit -- instead of the hardware rule
//
//   out.q[0] = imm[0] ? DEST.q[1] : DEST.q[0]    (selects WITHIN DEST)
//   out.q[1] = imm[1] ? SRC.q[1]  : SRC.q[0]    (selects WITHIN SRC)
//
// which swapped a pair of theta columns every round and silently permuted
// the whole 1600-bit state differently from bare metal (no fault, wrong
// output: fnv fcfa0f4eb454b7d4 native vs 8496bcb3d78151b4 emulated).
// Hardware-verified on i9-14900HX: with DEST=[A,A] and SRC=[B,B] every
// imm8 yields [A.lo, B.lo]; gcc -O0 folds _mm_shuffle_pd imm0 to UNPCKLPD
// and imm3 to UNPCKHPD, pinning DEST.lo / SRC.hi as the imm=0 endpoints.
// The VEX.256 imm bits index GLOBAL qwords (the upper lane uses bits 2/3,
// unlike VSHUFPS which restarts its bit pairs per lane).
// The SHUFPS groups pin the PS rule the PD code shares a handler with:
// output dwords 0/1 select WITHIN DEST by imm bits [1:0]/[3:2], dwords 2/3
// select WITHIN SRC by imm bits [5:4]/[7:6], and the bit pairs restart per
// lane at 256-bit (verified: imm=0x11 gives lane1 = [src1.d1, src1.d0,
// src2.d1, src2.d0]).
// Also runs the full 24-round Keccak-f[1600] permutation of the djb
// reference core over a deterministic state and checks the FNV-1a hash of
// the result end to end (the workload that reported the bug).
// Gating: the legacy groups are plain SSE2 and the VEX/VPERMIL groups are
// gated on runtime CPUID (AVX), so they run everywhere, natively AND
// under the emulator (the native-vs-emulated mirror).  Prints shufpd-ok.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static int failures = 0;
#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s\n", (name)); failures++; } \
} while (0)
static void cpuid(unsigned leaf, unsigned sub, unsigned out[4]) {
    asm volatile("cpuid" : "=a"(out[0]), "=b"(out[1]), "=c"(out[2]), "=d"(out[3])
                 : "a"(leaf), "c"(sub));
}
static int have_avx(void) {
    unsigned f[4];
    cpuid(1, 0, f);
    return (f[2] >> 28) & 1;
}
/* the two source vectors, with distinct qwords in both halves of each lane */
static const unsigned long long A[8] = {0xAAAAAAAAAAAAAAAAULL, 0xA1A2A3A4A5A6A7A8ULL,
                                        0xBBBBBBBBBBBBBBBBULL, 0xB1B2B3B4B5B6B7B8ULL,
                                        0xA0A0A0A0A0A0A0A0ULL, 0xA9A8A7A6A5A4A3A2ULL,
                                        0xB0B0B0B0B0B0B0B0ULL, 0xB9B8B7B6B5B4B3B2ULL};
static const unsigned long long B[8] = {0xCCCCCCCCCCCCCCCCULL, 0xC1C2C3C4C5C6C7C8ULL,
                                        0xDDDDDDDDDDDDDDDDULL, 0xD1D2D3D4D5D6D7D8ULL,
                                        0xC0C0C0C0C0C0C0C0ULL, 0xC9C8C7C6C5C4C3C2ULL,
                                        0xD0D0D0D0D0D0D0D0ULL, 0xD9D8D7D6D5D4D3D2ULL};
/* VSHUFPD expected qwords: output qword 2L (even) selects WITHIN src1=A by
 * imm bit 2L; output qword 2L+1 (odd) selects WITHIN src2=B by bit 2L+1. */
static unsigned long long exp_pd_q(unsigned imm, unsigned lane, unsigned odd) {
    const unsigned long long *src = odd ? B : A;
    unsigned sel = (imm >> (2 * lane + odd)) & 1;
    return src[2 * lane + sel];
}
/* VSHUFPS expected dword: dwords 0/1 of each lane select within src1=A by
 * imm bits [1:0]/[3:2]; dwords 2/3 select within src2=B by bits [5:4]/[7:6].
 * The bit pairs restart per lane. */
static unsigned int exp_ps_d(unsigned imm, unsigned lane, unsigned i) {
    unsigned sel = (imm >> (2 * i)) & 3;
    unsigned half = (i >= 2);
    const unsigned int *src = half ? (const unsigned int *)B : (const unsigned int *)A;
    return src[4 * lane + 2 * (sel >> 1) + (sel & 1)];
}
int main(void) {
    /* ---- legacy SHUFPD reg-reg, imm 0..3 (SSE2, always present) ---- */
    {
        unsigned long long dst[2] = {A[0], A[1]};
        unsigned long long src[2] = {B[0], B[1]};
#define TEST_LEGACY_PD(imm) do { \
    unsigned long long got[4]; \
    asm volatile("movups %1, %%xmm0\n\t" \
                 "movups %2, %%xmm1\n\t" \
                 "shufpd $" #imm ", %%xmm1, %%xmm0\n\t" \
                 "movups %%xmm0, %0" \
                 : "=m"(got) : "m"(dst), "m"(src) : "%xmm0", "%xmm1"); \
    unsigned long long want[4] = {exp_pd_q(imm, 0, 0), exp_pd_q(imm, 0, 1), 0, 0}; \
    CHECK(memcmp(got, want, 16) == 0, "legacy shufpd reg-reg imm=" #imm); \
    printf("legacy shufpd imm=" #imm " done\n"); \
} while (0)
        TEST_LEGACY_PD(0);
        TEST_LEGACY_PD(1);
        TEST_LEGACY_PD(2);
        TEST_LEGACY_PD(3);
#undef TEST_LEGACY_PD
    }
    /* ---- legacy SHUFPD with the m128 source form ---- */
    {
        unsigned long long dst[2] = {A[0], A[1]};
#define TEST_LEGACY_PD_MEM(imm) do { \
    unsigned long long got[4]; \
    asm volatile("movups %1, %%xmm0\n\t" \
                 "shufpd $" #imm ", %2, %%xmm0\n\t" \
                 "movups %%xmm0, %0" \
                 : "=m"(got) : "m"(dst), "m"(B) : "%xmm0"); \
    unsigned long long want[4] = {exp_pd_q(imm, 0, 0), exp_pd_q(imm, 0, 1), 0, 0}; \
    CHECK(memcmp(got, want, 16) == 0, "legacy shufpd m128 imm=" #imm); \
    printf("legacy shufpd-mem imm=" #imm " done\n"); \
} while (0)
        TEST_LEGACY_PD_MEM(0);
        TEST_LEGACY_PD_MEM(1);
        TEST_LEGACY_PD_MEM(2);
        TEST_LEGACY_PD_MEM(3);
#undef TEST_LEGACY_PD_MEM
    }
    /* ---- legacy SHUFPS reg-reg (pin the PS roles the PD code shares a
     * handler with) ---- */
    {
        const unsigned int Ad[4] = {0xA0A0A0A0u, 0xA1A1A1A1u, 0xA2A2A2A2u, 0xA3A3A3A3u};
        const unsigned int Bd[4] = {0xB0B0B0B0u, 0xB1B1B1B1u, 0xB2B2B2B2u, 0xB3B3B3B3u};
#define TEST_LEGACY_PS(imm) do { \
    unsigned int got[4]; \
    asm volatile("movups %1, %%xmm0\n\t" \
                 "movups %2, %%xmm1\n\t" \
                 "shufps $" #imm ", %%xmm1, %%xmm0\n\t" \
                 "movups %%xmm0, %0" \
                 : "=m"(got) : "m"(Ad), "m"(Bd) : "%xmm0", "%xmm1"); \
    unsigned int want[4]; \
    for (unsigned i = 0; i < 4; i++) { \
        unsigned sel = ((imm) >> (2 * i)) & 3; \
        const unsigned int *s = (i < 2) ? Ad : Bd; \
        want[i] = s[sel]; \
    } \
    CHECK(memcmp(got, want, 16) == 0, "legacy shufps reg-reg imm=" #imm); \
    printf("legacy shufps imm=" #imm " done\n"); \
} while (0)
        TEST_LEGACY_PS(0x00);
        TEST_LEGACY_PS(0x44);
        TEST_LEGACY_PS(0xE4);
        TEST_LEGACY_PS(0x1B);
        TEST_LEGACY_PS(0xFF);
#undef TEST_LEGACY_PS
    }
    if (have_avx()) {
        /* ---- VEX.128 VSHUFPD: dst,src1,src2,imm; upper ymm stays zeroed ---- */
        {
#define TEST_VEX128_PD(imm) do { \
    unsigned long long got[4]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vmovups %2, %%ymm2\n\t" \
                 "vshufpd $" #imm ", %%xmm2, %%xmm1, %%xmm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(A), "m"(B) : "%ymm0", "%ymm1", "%ymm2"); \
    unsigned long long want[4] = {exp_pd_q(imm, 0, 0), exp_pd_q(imm, 0, 1), 0, 0}; \
    CHECK(memcmp(got, want, 16) == 0, "vex128 vshufpd imm=" #imm); \
    printf("vex128 vshufpd imm=" #imm " done\n"); \
} while (0)
            TEST_VEX128_PD(0);
            TEST_VEX128_PD(1);
            TEST_VEX128_PD(2);
            TEST_VEX128_PD(3);
            TEST_VEX128_PD(0x55);
#undef TEST_VEX128_PD
        }
        /* ---- VEX.256 VSHUFPD: global imm bits, upper lane uses bits 2/3 ---- */
        {
#define TEST_VEX256_PD(imm) do { \
    unsigned long long got[4]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vmovups %2, %%ymm2\n\t" \
                 "vshufpd $" #imm ", %%ymm2, %%ymm1, %%ymm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(A), "m"(B) : "%ymm0", "%ymm1", "%ymm2"); \
    unsigned long long want[4] = {exp_pd_q(imm, 0, 0), exp_pd_q(imm, 0, 1), \
                                  exp_pd_q(imm, 1, 0), exp_pd_q(imm, 1, 1)}; \
    CHECK(memcmp(got, want, 32) == 0, "vex256 vshufpd imm=" #imm); \
    printf("vex256 vshufpd imm=" #imm " done\n"); \
} while (0)
            TEST_VEX256_PD(0);
            TEST_VEX256_PD(1);
            TEST_VEX256_PD(2);
            TEST_VEX256_PD(3);
            TEST_VEX256_PD(5);
            TEST_VEX256_PD(0x0A);
            TEST_VEX256_PD(0x0F);
            TEST_VEX256_PD(0x55);
            TEST_VEX256_PD(0xAA);
#undef TEST_VEX256_PD
        }
        /* ---- VEX.256 VSHUFPS: bit pairs restart per lane ---- */
        {
#define TEST_VEX256_PS(imm) do { \
    unsigned int got[8]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vmovups %2, %%ymm2\n\t" \
                 "vshufps $" #imm ", %%ymm2, %%ymm1, %%ymm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(A), "m"(B) : "%ymm0", "%ymm1", "%ymm2"); \
    unsigned int want[8]; \
    for (unsigned lane = 0; lane < 2; lane++) \
        for (unsigned i = 0; i < 4; i++) want[4 * lane + i] = exp_ps_d(imm, lane, i); \
    CHECK(memcmp(got, want, 32) == 0, "vex256 vshufps per-lane bits imm=" #imm); \
    printf("vex256 vshufps imm=" #imm " done\n"); \
} while (0)
            TEST_VEX256_PS(0x11);
            TEST_VEX256_PS(0x44);
            TEST_VEX256_PS(0xE4);
            TEST_VEX256_PS(0x1B);
            TEST_VEX256_PS(0xFF);
#undef TEST_VEX256_PS
        }

        /* ---- VPERMILPD variable-form control rule (VEX.256, hardware-
         * verified differential on this AVX2 host): the control bit is BIT 1
         * of each control qword, read per DEST qword, selecting within that
         * 128-bit lane's qword pair.  Bit 0 and bit 2 must do nothing (the
         * old code read bit 0, so controls with only bit 1 set selected the
         * wrong qword). ---- */
        {
            static const unsigned long long P[4] = {0xA0A0A0A0A0A0A0A0ULL,
                                                    0xA1A1A1A1A1A1A1A1ULL,
                                                    0xA2A2A2A2A2A2A2A2ULL,
                                                    0xA3A3A3A3A3A3A3A3ULL};
#define TEST_VPERMILPD_VAR(c0, c1, c2, c3, want0, want1, want2, want3, name) do { \
    unsigned long long got[4]; \
    const unsigned long long ctl[4] = {(c0), (c1), (c2), (c3)}; \
    asm volatile("vmovups %1, %%ymm0\n\t" \
                 "vmovups %2, %%ymm1\n\t" \
                 "vpermilpd %%ymm1, %%ymm0, %%ymm2\n\t" \
                 "vmovups %%ymm2, %0" \
                 : "=m"(got) : "m"(P), "m"(ctl) : "%ymm0", "%ymm1", "%ymm2"); \
    unsigned long long want[4] = {(want0), (want1), (want2), (want3)}; \
    CHECK(memcmp(got, want, 32) == 0, name); \
    printf(name " done\n"); \
} while (0)
            /* bit 1 of q0 set -> dest.q0 = P1, dest.q1 = P0; bit 1 of q2 set
             * -> dest.q2 = P3, dest.q3 = P2. */
            TEST_VPERMILPD_VAR(0x2, 0x0, 0x2, 0x0,
                               P[1], P[0], P[3], P[2],
                               "vpermilpd var bit1 controls");
            /* Bits 0 and 2 set everywhere must NOT change the selection:
             * sel stays 0, so each dest qword keeps its pair's LOW member
             * (dest = [P0, P0, P2, P2] -- the variable form always selects
             * within the dest qword's own pair, unlike the imm form whose
             * dest.q1 selects by its own imm bit; hardware-verified). */
            TEST_VPERMILPD_VAR(0x1, 0x1, 0x1, 0x1,
                               P[0], P[0], P[2], P[2],
                               "vpermilpd var bit0 ignored");
            TEST_VPERMILPD_VAR(0x4, 0x4, 0x4, 0x4,
                               P[0], P[0], P[2], P[2],
                               "vpermilpd var bit2 ignored");
#undef TEST_VPERMILPD_VAR
        }

        /* ---- VPERMILPD imm-form global imm indexing (VEX.256, hardware-
         * verified differential sweep over ALL 16 immediates): dest.q[j]
         * selects within its 128-bit lane's qword pair by imm bit j with j
         * the GLOBAL dest qword number -- the upper lane uses bits 2/3, it
         * does NOT restart at bit 0.  The old emulator restarted per lane,
         * so 12 of 16 immediates diverged from bare metal (imm=$3 gave
         * [a1,a1,a3,a3] instead of [a1,a1,a2,a2]; imm=$0xC gave
         * [a0,a0,a2,a2] instead of [a0,a0,a3,a3]).  VPERMILPS restarts its
         * 2-bit pairs per lane (imm8 holds only four pairs) -- pinned here
         * so the PD fix does not leak into the PS rule the same handler
         * shares. ---- */
        {
            static const unsigned long long V[4] = {0x1111111111111111ULL,
                                                    0x2222222222222222ULL,
                                                    0x3333333333333333ULL,
                                                    0x4444444444444444ULL};
#define TEST_VPERMILPD_IMM256(imm) do { \
    unsigned long long got[4]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vpermilpd $" #imm ", %%ymm1, %%ymm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(V) : "%ymm0", "%ymm1"); \
    unsigned long long want[4]; \
    for (unsigned j = 0; j < 4; j++) \
        want[j] = V[(j & ~1u) + (((imm) >> j) & 1)]; \
    CHECK(memcmp(got, want, 32) == 0, "vpermilpd imm256 global bits imm=" #imm); \
    printf("vpermilpd imm256 imm=" #imm " done\n"); \
} while (0)
            TEST_VPERMILPD_IMM256(0x0);
            TEST_VPERMILPD_IMM256(0x1);
            TEST_VPERMILPD_IMM256(0x2);
            TEST_VPERMILPD_IMM256(0x3);
            TEST_VPERMILPD_IMM256(0x4);
            TEST_VPERMILPD_IMM256(0x5);
            TEST_VPERMILPD_IMM256(0x6);
            TEST_VPERMILPD_IMM256(0x7);
            TEST_VPERMILPD_IMM256(0x8);
            TEST_VPERMILPD_IMM256(0x9);
            TEST_VPERMILPD_IMM256(0xA);
            TEST_VPERMILPD_IMM256(0xB);
            TEST_VPERMILPD_IMM256(0xC);
            TEST_VPERMILPD_IMM256(0xD);
            TEST_VPERMILPD_IMM256(0xE);
            TEST_VPERMILPD_IMM256(0xF);
#undef TEST_VPERMILPD_IMM256
        }
        {
            static const unsigned int W[8] = {0x11111111u, 0x22222222u, 0x33333333u,
                                              0x44444444u, 0x55555555u, 0x66666666u,
                                              0x77777777u, 0x88888888u};
#define TEST_VPERMILPS_IMM256(imm) do { \
    unsigned int got[8]; \
    asm volatile("vmovups %1, %%ymm1\n\t" \
                 "vpermilps $" #imm ", %%ymm1, %%ymm0\n\t" \
                 "vmovups %%ymm0, %0" \
                 : "=m"(got) : "m"(W) : "%ymm0", "%ymm1"); \
    unsigned int want[8]; \
    for (unsigned lane = 0; lane < 2; lane++) \
        for (unsigned i = 0; i < 4; i++) \
            want[4 * lane + i] = W[4 * lane + (((imm) >> (2 * i)) & 3)]; \
    CHECK(memcmp(got, want, 32) == 0, "vpermilps imm256 per-lane pairs imm=" #imm); \
    printf("vpermilps imm256 imm=" #imm " done\n"); \
} while (0)
            TEST_VPERMILPS_IMM256(0x3);
            TEST_VPERMILPS_IMM256(0xC);
            TEST_VPERMILPS_IMM256(0x1B);
            TEST_VPERMILPS_IMM256(0xE4);
            TEST_VPERMILPS_IMM256(0xFF);
#undef TEST_VPERMILPS_IMM256
        }
    } else {
        printf("skip avx groups\n");
    }
    /* ---- end to end: the djb reference Keccak-f[1600] core (the workload
     * that reported the bug).  Same LCG input and same FNV-1a hash as the
     * differential driver: every correct run of any implementation prints
     * fcfa0f4eb454b7d4. ---- */
    {
        typedef unsigned long long u64;
        u64 st[25];
        unsigned char inbuf[200];
        unsigned x = 0x12345678u;
        for (unsigned i = 0; i < 200; i++) {
            x = x * 1664525u + 1013904223u;
            inbuf[i] = (unsigned char)(x >> 24);
        }
        memcpy(st, inbuf, 200);
#define ROL64(a, offset) \
    ((offset != 0) ? ((((u64)a) << offset) ^ (((u64)a) >> (64 - offset))) : a)
#define KIDX(x, y) (((x) % 5) + 5 * ((y) % 5))
        static const unsigned int rho[25] = {0,  1,  62, 28, 27, 36, 44,  6,  55, 20,
                                             3,  10, 43, 25, 39, 41, 45, 15, 21, 8,
                                             18, 2,  61, 56, 14};
        static const u64 rc[24] = {
            0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
            0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
            0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
            0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
            0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
            0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
            0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
            0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};
        for (unsigned r = 0; r < 24; r++) {
            u64 C[5], D[5];
            for (unsigned xx = 0; xx < 5; xx++) {
                C[xx] = 0;
                for (unsigned yy = 0; yy < 5; yy++) C[xx] ^= st[KIDX(xx, yy)];
            }
            for (unsigned xx = 0; xx < 5; xx++)
                D[xx] = ROL64(C[(xx + 1) % 5], 1) ^ C[(xx + 4) % 5];
            for (unsigned xx = 0; xx < 5; xx++)
                for (unsigned yy = 0; yy < 5; yy++) st[KIDX(xx, yy)] ^= D[xx];
            for (unsigned xx = 0; xx < 5; xx++)
                for (unsigned yy = 0; yy < 5; yy++)
                    st[KIDX(xx, yy)] = ROL64(st[KIDX(xx, yy)], rho[KIDX(xx, yy)]);
            u64 tmp[25];
            for (unsigned xx = 0; xx < 5; xx++)
                for (unsigned yy = 0; yy < 5; yy++) tmp[KIDX(xx, yy)] = st[KIDX(xx, yy)];
            for (unsigned xx = 0; xx < 5; xx++)
                for (unsigned yy = 0; yy < 5; yy++)
                    st[KIDX(0 * xx + 1 * yy, 2 * xx + 3 * yy)] = tmp[KIDX(xx, yy)];
            for (unsigned yy = 0; yy < 5; yy++) {
                for (unsigned xx = 0; xx < 5; xx++)
                    C[xx] = st[KIDX(xx, yy)] ^
                            ((~st[KIDX(xx + 1, yy)]) & st[KIDX(xx + 2, yy)]);
                for (unsigned xx = 0; xx < 5; xx++) st[KIDX(xx, yy)] = C[xx];
            }
            st[KIDX(0, 0)] ^= rc[r];
        }
        unsigned long long h = 1469598103934665603ULL;
        const unsigned char *p = (const unsigned char *)st;
        for (unsigned i = 0; i < 200; i++) {
            h ^= p[i];
            h *= 1099511628211ULL;
        }
        CHECK(h == 0xfcfa0f4eb454b7d4ULL, "keccakf1600 reference1600 fnv");
        printf("keccak fnv=%016llx\n", h);
    }
    if (failures == 0) {
        printf("shufpd-ok\n");
        return 0;
    }
    printf("shufpd FAILED: %d\n", failures);
    return 1;
}
