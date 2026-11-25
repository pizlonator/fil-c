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

// Instruction-semantics regression guest for the VEX half-move family --

// VMOVHLPS/VMOVLHPS (the reg-reg spellings of 0F 12/16) and the VMOVLPS/

// VMOVLPD/VMOVHPS/VMOVHPD m64 forms -- which clang builds into the

// KeccakP1600times4 lane interleave of djb's supercop crypto_kem/

// newhope512cca avx2 kernel ("unimplemented vmovlhps" waiting to happen).

// Every group computes its expected bytes in C from hardware-verified

// semantics and memcmps them against the instruction result, so the native

// run pins the semantics on real silicon and the emulator run must produce

// the exact same output:

//   - VMOVHLPS x1,x2,x3: dst = [x3[127:64], x2[127:64]], upper zeroed;

//   - VMOVLHPS x1,x2,x3: dst = [x2[63:0], x3[63:0]], upper zeroed (this is

//     what clang emits 56x into the workload's Keccak lane shuffles);

//   - VMOVLPS/VMOVLPD x1,x2,m64: dst = [m64, x2[127:64]], upper zeroed;

//   - VMOVHPS/VMOVHPD x1,x2,m64: dst = [x2[63:0], m64], upper zeroed;

//   - the store forms: VMOVHPD m64, xmm stores xmm[127:64]; VMOVLPS m64, xmm

//     stores xmm[63:0];

//   - legacy reg-reg MOVHLPS/MOVLHPS keep the dest's other half AND the

//     upper YMM (no VEX zeroing).
// Gated on runtime CPUID (AVX).  Prints movlhps-ok.
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, name) do { \
    if (!(cond)) { printf("FAIL %s\n", (name)); failures++; } \
} while (0)

static unsigned cpuid1_ecx(void) {
    unsigned a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    return c;
}

static const unsigned long long A[4] = {0xAAAAAAAAAAAAAAAAULL, 0xA1A2A3A4A5A6A7A8ULL,
                                        0xBBBBBBBBBBBBBBBBULL, 0xB1B2B3B4B5B6B7B8ULL};
static const unsigned long long B[4] = {0xCCCCCCCCCCCCCCCCULL, 0xC1C2C3C4C5C6C7C8ULL,
                                        0xDDDDDDDDDDDDDDDDULL, 0xD1D2D3D4D5D6D7D8ULL};
static const unsigned long long E[4] = {0xEEEEEEEEEEEEEEEEULL, 0xE1E2E3E4E5E6E7E8ULL,
                                        0xFFFFFFFFFFFFFFFFULL, 0xF1F2F3F4F5F6F7F8ULL};

