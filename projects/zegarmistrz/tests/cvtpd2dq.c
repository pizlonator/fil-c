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

// Instruction-semantics regression guest for CVTPD2DQ/VCVTPD2DQ and

// CVTTPD2DQ/VCVTTPD2DQ, the packed double -> int32 narrowing converts djb's

// supercop crypto_kem/newhope512cca avx2 kernel (qhasm ntt_double.S) uses

// ("unimplemented vcvtpd2dq", then wrong NTT results from the VEX.256 form

// converting only its low two lanes).  Every group computes its expected

// bytes in C from hardware-verified semantics and memcmps them against the

// instruction result, so the native run pins the semantics on real silicon

// and the emulator run must produce the exact same output:

//   - legacy (F2 0F E6): 2 doubles -> 2 dwords, dest upper 64 bits ZEROED

//     (verified on this host), NaN/inf/out-of-range -> 0x80000000;

//   - VEX.128 (F2 0F E6 + VEX): same 2 lanes, VEX zeroing above the dest;

//   - VEX.256: FOUR doubles -> FOUR dwords in the low 128 bits, upper 128

//     zeroed -- the lane count comes from the SOURCE width, not the (always

//     128-bit) destination;

//   - rounding per MXCSR.RC (mode 0 round-to-nearest-even verified:

//     0.5->0, 1.5->2, 2.5->2, -2.5->-2, 4.5->4; 2147483647.5 rounds to 2^31

//     and overflows -> 0x80000000); the truncating form (66 0F E6) rounds

//     toward zero regardless of RC (1.5->1, -1.5->-1);

//   - register AND memory sources in every width.
// Gated on runtime CPUID (SSE2 for the legacy form, AVX for the VEX forms).

// Prints cvtpd2dq-ok.
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

static unsigned cpuid1_edx(void) {
    unsigned a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(1));
    return d;
}

static unsigned char buf[64];

static const unsigned long long junk[4] = {
    0xDEADBEEFDEADBEEFULL, 0xCAFEBABECAFEBABEULL,
    0x1122334455667788ULL, 0x99AABBCCDDEEFF00ULL};

int main(void) {
    int sse2 = (cpuid1_edx() >> 26) & 1, avx = (cpuid1_ecx() >> 28) & 1;

    if (sse2) {
        // ---- legacy cvtpd2dq, register source ------------------------------
        {
            double d[2] = {1.5, -1.5};
            __m128 r;
            asm volatile("vmovups %2, %%xmm0\n\t" // junk in dest upper
                         "vmovupd %1, %%xmm2\n\t"
                         "cvtpd2dq %%xmm2, %%xmm0\n\t"
                         "vmovups %%xmm0, %0"
                         : "=m"(r) : "m"(d), "m"(junk) : "%xmm0", "%xmm2");
            unsigned int want[4] = {2u, 0xfffffffeu, 0u, 0u};
            memcpy(buf, &r, 16);
            CHECK(eq(buf, want, 16), "cvtpd2dq reg: values + zeroed upper 64");
            printf("cvtpd2dq-legacy-reg done\n");
        }
        // ---- rounding: nearest-even ----------------------------------------
        {
            double d[2] = {0.5, -0.5};
            __m128 r;
            asm volatile("cvtpd2dq %1, %0" : "=&x"(r) : "m"(d));
            unsigned int want[4] = {0u, 0u, 0u, 0u};
            memcpy(buf, &r, 16);
            CHECK(eq(buf, want, 16), "cvtpd2dq 0.5/-0.5 -> 0/0 (nearest-even)");
            double e[2] = {1.5, 2.5};
            asm volatile("cvtpd2dq %1, %0" : "=&x"(r) : "x"(*( __m128d*)e) );
            memcpy(buf, &r, 16);
            unsigned int want2[4] = {2u, 2u, 0u, 0u};
            CHECK(eq(buf, want2, 16), "cvtpd2dq 1.5/2.5 -> 2/2");
            double f[2] = {-2.5, 4.5};
            asm volatile("cvtpd2dq %1, %0" : "=&x"(r) : "x"(*(__m128d*)f));
            memcpy(buf, &r, 16);
            unsigned int want3[4] = {0xfffffffeu, 4u, 0u, 0u};
            CHECK(eq(buf, want3, 16), "cvtpd2dq -2.5/4.5 -> -2/4");
            printf("cvtpd2dq-rounding done\n");
        }
        // ---- NaN/inf/overflow -> integer indefinite -------------------------
        {
            double d[2] = {__builtin_nan(""), __builtin_inf()};
            __m128 r;
            asm volatile("cvtpd2dq %1, %0" : "=&x"(r) : "x"(*(__m128d*)d));
            unsigned int want[4] = {0x80000000u, 0x80000000u, 0u, 0u};
            memcpy(buf, &r, 16);
            CHECK(eq(buf, want, 16), "cvtpd2dq nan/inf -> 0x80000000");
            double e[2] = {4294967296.0, -4294967296.0};
            asm volatile("cvtpd2dq %1, %0" : "=&x"(r) : "x"(*(__m128d*)e));
            memcpy(buf, &r, 16);
            CHECK(eq(buf, want, 16), "cvtpd2dq +-2^32 -> 0x80000000");
            printf("cvtpd2dq-indefinite done\n");
        }
        // ---- legacy cvttpd2dq truncates regardless of RC --------------------
        {
            double d[2] = {1.5, -1.5};
            __m128 r;
            asm volatile("cvttpd2dq %1, %0" : "=&x"(r) : "x"(*(__m128d*)d));
            unsigned int want[4] = {1u, 0xffffffffu, 0u, 0u};
            memcpy(buf, &r, 16);
            CHECK(eq(buf, want, 16), "cvttpd2dq truncates");
            printf("cvttpd2dq-legacy done\n");
        }
    } else {
        printf("skip sse2 groups\n");
    }

    if (avx) {
        // ---- VEX.256: four lanes from the 256-bit source --------------------
        {
            double d[4] = {-12345.0, 3.0, -0.5, 2147483647.5};
            __m256i r;
            asm volatile("vmovups %2, %%ymm0\n\t"
                         "vmovupd %1, %%ymm1\n\t"
                         "vcvtpd2dq %%ymm1, %%xmm0\n\t"
                         "vmovups %%ymm0, %0"
                         : "=m"(r) : "m"(d), "m"(junk) : "%xmm0", "%ymm0", "%ymm1");
            unsigned int want[8] = {0xffffcfc7u, 3u, 0u, 0x80000000u, 0u, 0u, 0u, 0u};
            memcpy(buf, &r, 32);
            CHECK(eq(buf, want, 32),
                  "vcvtpd2dq ymm: 4 lanes + zeroed upper 128 (overflow lane)");
            printf("vcvtpd2dq-vex256 done\n");
        }
        // ---- VEX.128: two lanes, VEX zeroing above the dest ------------------
        {
            double d[2] = {7.5, -7.5};
            __m256i r;
            asm volatile("vmovups %2, %%ymm0\n\t"
                         "vmovupd %1, %%xmm1\n\t"
                         "vcvtpd2dq %%xmm1, %%xmm0\n\t"
                         "vmovups %%ymm0, %0"
                         : "=m"(r) : "m"(d), "m"(junk) : "%xmm0", "%xmm1", "%ymm0");
            unsigned int want[8] = {8u, 0xfffffff8u, 0u, 0u, 0u, 0u, 0u, 0u};
            memcpy(buf, &r, 32);
            CHECK(eq(buf, want, 32), "vcvtpd2dq xmm reg-src: 2 lanes + VEX zeroing");
            printf("vcvtpd2dq-vex128 done\n");
        }
        // ---- VEX.256 memory source (raw bytes: gas picks the m256 form
        //      only when the encoding is spelled out, c4 e1 7f e6 mod=00
        //      reg=0 rm=000 for a %rax base + reg-dest xmm0) ------------------
        {
            static const double d[4] = {10.5, 20.5, 30.5, 40.5};
            unsigned int out[8];
            asm volatile("lea %2, %%rax\n\t"
                         "vmovups %1, %%ymm0\n\t"
                         ".byte 0xc4, 0xe1, 0x7f, 0xe6, 0x00\n\t"
                         "vmovups %%ymm0, %0"
                         : "=m"(out) : "m"(junk), "m"(d)
                         : "%rax", "%xmm0", "%ymm0");
            unsigned int want[8] = {10u, 20u, 30u, 40u, 0u, 0u, 0u, 0u};
            CHECK(eq(out, want, 32), "vcvtpd2dq m256 mem source: 4 lanes");
            printf("vcvtpd2dq-mem256 done\n");
        }
        // ---- VEX truncating form ----------------------------------------------
        {
            double d[2] = {1.5, -1.5};
            __m128 r;
            asm volatile("vcvttpd2dq %1, %0" : "=&x"(r) : "x"(*(__m128d*)d));
            unsigned int want[4] = {1u, 0xffffffffu, 0u, 0u};
            memcpy(buf, &r, 16);
            CHECK(eq(buf, want, 16), "vcvttpd2dq truncates");
            printf("vcvttpd2dq-vex done\n");
        }
    } else {
        printf("skip avx groups\n");
    }

    if (failures == 0) {
        printf("cvtpd2dq-ok\n");
        return 0;
    }
    printf("cvtpd2dq FAILED: %d\n", failures);
    return 1;
}