int main(void) {
    if (!((cpuid1_ecx() >> 28) & 1)) {
        printf("skip avx groups\n");
        printf("movlhps-ok\n");
        return 0;
    }
    unsigned long long got[4];
    __m256i s1, s2, s3;
    memcpy(&s1, A, 32);
    memcpy(&s2, B, 32);
    memcpy(&s3, E, 32);

    /* VMOVLHPS xmm1, xmm2, xmm3: dst = [s1.low, s2.low], upper zeroed */
    {
        unsigned long long want[4] = {B[0], E[0], 0, 0};
        asm volatile("vmovups %1, %%ymm1\n\t"
                     "vmovups %2, %%ymm2\n\t"
                     "vmovups %3, %%ymm3\n\t"
                     "vmovlhps %%xmm3, %%xmm2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(s1), "m"(s2), "m"(s3) : "%ymm0", "%ymm1", "%ymm2", "%ymm3");
        CHECK(memcmp(got, want, 32) == 0, "vmovlhps x,x,x");
        printf("vmovlhps done\n");
    }
    /* VMOVHLPS xmm1, xmm2, xmm3: dst = [s2.high, s1.high], upper zeroed */
    {
        unsigned long long want[4] = {E[1], B[1], 0, 0};
        asm volatile("vmovups %1, %%ymm1\n\t"
                     "vmovups %2, %%ymm2\n\t"
                     "vmovups %3, %%ymm3\n\t"
                     "vmovhlps %%xmm3, %%xmm2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(s1), "m"(s2), "m"(s3) : "%ymm0", "%ymm1", "%ymm2", "%ymm3");
        CHECK(memcmp(got, want, 32) == 0, "vmovhlps x,x,x");
        printf("vmovhlps done\n");
    }
    /* VMOVLPS xmm1, xmm2, m64: dst = [m64, s1.high], upper zeroed */
    {
        unsigned long long m = 0x1234567890ABCDEFULL;
        unsigned long long want[4] = {m, B[1], 0, 0};
        asm volatile("vmovups %2, %%ymm2\n\t"
                     "vmovlps %1, %%xmm2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(m), "m"(s2) : "%ymm0", "%ymm2");
        CHECK(memcmp(got, want, 32) == 0, "vmovlps x,x,m64");
        printf("vmovlps-mem done\n");
    }
    /* VMOVLPD xmm1, xmm2, m64: dst = [m64, s1.high], upper zeroed */
    {
        unsigned long long m = 0x1234567890ABCDEFULL;
        unsigned long long want[4] = {m, B[1], 0, 0};
        asm volatile("vmovups %2, %%ymm2\n\t"
                     "vmovlpd %1, %%xmm2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(m), "m"(s2) : "%ymm0", "%ymm2");
        CHECK(memcmp(got, want, 32) == 0, "vmovlpd x,x,m64");
        printf("vmovlpd-mem done\n");
    }
    /* VMOVHPS xmm1, xmm2, m64: dst = [s1.low, m64], upper zeroed */
    {
        unsigned long long m = 0x1234567890ABCDEFULL;
        unsigned long long want[4] = {B[0], m, 0, 0};
        asm volatile("vmovups %2, %%ymm2\n\t"
                     "vmovhps %1, %%xmm2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(m), "m"(s2) : "%ymm0", "%ymm2");
        CHECK(memcmp(got, want, 32) == 0, "vmovhps x,x,m64");
        printf("vmovhps-mem done\n");
    }
    /* VMOVHPD xmm1, xmm2, m64: dst = [s1.low, m64], upper zeroed */
    {
        unsigned long long m = 0x1234567890ABCDEFULL;
        unsigned long long want[4] = {B[0], m, 0, 0};
        asm volatile("vmovups %2, %%ymm2\n\t"
                     "vmovhpd %1, %%xmm2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(m), "m"(s2) : "%ymm0", "%ymm2");
        CHECK(memcmp(got, want, 32) == 0, "vmovhpd x,x,m64");
        printf("vmovhpd-mem done\n");
    }
    /* store forms: VMOVHPD m64, xmm -> m64 = xmm.high; VMOVLPS m64, xmm ->
     * m64 = xmm.low */
    {
        unsigned long long m1 = 0, m2 = 0;
        asm volatile("vmovups %2, %%xmm3\n\t"
                     "vmovhpd %%xmm3, %0\n\t"
                     "vmovlps %%xmm3, %1"
                     : "=m"(m1), "=m"(m2) : "m"(s2) : "%xmm3");
        CHECK(m1 == B[1], "vmovhpd store m64 = xmm[127:64]");
        CHECK(m2 == B[0], "vmovlps store m64 = xmm[63:0]");
        printf("store-forms done\n");
    }
    /* legacy reg-reg MOVHLPS/MOVLHPS keep the other half AND the upper YMM */
    {
        unsigned long long want[4] = {A[0], B[0], A[2], A[3]};
        asm volatile("vmovups %1, %%ymm1\n\t"
                     "vmovups %2, %%ymm2\n\t"
                     "movlhps %%xmm2, %%xmm1\n\t"
                     "vmovups %%ymm1, %0"
                     : "=m"(got) : "m"(s1), "m"(s2) : "%ymm1", "%ymm2");
        CHECK(memcmp(got, want, 32) == 0, "legacy movlhps keeps low + upper ymm");
        unsigned long long want2[4] = {B[1], A[1], A[2], A[3]};
        asm volatile("vmovups %1, %%ymm1\n\t"
                     "vmovups %2, %%ymm2\n\t"
                     "movhlps %%xmm2, %%xmm1\n\t"
                     "vmovups %%ymm1, %0"
                     : "=m"(got) : "m"(s1), "m"(s2) : "%ymm1", "%ymm2");
        CHECK(memcmp(got, want2, 32) == 0, "legacy movhlps keeps low + upper ymm");
        printf("legacy-regreg done\n");
    }

    if (failures == 0) {
        printf("movlhps-ok\n");
        return 0;
    }
    printf("movlhps FAILED: %d\n", failures);
    return 1;
}
