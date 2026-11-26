// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Form-matrix guest for djb's opcode report (more-djb-issues.txt): every
// legacy/VEX encoding and reg/mem operand spelling of the opcodes on his
// list -- vpmulhrsw vmovddup vpsllvq vpsrlvq vpsrlvd vpsllvd vlddqu vmovlpd
// vmovhpd vcvtdq2pd -- plus the pextr/pinsr/movd forms that errored on his
// workload ("bad movd operands", "bad pextr form").  The previous batch
// implemented those mnemonics; this guest sweeps their whole FORM matrix so
// a residual bad form (not opcode) still gets caught: the mem-operand
// spellings gcc emits for unaligned/relocated data, the m64 store forms of
// the 0F 12/16 family, the legacy (non-VEX) load/store encodings, GPR and
// memory destinations for pextr, and the GPR/memory sources for pinsr.
// Lane-placement-sensitive groups run through a ymm0 scratch: preload a
// known 32-byte pattern, execute the one instruction against %xmm0, store
// the full ymm back out, and compare all 32 bytes -- this catches BOTH the
// within-xmm lane rules (which half is overwritten, which kept) and the
// upper-YMM rules (legacy forms keep it, VEX forms zero it).  Every group
// computes its expected bytes in C from the ISA formula and memcmps them,
// so the native run validates the formulas against real silicon; the
// emulator run must then produce byte-identical output.  Groups gate on
// runtime CPUID.  Prints insn-forms-ok.
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
// The ymm0 scratch pattern (P2/P3 double as the canary lanes: a legacy form
// that wrongly zeroes the upper YMM, or a VEX form that wrongly keeps it,
// shows up as a mismatch on bytes 16..31) and the low-128 slice of the same
// pattern for preloading a known xmm via the "x" constraint (a 16-byte
// operand type pins the register class to xmm; passing the 32-byte pattern
// would give a ymm register and break the xmm instructions).

static const unsigned long long pat[4] = {
    0x1111111111111111ULL, 0x2222222222222222ULL,
    0x3333333333333333ULL, 0x4444444444444444ULL
};

static unsigned long long got[4];

static __m128i pat128;
// An unaligned source for the unaligned-load forms (lddqu exists FOR
// unaligned loads; movlps/movhps/movlpd/movhpd take m64 that gcc emits from
// arbitrary struct offsets).

static unsigned char ua[64] __attribute__((aligned(64)));

int main(void) {
    unsigned ecx1 = cpuid1_ecx();
    int sse3 = (ecx1 >> 1) & 1, ssse3 = (ecx1 >> 9) & 1;
    int sse41 = (ecx1 >> 19) & 1, avx = (ecx1 >> 27) & 1;
    int avx2 = (cpuid7_ebx() >> 5) & 1;
    memcpy(&pat128, pat, 16);
    static const unsigned long long m64a = 0xA5A5A5A5A5A5A5A5ULL;
    // Park one known qword at offset 3: not 8/16-byte aligned, crosses
    // nothing (so native faults would be real, not alignment noise).
    memcpy(ua + 3, &m64a, 8);

    // ---- LDDQU / VLDDQU (SSE3 F2 0F F0; VEX F2 0F F0) ----------------------
    // Architecturally an unaligned 16-byte load, identical to movdqu (the
    // cache-line-crossing hint is not observable here).  The legacy form
    // keeps the upper YMM; the VEX forms zero above the loaded width.
    if (sse3) {
        unsigned long long want[4];
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "lddqu %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[16])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want, ua + 3, 16);
        CHECK(eq(got, want, 32), "lddqu legacy load keeps upper ymm");
        if (avx) {
            asm volatile("vmovups %1, %%ymm0\n\t"
                         "vlddqu %2, %%xmm0\n\t"
                         "vmovups %%ymm0, %0"
                         : "=m"(got) : "m"(pat),
                           "m"(*(const unsigned char (*)[16])(ua + 3)) : "%ymm0");
            memset(want, 0, 32);
            memcpy(want, ua + 3, 16);
            CHECK(eq(got, want, 32), "vlddqu xmm load zeroes upper ymm");
            unsigned long long got2[4];
            asm volatile("vlddqu %1, %0" : "=&x"(*(__m256i*)got2)
                         : "m"(*(const unsigned char (*)[32])(ua + 3)));
            unsigned char expect[32];
            memset(expect, 0, 32);
            memcpy(expect, ua + 3, 32);
            CHECK(eq(got2, expect, 32), "vlddqu ymm load");
        }
        printf("lddqu done\n");
    } else {
        printf("skip sse3 lddqu\n");
    }

    // ---- legacy MOVLPS/MOVLPD/MOVHPS/MOVHPD loads (0F 12/16, 66 0F 12/16) --
    // dst[63:0] <- m64 for the l-forms, dst[127:64] <- m64 for the h-forms;
    // the other half AND the upper YMM stay untouched.
    {
        unsigned long long want[4];
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "movlps %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[8])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want, ua + 3, 8);
        CHECK(eq(got, want, 32), "movlps legacy load low half only");
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "movlpd %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[8])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want, ua + 3, 8);
        CHECK(eq(got, want, 32), "movlpd legacy load low half only");
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "movhps %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[8])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want + 1, ua + 3, 8);
        CHECK(eq(got, want, 32), "movhps legacy load high half only");
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "movhpd %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[8])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want + 1, ua + 3, 8);
        CHECK(eq(got, want, 32), "movhpd legacy load high half only");
        printf("movlp-legacy-load done\n");
    }

    // ---- VEX m64 loads where the dest doubles as the untouched-half source -
    // GAS only accepts the VEX m64 load family in the 3-operand spelling
    // (vmovlps m64, src1, dst), and the encoding always carries a vvvv
    // source: there is no 2-operand VEX form.  With src1 == dst -- the
    // spelling gcc emits for load-low/high-into-existing-register -- the
    // result is [m64, old dst high] (or [old dst low, m64]), and the VEX
    // zeroing rules zero everything above 128 bits (the P2/P3 canaries).
    if (avx) {
        unsigned long long want[4];
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovlps %2, %%xmm0, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "+m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[8])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want, ua + 3, 8);
        want[2] = 0;
        want[3] = 0;
        CHECK(eq(got, want, 32), "vmovlps load keeps dst high, zeroes upper");
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovlpd %2, %%xmm0, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "+m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[8])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want, ua + 3, 8);
        want[2] = 0;
        want[3] = 0;
        CHECK(eq(got, want, 32), "vmovlpd load keeps dst high, zeroes upper");
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovhps %2, %%xmm0, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "+m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[8])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want + 1, ua + 3, 8);
        want[2] = 0;
        want[3] = 0;
        CHECK(eq(got, want, 32), "vmovhps load keeps dst low, zeroes upper");
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovhpd %2, %%xmm0, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "+m"(got) : "m"(pat),
                       "m"(*(const unsigned char (*)[8])(ua + 3)) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy(want + 1, ua + 3, 8);
        want[2] = 0;
        want[3] = 0;
        CHECK(eq(got, want, 32), "vmovhpd load keeps dst low, zeroes upper");
        printf("vmovlp-2op-load done\n");
        // ---- VEX 3-operand m64 loads (xmm1, xmm2, m64) ---------------------
        // VMOVLPS/VMOVLPD x1,x2,m64: [m64, x2.high]; VMOVHPS/VMOVHPD:
        // [x2.low, m64]; dest above 128 zeroed (VEX zeroing).
        __m128i src2;
        unsigned long long src2v[2] = {0x0DEFACED0DEFACEDULL, 0x7654321076543210ULL};
        memcpy(&src2, src2v, 16);
        __m128i o128;
        unsigned long long want128[2];
        asm volatile("vmovlps %2, %1, %0"
                     : "=&x"(o128) : "x"(src2),
                       "m"(*(const unsigned char (*)[8])(ua + 3)));
        memcpy(want128, ua + 3, 8);
        memcpy(want128 + 1, src2v + 1, 8);
        CHECK(eq(&o128, want128, 16), "vmovlps 3-op load");
        asm volatile("vmovlpd %2, %1, %0"
                     : "=&x"(o128) : "x"(src2),
                       "m"(*(const unsigned char (*)[8])(ua + 3)));
        CHECK(eq(&o128, want128, 16), "vmovlpd 3-op load");
        asm volatile("vmovhps %2, %1, %0"
                     : "=&x"(o128) : "x"(src2),
                       "m"(*(const unsigned char (*)[8])(ua + 3)));
        memcpy(want128, src2v, 8);
        memcpy(want128 + 1, ua + 3, 8);
        CHECK(eq(&o128, want128, 16), "vmovhps 3-op load");
        asm volatile("vmovhpd %2, %1, %0"
                     : "=&x"(o128) : "x"(src2),
                       "m"(*(const unsigned char (*)[8])(ua + 3)));
        CHECK(eq(&o128, want128, 16), "vmovhpd 3-op load");
        printf("vmovlp-3op-load done\n");
        // ---- m64 STORE forms, legacy + VEX ---------------------------------
        // MOVLPS/MOVLPD m64, xmm stores xmm[63:0]; MOVHPS/MOVHPD m64, xmm
        // stores xmm[127:64]; the VEX store forms pick the same lanes.  The
        // destination is a single 8-byte slot (a whole-array output
        // constraint would claim a 16-byte clobber and gcc dead-stores the
        // canary word).
        unsigned long long slot0 = 1, slot1 = 2;
        __m128d sv;
        unsigned long long svv[2] = {0xCAFE00CAFE0001ULL, 0xCAFE00CAFE0002ULL};
        memcpy(&sv, svv, 16);
        asm volatile("movlps %1, %0" : "=m"(slot0) : "x"(sv));
        CHECK(slot0 == svv[0] && slot1 == 2, "movlps legacy store low");
        asm volatile("movlpd %1, %0" : "=m"(slot0) : "x"(sv));
        CHECK(slot0 == svv[0] && slot1 == 2, "movlpd legacy store low");
        asm volatile("movhps %1, %0" : "=m"(slot0) : "x"(sv));
        CHECK(slot0 == svv[1] && slot1 == 2, "movhps legacy store high");
        asm volatile("movhpd %1, %0" : "=m"(slot0) : "x"(sv));
        CHECK(slot0 == svv[1] && slot1 == 2, "movhpd legacy store high");
        asm volatile("vmovlps %1, %0" : "=m"(slot0) : "x"(sv));
        CHECK(slot0 == svv[0] && slot1 == 2, "vmovlps store low");
        asm volatile("vmovlpd %1, %0" : "=m"(slot0) : "x"(sv));
        CHECK(slot0 == svv[0] && slot1 == 2, "vmovlpd store low");
        asm volatile("vmovhps %1, %0" : "=m"(slot0) : "x"(sv));
        CHECK(slot0 == svv[1] && slot1 == 2, "vmovhps store high");
        asm volatile("vmovhpd %1, %0" : "=m"(slot0) : "x"(sv));
        CHECK(slot0 == svv[1] && slot1 == 2, "vmovhpd store high");
        printf("movlp-store done\n");
    } else {
        printf("skip avx movlp groups\n");
    }

    // ---- PMULHRSW legacy (66 0F 38 0B) + VPMULHRSW (VEX) -------------------
    if (ssse3) {
        short a[8] = {3, (short)0x3fff, (short)0x4000, -1,
                      32767, (short)-32768, 12345, 1000};
        short b[8] = {1, 1, 1, 1, 32767, (short)-32768, -6789, -1000};
        __m128i ma, mb;
        memcpy(&ma, a, 16);
        memcpy(&mb, b, 16);
        // legacy reg,reg
        asm volatile("pmulhrsw %1, %0" : "+x"(ma) : "x"(mb));
        for (int i = 0; i < 8; i++) {
            int32_t prod = (int32_t)a[i] * (int32_t)b[i];
            short w2 = (short)(((prod >> 14) + 1) >> 1);
            CHECK(((short*)&ma)[i] == w2, "pmulhrsw legacy reg,reg");
        }
        // legacy reg,mem
        short bm[8];
        memcpy(bm, b, 16);
        memcpy(&ma, a, 16);
        asm volatile("pmulhrsw %1, %0" : "+x"(ma) : "m"(*(const short (*)[8])bm));
        for (int i = 0; i < 8; i++) {
            int32_t prod = (int32_t)a[i] * (int32_t)b[i];
            short w2 = (short)(((prod >> 14) + 1) >> 1);
            CHECK(((short*)&ma)[i] == w2, "pmulhrsw legacy reg,mem");
        }
        printf("pmulhrsw-legacy done\n");
    } else {
        printf("skip ssse3 pmulhrsw\n");
    }
    if (avx) {
        short a[8] = {16384, -16385, 2, -2, 100, -100, 256, 1};
        short b[8] = {1, 1, 16384, -16384, -256, 256, 129, -1};
        __m128i ma, mb, o;
        memcpy(&ma, a, 16);
        memcpy(&mb, b, 16);
        // VEX reg,reg,reg
        asm volatile("vpmulhrsw %2, %1, %0" : "=&x"(o) : "x"(ma), "x"(mb));
        for (int i = 0; i < 8; i++) {
            int32_t prod = (int32_t)a[i] * (int32_t)b[i];
            short w2 = (short)(((prod >> 14) + 1) >> 1);
            CHECK(((short*)&o)[i] == w2, "vpmulhrsw reg,reg,reg");
        }
        // VEX reg,reg,mem
        short bm[8];
        memcpy(bm, b, 16);
        asm volatile("vpmulhrsw %2, %1, %0" : "=&x"(o) : "x"(ma),
                     "m"(*(const short (*)[8])bm));
        for (int i = 0; i < 8; i++) {
            int32_t prod = (int32_t)a[i] * (int32_t)b[i];
            short w2 = (short)(((prod >> 14) + 1) >> 1);
            CHECK(((short*)&o)[i] == w2, "vpmulhrsw reg,reg,mem");
        }
        printf("vpmulhrsw done\n");
    } else {
        printf("skip avx vpmulhrsw\n");
    }
    // ---- MOVDDUP legacy (F2 0F 12) + VMOVDDUP (VEX) ---------------------
    // Even 64-bit lane duplicated into the odd lane; reg AND mem sources
    // for both encodings, xmm and ymm widths.
    if (sse3) {
        unsigned long long sv[4] = {0x1111111111111111ULL, 0x2222222222222222ULL,
                                    0x3333333333333333ULL, 0x4444444444444444ULL};
        __m128d r, a128;
        memcpy(&a128, sv, 16);
        unsigned long long ur[2];
        // legacy reg,reg (F2 0F 12 mod=11)
        asm volatile("movddup %1, %0" : "=x"(r) : "x"(a128));
        memcpy(ur, &r, 16);
        CHECK(ur[0] == sv[0] && ur[1] == sv[0], "movddup legacy reg,reg");
        // legacy mem,reg
        asm volatile("movddup %1, %0" : "=x"(r)
                     : "m"(*(const unsigned long long (*)[2])sv));
        memcpy(ur, &r, 16);
        CHECK(ur[0] == sv[0] && ur[1] == sv[0], "movddup legacy mem");
        printf("movddup-legacy done\n");
    } else {
        printf("skip sse3 movddup\n");
    }
    if (avx) {
        unsigned long long sv[4] = {1ULL, 2ULL, 3ULL, 4ULL};
        __m128d r2, a128;
        memcpy(&a128, sv, 16);
        unsigned long long ur2[2];
        asm volatile("vmovddup %1, %0" : "=x"(r2) : "x"(a128));
        memcpy(ur2, &r2, 16);
        CHECK(ur2[0] == 1 && ur2[1] == 1, "vmovddup xmm reg,reg");
        asm volatile("vmovddup %1, %0" : "=x"(r2)
                     : "m"(*(const unsigned long long (*)[2])sv));
        memcpy(ur2, &r2, 16);
        CHECK(ur2[0] == 1 && ur2[1] == 1, "vmovddup xmm mem");
        __m256d r4, a4;
        memcpy(&a4, sv, 32);
        unsigned long long ur4[4];
        asm volatile("vmovddup %1, %0" : "=x"(r4) : "x"(a4));
        memcpy(ur4, &r4, 32);
        CHECK(ur4[0] == 1 && ur4[1] == 1 && ur4[2] == 3 && ur4[3] == 3,
              "vmovddup ymm reg,reg");
        asm volatile("vmovddup %1, %0" : "=x"(r4)
                     : "m"(*(const unsigned long long (*)[4])sv));
        memcpy(ur4, &r4, 32);
        CHECK(ur4[0] == 1 && ur4[1] == 1 && ur4[2] == 3 && ur4[3] == 3,
              "vmovddup ymm mem");
        printf("vmovddup done\n");
    } else {
        printf("skip avx vmovddup\n");
    }

    // ---- CVTDQ2PD legacy (F3 0F E6) + VCVTDQ2PD (VEX) ----------------------
    // Widening int32 -> double; the source is half the dest width.  The
    // legacy form keeps the upper YMM; VEX.256 takes a full xmm source.
    // (SSE2: no gate needed.)
    if (1) {
        int32_t v[4] = {-12345, 3, 7, -8};
        __m128i iv;
        memcpy(&iv, v, 16);
        __m128d r;
        unsigned char rb[16];
        // legacy reg,reg: converts the low two dwords of the xmm source.
        asm volatile("cvtdq2pd %1, %0" : "=&x"(r) : "x"(iv));
        double want[2] = {(double)v[0], (double)v[1]};
        memcpy(rb, &r, 16);
        CHECK(eq(rb, want, 16), "cvtdq2pd legacy reg,reg");
        // legacy mem,reg (m64): converts the two dwords in memory.
        asm volatile("cvtdq2pd %1, %0" : "=&x"(r) : "m"(*(const int32_t (*)[2])v));
        memcpy(rb, &r, 16);
        CHECK(eq(rb, want, 16), "cvtdq2pd legacy mem");
        // legacy keeps the upper YMM (xmm0 scratch, source in xmm1).
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovaps %2, %%xmm1\n\t"
                     "cvtdq2pd %%xmm1, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "x"(iv) : "%ymm0", "%xmm1");
        unsigned long long want4[4];
        memcpy(want4, pat, 32);
        memcpy(want4, want, 16);
        CHECK(eq(got, want4, 32), "cvtdq2pd legacy keeps upper ymm");
        printf("cvtdq2pd-legacy done\n");
    }
    if (avx) {
        int32_t v[4] = {-7, 1, 1000000, -2147483647 - 1};
        int32_t mv[2] = {-7, 1};
        __m128i iv;
        memcpy(&iv, v, 16);
        __m128d d2;
        unsigned char rb[16], rb4[32];
        asm volatile("vcvtdq2pd %1, %0" : "=&x"(d2) : "x"(iv));
        double want2[2] = {-7.0, 1.0};
        memcpy(rb, &d2, 16);
        CHECK(eq(rb, want2, 16), "vcvtdq2pd xmm reg,reg");
        asm volatile("vcvtdq2pd %1, %0" : "=&x"(d2) : "m"(*(const int32_t (*)[2])mv));
        memcpy(rb, &d2, 16);
        CHECK(eq(rb, want2, 16), "vcvtdq2pd xmm mem");
        __m256d d4;
        asm volatile("vcvtdq2pd %1, %0" : "=&x"(d4) : "x"(iv));
        double want4[4] = {-7.0, 1.0, 1000000.0, -2147483648.0};
        memcpy(rb4, &d4, 32);
        CHECK(eq(rb4, want4, 32), "vcvtdq2pd ymm reg,reg");
        // The ymm-dest form's memory source is 16 bytes (m128): the source
        // is HALF the dest width in every encoding of the widening convert
        // (verified on this host: the m-form converts four dwords).
        asm volatile("vcvtdq2pd %1, %0" : "=&x"(d4) : "m"(*(const int32_t (*)[4])v));
        memcpy(rb4, &d4, 32);
        CHECK(eq(rb4, want4, 32), "vcvtdq2pd ymm mem (m128 source)");
        printf("vcvtdq2pd done\n");
    } else {
        printf("skip avx vcvtdq2pd\n");
    }

    // ---- VEX variable shifts: xmm AND ymm widths, reg AND mem count sources
    // Per-lane counts; a count >= the element width yields 0 (or all-ones
    // for the arithmetic right form of a negative lane).  The dword and
    // qword tests use SEPARATE data/count pairs so a lane-width mixup in
    // the operand build shows as a check failure, not a false pass.
    if (avx2) {
        unsigned av8[8] = {0xf0f0f0f0u, 0x80000000u, 1234567u,
                           0xffffffffu, 5u, 6u, 7u, 8u};
        unsigned cv8[8] = {4u, 32u, 33u, 1u, 0u, 3u, 4u, 5u};
        unsigned long long av4[4] = {0xf0f0f0f0f0f0f0f0ULL, 0x8000000000000000ULL,
                                     1234567890123ULL, 0xffffffffffffffffULL};
        unsigned long long cv4[4] = {4u, 64u, 65u, 1u};
        unsigned rl[8], rr[8], ra[8];
        unsigned long long ql[4];
        // ymm dword, reg/reg
        __m256i od, ad, cd;
        memcpy(&ad, av8, 32);
        memcpy(&cd, cv8, 32);
        asm volatile("vpsllvd %2, %1, %0" : "=&x"(od) : "x"(ad), "x"(cd));
        memcpy(rl, &od, 32);
        for (int i = 0; i < 8; i++) {
            CHECK(rl[i] == (cv8[i] >= 32 ? 0 : av8[i] << cv8[i]),
                  "vpsllvd ymm reg");
        }
        asm volatile("vpsrlvd %2, %1, %0" : "=&x"(od) : "x"(ad), "x"(cd));
        memcpy(rr, &od, 32);
        for (int i = 0; i < 8; i++) {
            CHECK(rr[i] == (cv8[i] >= 32 ? 0 : av8[i] >> cv8[i]),
                  "vpsrlvd ymm reg");
        }
        asm volatile("vpsravd %2, %1, %0" : "=&x"(od) : "x"(ad), "x"(cd));
        memcpy(ra, &od, 32);
        for (int i = 0; i < 8; i++) {
            unsigned v = av8[i], cnt = cv8[i];
            unsigned w2 = cnt >= 32 ? ((int32_t)v < 0 ? 0xffffffffu : 0)
                                    : (unsigned)((int32_t)v >> cnt);
            CHECK(ra[i] == w2, "vpsravd ymm reg");
        }
        // ymm dword, mem count
        unsigned cm8[8];
        memcpy(cm8, cv8, 32);
        asm volatile("vpsllvd %2, %1, %0" : "=&x"(od) : "x"(ad),
                     "m"(*(const unsigned (*)[8])cm8));
        memcpy(rl, &od, 32);
        for (int i = 0; i < 8; i++) {
            CHECK(rl[i] == (cv8[i] >= 32 ? 0 : av8[i] << cv8[i]),
                  "vpsllvd ymm mem");
        }
        asm volatile("vpsrlvd %2, %1, %0" : "=&x"(od) : "x"(ad),
                     "m"(*(const unsigned (*)[8])cm8));
        memcpy(rr, &od, 32);
        for (int i = 0; i < 8; i++) {
            CHECK(rr[i] == (cv8[i] >= 32 ? 0 : av8[i] >> cv8[i]),
                  "vpsrlvd ymm mem");
        }
        asm volatile("vpsravd %2, %1, %0" : "=&x"(od) : "x"(ad),
                     "m"(*(const unsigned (*)[8])cm8));
        memcpy(ra, &od, 32);
        for (int i = 0; i < 8; i++) {
            unsigned v = av8[i], cnt = cv8[i];
            unsigned w2 = cnt >= 32 ? ((int32_t)v < 0 ? 0xffffffffu : 0)
                                    : (unsigned)((int32_t)v >> cnt);
            CHECK(ra[i] == w2, "vpsravd ymm mem");
        }
        // ymm qword, reg/reg + mem count (their own data pair)
        __m256i oq, aq, cq;
        memcpy(&aq, av4, 32);
        memcpy(&cq, cv4, 32);
        unsigned long long cm4[4];
        memcpy(cm4, cv4, 32);
        asm volatile("vpsllvq %2, %1, %0" : "=&x"(oq) : "x"(aq), "x"(cq));
        memcpy(ql, &oq, 32);
        for (int i = 0; i < 4; i++) {
            CHECK(ql[i] == (cv4[i] >= 64 ? 0 : av4[i] << cv4[i]),
                  "vpsllvq ymm reg");
        }
        asm volatile("vpsllvq %2, %1, %0" : "=&x"(oq) : "x"(aq),
                     "m"(*(const unsigned long long (*)[4])cm4));
        memcpy(ql, &oq, 32);
        for (int i = 0; i < 4; i++) {
            CHECK(ql[i] == (cv4[i] >= 64 ? 0 : av4[i] << cv4[i]),
                  "vpsllvq ymm mem");
        }
        asm volatile("vpsrlvq %2, %1, %0" : "=&x"(oq) : "x"(aq),
                     "m"(*(const unsigned long long (*)[4])cm4));
        memcpy(ql, &oq, 32);
        for (int i = 0; i < 4; i++) {
            CHECK(ql[i] == (cv4[i] >= 64 ? 0 : av4[i] >> cv4[i]),
                  "vpsrlvq ymm mem");
        }
        // xmm qword, reg/reg + mem count
        __m128i o2, a2, c2;
        memcpy(&a2, av4, 16);
        memcpy(&c2, cv4, 16);
        asm volatile("vpsllvq %2, %1, %0" : "=&x"(o2) : "x"(a2), "x"(c2));
        memcpy(ql, &o2, 16);
        for (int i = 0; i < 2; i++) {
            CHECK(ql[i] == (cv4[i] >= 64 ? 0 : av4[i] << cv4[i]),
                  "vpsllvq xmm reg");
        }
        asm volatile("vpsrlvq %2, %1, %0" : "=&x"(o2) : "x"(a2),
                     "m"(*(const unsigned long long (*)[2])cm4));
        memcpy(ql, &o2, 16);
        for (int i = 0; i < 2; i++) {
            CHECK(ql[i] == (cv4[i] >= 64 ? 0 : av4[i] >> cv4[i]),
                  "vpsrlvq xmm mem");
        }
        // xmm dword, reg/reg + mem count
        memcpy(&a2, av8, 16);
        memcpy(&c2, cv8, 16);
        asm volatile("vpsrlvd %2, %1, %0" : "=&x"(o2) : "x"(a2),
                     "m"(*(const unsigned (*)[4])cm8));
        memcpy(rr, &o2, 16);
        for (int i = 0; i < 4; i++) {
            CHECK(rr[i] == (cv8[i] >= 32 ? 0 : av8[i] >> cv8[i]),
                  "vpsrlvd xmm mem");
        }
        asm volatile("vpsllvd %2, %1, %0" : "=&x"(o2) : "x"(a2), "x"(c2));
        memcpy(rl, &o2, 16);
        for (int i = 0; i < 4; i++) {
            CHECK(rl[i] == (cv8[i] >= 32 ? 0 : av8[i] << cv8[i]),
                  "vpsllvd xmm reg");
        }
        asm volatile("vpsravd %2, %1, %0" : "=&x"(o2) : "x"(a2), "x"(c2));
        memcpy(ra, &o2, 16);
        for (int i = 0; i < 4; i++) {
            unsigned v = av8[i], cnt = cv8[i];
            unsigned w2 = cnt >= 32 ? ((int32_t)v < 0 ? 0xffffffffu : 0)
                                    : (unsigned)((int32_t)v >> cnt);
            CHECK(ra[i] == w2, "vpsravd xmm reg");
        }
        printf("vpslv done\n");
    } else {
        printf("skip avx2 vpslv\n");
    }
    // ---- PEXTRW/B/D/Q: GPR and MEMORY destinations ----------------------
    // legacy PEXTRW (0F C5) is GPR-dest only; SSE4.1 (66 0F 3A 15) adds the
    // m16 dest; PEXTRB/D/Q (66 0F 3A 14/16) take r32/r64 or m8/m32/m64; the
    // VEX forms cover the same matrix.  (These are the forms the old code
    // hit with "bad pextr form".)  The test vector is dword-wise
    // {0xCAFEBABE, 0x12345678, 3, 4}: word[1]=0xCAFE, word[2]=0x5678,
    // word[3]=0x1234, byte[3]=0xCA, byte[5]=0x56, qword[1]=
    // 0x0000000400000003.  Memory dests are SINGLE elements: an array-typed
    // output constraint would claim a bigger clobber and gcc dead-stores
    // the canary words.
    if (sse41) {
        __m128i v = _mm_setr_epi32(0xCAFEBABEu, 0x12345678u, 3, 4);
        // pextrw -> GPR (legacy 0F C5)
        unsigned r32 = 0xDEADBEEFu;
        asm volatile("pextrw $1, %1, %0" : "=r"(r32) : "x"(v));
        CHECK((r32 & 0xffffu) == 0xCAFEu, "pextrw legacy reg dest");
        // pextrw -> m16 (single 16-bit slot)
        unsigned short wslot = 9;
        asm volatile("pextrw $2, %1, %0" : "=m"(wslot) : "x"(v));
        CHECK(wslot == 0x5678u, "pextrw mem dest");
        // pextrb -> GPR and -> m8 (single 8-bit slot)
        asm volatile("pextrb $5, %1, %0" : "=r"(r32) : "x"(v));
        CHECK((r32 & 0xffu) == 0x56u, "pextrb reg dest");
        unsigned char bslot = 9;
        asm volatile("pextrb $3, %1, %0" : "=m"(bslot) : "x"(v));
        CHECK(bslot == 0xCAu, "pextrb mem dest");
        // pextrd -> GPR and -> m32 (single 32-bit slot)
        asm volatile("pextrd $1, %1, %0" : "=r"(r32) : "x"(v));
        CHECK(r32 == 0x12345678u, "pextrd reg dest");
        unsigned dslot = 1;
        asm volatile("pextrd $0, %1, %0" : "=m"(dslot) : "x"(v));
        CHECK(dslot == 0xCAFEBABEu, "pextrd mem dest");
        // pextrq -> GPR and -> m64 (single 64-bit slot)
        unsigned long long r64 = 0xDEADBEEFCAFEBABEULL;
        asm volatile("pextrq $1, %1, %0" : "=r"(r64) : "x"(v));
        CHECK(r64 == 0x0000000400000003ULL, "pextrq reg dest");
        unsigned long long qslot = 7;
        asm volatile("pextrq $0, %1, %0" : "=m"(qslot) : "x"(v));
        CHECK(qslot == 0x12345678CAFEBABEULL, "pextrq mem dest");
        printf("pextr done\n");
    } else {
        printf("skip sse41 pextr\n");
    }
    if (avx) {
        __m128i v = _mm_setr_epi32(0xCAFEBABEu, 0x12345678u, 3, 4);
        unsigned r32 = 0xDEADBEEFu;
        unsigned long long r64 = 0xDEADBEEFCAFEBABEULL;
        // VPEXTRW -> GPR
        asm volatile("vpextrw $3, %1, %0" : "=r"(r32) : "x"(v));
        CHECK((r32 & 0xffffu) == 0x1234u, "vpextrw reg dest");
        // VPEXTRW -> m16
        unsigned short wslot = 9;
        asm volatile("vpextrw $1, %1, %0" : "=m"(wslot) : "x"(v));
        CHECK(wslot == 0xCAFEu, "vpextrw mem dest");
        // VPEXTRB -> GPR and -> m8
        asm volatile("vpextrb $5, %1, %0" : "=r"(r32) : "x"(v));
        CHECK((r32 & 0xffu) == 0x56u, "vpextrb reg dest");
        unsigned char bslot = 9;
        asm volatile("vpextrb $4, %1, %0" : "=m"(bslot) : "x"(v));
        CHECK(bslot == 0x78u, "vpextrb mem dest");
        // VPEXTRD -> GPR and -> m32
        asm volatile("vpextrd $2, %1, %0" : "=r"(r32) : "x"(v));
        CHECK(r32 == 3, "vpextrd reg dest");
        unsigned dslot = 1;
        asm volatile("vpextrd $3, %1, %0" : "=m"(dslot) : "x"(v));
        CHECK(dslot == 4, "vpextrd mem dest");
        // VPEXTRQ -> GPR and -> m64
        asm volatile("vpextrq $0, %1, %0" : "=r"(r64) : "x"(v));
        CHECK(r64 == 0x12345678CAFEBABEULL, "vpextrq reg dest");
        unsigned long long qslot = 7;
        asm volatile("vpextrq $1, %1, %0" : "=m"(qslot) : "x"(v));
        CHECK(qslot == 0x0000000400000003ULL, "vpextrq mem dest");
        printf("vpextr done\n");
    } else {
        printf("skip avx vpextr\n");
    }

    // ---- PINSRW/B/D/Q: GPR and MEMORY sources ------------------------------
    // legacy PINSRW (0F C4) takes r32 or m16; PINSRB/D/Q (66 0F 3A 20/22)
    // take r32/r64 or m8/m32/m64.  The legacy forms merge into the DEST's
    // existing lanes and keep the upper YMM (the scratch pattern's P2/P3
    // canaries); the VEX forms take the unchanged lanes from the second
    // source and zero above 128 bits.
    {
        unsigned short wslot = 0xABCD;
        unsigned wreg = wslot; // pinsrw takes r32 in 64-bit mode
        unsigned long long want[4];
        // pinsrw <- r32 (0F C4)
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "pinsrw $5, %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "r"(wreg) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy((unsigned char*)want + 10, &wslot, 2);
        CHECK(eq(got, want, 32), "pinsrw legacy r32 keeps rest + upper ymm");
        // pinsrw <- m16
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "pinsrw $7, %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat),
                       "m"(*(const unsigned short (*)[1])&wslot) : "%ymm0");
        memcpy(want, pat, 32);
        memcpy((unsigned char*)want + 14, &wslot, 2);
        CHECK(eq(got, want, 32), "pinsrw legacy m16 keeps rest + upper ymm");
        printf("pinsrw-legacy done\n");
    }
    if (sse41) {
        // pinsrb/d/q legacy (merge form): the untouched lanes keep the old
        // dest, and the upper YMM is untouched.
        __m128i acc = _mm_setr_epi32(0x11111111u, 0x22222222u, 3, 4);
        unsigned v32 = 0x99u;
        unsigned char m8v = 0x77u;
        unsigned long long v64 = 0x6666666666666666ULL;
        unsigned long long m64v = 0x5555555555555555ULL;
        asm volatile("pinsrb $3, %1, %0" : "+x"(acc) : "r"(v32));
        CHECK(((unsigned char*)&acc)[3] == 0x99 &&
              ((unsigned char*)&acc)[0] == 0x11, "pinsrb legacy r32 merge");
        acc = _mm_setr_epi32(0x11111111u, 0x22222222u, 3, 4);
        asm volatile("pinsrb $9, %1, %0" : "+x"(acc)
                     : "m"(*(const unsigned char (*)[1])&m8v));
        CHECK(((unsigned char*)&acc)[9] == 0x77, "pinsrb legacy m8 merge");
        acc = _mm_setr_epi32(0x11111111u, 0x22222222u, 3, 4);
        asm volatile("pinsrd $2, %1, %0" : "+x"(acc) : "r"(v32));
        CHECK(((unsigned char*)&acc)[8] == 0x99 &&
              ((unsigned char*)&acc)[0] == 0x11, "pinsrd legacy r32 merge");
        unsigned m32v = 0x88u;
        acc = _mm_setr_epi32(0x11111111u, 0x22222222u, 3, 4);
        asm volatile("pinsrd $1, %1, %0" : "+x"(acc)
                     : "m"(*(const unsigned (*)[1])&m32v));
        CHECK(((unsigned*)&acc)[1] == 0x88u, "pinsrd legacy m32 merge");
        acc = _mm_setr_epi32(0x11111111u, 0x22222222u, 3, 4);
        asm volatile("pinsrq $1, %1, %0" : "+x"(acc) : "r"(v64));
        CHECK(((unsigned long long*)&acc)[1] == 0x6666666666666666ULL &&
              ((unsigned long long*)&acc)[0] == 0x2222222211111111ULL,
              "pinsrq legacy r64 merge");
        acc = _mm_setr_epi32(0x11111111u, 0x22222222u, 3, 4);
        asm volatile("pinsrq $0, %1, %0" : "+x"(acc)
                     : "m"(*(const unsigned long long (*)[1])&m64v));
        CHECK(((unsigned long long*)&acc)[0] == 0x5555555555555555ULL,
              "pinsrq legacy m64 merge");
        printf("pinsrbdq-legacy done\n");
    } else {
        printf("skip sse41 pinsrbdq\n");
    }
    if (avx) {
        // VPINSR* (VEX 3-operand): unchanged lanes come from the SECOND
        // source; the dest above 128 bits is zeroed (checked through the
        // ymm0 scratch for vpinsrw, structurally for the rest).
        unsigned short wslot = 0xABCD;
        unsigned wreg = wslot; // vpinsrw takes r32 in 64-bit mode
        unsigned long long want[4];
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovaps %3, %%xmm1\n\t"
                     "vpinsrw $5, %2, %%xmm1, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "r"(wreg), "x"(pat128)
                     : "%ymm0", "%xmm1");
        memset(want, 0, 32);
        memcpy(want, &pat128, 16);
        memcpy((unsigned char*)want + 10, &wslot, 2);
        CHECK(eq(got, want, 32), "vpinsrw VEX r32 zeroes upper ymm");
        __m128i base = _mm_setr_epi32(0x11111111u, 0x22222222u, 3, 4);
        __m128i o;
        unsigned v32 = 0x99u;
        unsigned char m8v = 0x77u;
        unsigned long long v64 = 0x6666666666666666ULL;
        unsigned long long m64v = 0x5555555555555555ULL;
        unsigned m32v = 0x88u;
        asm volatile("vpinsrb $3, %2, %1, %0" : "=&x"(o) : "x"(base), "r"(v32));
        CHECK(((unsigned char*)&o)[3] == 0x99 &&
              ((unsigned char*)&o)[0] == 0x11, "vpinsrb reg src");
        asm volatile("vpinsrb $9, %2, %1, %0" : "=&x"(o) : "x"(base),
                     "m"(*(const unsigned char (*)[1])&m8v));
        CHECK(((unsigned char*)&o)[9] == 0x77, "vpinsrb mem src");
        asm volatile("vpinsrd $2, %2, %1, %0" : "=&x"(o) : "x"(base), "r"(v32));
        CHECK(((unsigned char*)&o)[8] == 0x99, "vpinsrd reg src");
        asm volatile("vpinsrd $1, %2, %1, %0" : "=&x"(o) : "x"(base),
                     "m"(*(const unsigned (*)[1])&m32v));
        CHECK(((unsigned*)&o)[1] == 0x88u, "vpinsrd mem src");
        asm volatile("vpinsrq $1, %2, %1, %0" : "=&x"(o) : "x"(base), "r"(v64));
        CHECK(((unsigned long long*)&o)[1] == 0x6666666666666666ULL,
              "vpinsrq reg src");
        asm volatile("vpinsrq $0, %2, %1, %0" : "=&x"(o) : "x"(base),
                     "m"(*(const unsigned long long (*)[1])&m64v));
        CHECK(((unsigned long long*)&o)[0] == 0x5555555555555555ULL,
              "vpinsrq mem src");
        printf("vpinsr done\n");
    } else {
        printf("skip avx vpinsr\n");
    }

    // ---- MOVD/MOVQ GPR<->xmm, all four directions, legacy + VEX ------------
    // Legacy zeroes the dest XMM above the moved element and keeps the upper
    // YMM; the VEX forms zero the upper YMM too.  Memory operands exist in
    // both directions (m32 for MOVD, m64 for MOVQ).  (These are the forms
    // the old code hit with "bad movd operands".)  (SSE2: no gate needed.)
    if (1) {
        unsigned v32 = 0x13579BDFu;
        unsigned long long v64 = 0x2468ACE02468ACE0ULL;
        unsigned long long want[4];
        // legacy movd r32 <- xmm (66 0F 7E mod=11, GPR dest)
        unsigned gr32 = 0;
        asm volatile("movd %1, %0" : "=r"(gr32) : "x"(pat128));
        CHECK(gr32 == 0x11111111u, "movd legacy r32<-xmm");
        // legacy movq r64 <- xmm (66 REX.W 0F 7E)
        unsigned long long gr64 = 0;
        asm volatile("movq %1, %0" : "=r"(gr64) : "x"(pat128));
        CHECK(gr64 == pat[0], "movq legacy r64<-xmm");
        // legacy movd xmm <- r32: zeroes bytes 4..15, keeps the upper ymm
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "movd %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "r"(v32) : "%ymm0");
        memcpy(want, pat, 32);
        ((unsigned*)want)[0] = v32;
        ((unsigned*)want)[1] = 0;
        ((unsigned*)want)[2] = 0;
        ((unsigned*)want)[3] = 0;
        CHECK(eq(got, want, 32), "movd legacy xmm<-r32 zeroes rest of xmm");
        // legacy movq xmm <- r64
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "movq %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "r"(v64) : "%ymm0");
        memcpy(want, pat, 32);
        want[0] = v64;
        want[1] = 0;
        CHECK(eq(got, want, 32), "movq legacy xmm<-r64 zeroes rest of xmm");
        // legacy movd xmm <- m32
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "movd %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat),
                       "m"(*(const unsigned (*)[1])&v32) : "%ymm0");
        memcpy(want, pat, 32);
        ((unsigned*)want)[0] = v32;
        ((unsigned*)want)[1] = 0;
        ((unsigned*)want)[2] = 0;
        ((unsigned*)want)[3] = 0;
        CHECK(eq(got, want, 32), "movd legacy xmm<-m32 zeroes rest of xmm");
        // legacy movq xmm <- m64
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "movq %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat),
                       "m"(*(const unsigned long long (*)[1])&v64) : "%ymm0");
        memcpy(want, pat, 32);
        want[0] = v64;
        want[1] = 0;
        CHECK(eq(got, want, 32), "movq legacy xmm<-m64 zeroes rest of xmm");
        // legacy stores: m32 <- xmm, m64 <- xmm (single-slot dests)
        unsigned m32slot = 1;
        unsigned long long m64slot = 1;
        __m128i xv;
        unsigned long long xvv[2] = {0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL};
        memcpy(&xv, xvv, 16);
        asm volatile("movd %1, %0" : "=m"(m32slot) : "x"(xv));
        CHECK(m32slot == 0x23456789u, "movd legacy m32<-xmm");
        asm volatile("movq %1, %0" : "=m"(m64slot) : "x"(xv));
        CHECK(m64slot == 0xABCDEF0123456789ULL, "movq legacy m64<-xmm");
        printf("movdq-legacy done\n");
    }
    if (avx) {
        unsigned v32 = 0x13579BDFu;
        unsigned long long v64 = 0x2468ACE02468ACE0ULL;
        unsigned long long want[4];
        // VEX vmovd xmm <- r32: zeroes bytes 4..15 AND the upper ymm.
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovd %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "r"(v32) : "%ymm0");
        memset(want, 0, 32);
        ((unsigned*)want)[0] = v32;
        CHECK(eq(got, want, 32), "vmovd VEX xmm<-r32 zeroes upper ymm");
        // VEX vmovq xmm <- r64
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovq %2, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "r"(v64) : "%ymm0");
        memset(want, 0, 32);
        want[0] = v64;
        CHECK(eq(got, want, 32), "vmovq VEX xmm<-r64 zeroes upper ymm");
        // VEX memory forms: m32 and m64 in both directions.
        unsigned rt32 = 0x13579BDFu;
        asm volatile("vmovd %1, %%xmm0\n\t"
                     "vmovd %%xmm0, %0"
                     : "=m"(*(unsigned (*)[1])&rt32)
                     : "m"(*(const unsigned (*)[1])&v32) : "%xmm0");
        CHECK(rt32 == 0x13579BDFu, "vmovd m32 roundtrip");
        unsigned long long rt64 = 0;
        asm volatile("vmovq %1, %%xmm0\n\t"
                     "vmovq %%xmm0, %0"
                     : "=m"(*(unsigned long long (*)[1])&rt64)
                     : "m"(*(const unsigned long long (*)[1])&v64) : "%xmm0");
        CHECK(rt64 == 0x2468ACE02468ACE0ULL, "vmovq m64 roundtrip");
        // VEX GPR reads: r32 <- xmm (vmovd), r64 <- xmm (vmovq).
        unsigned gr32 = 0;
        unsigned long long gr64 = 0;
        __m128i xv;
        unsigned long long xvv[2] = {0xABCDEF0123456789ULL, 0x9876543210FEDCBAULL};
        memcpy(&xv, xvv, 16);
        asm volatile("vmovd %1, %0" : "=r"(gr32) : "x"(xv));
        CHECK(gr32 == 0x23456789u, "vmovd r32<-xmm");
        asm volatile("vmovq %1, %0" : "=r"(gr64) : "x"(xv));
        CHECK(gr64 == 0xABCDEF0123456789ULL, "vmovq r64<-xmm");
        printf("vmovdq done\n");
    } else {
        printf("skip avx vmovdq\n");
    }

    // ---- MOVQ xmm1, xmm2 reg-reg: both spellings, legacy + VEX -------------
    // Load form (F3 0F 7E mod=11, what gas emits for movq %xmm,%xmm) and
    // store form (66 0F D6 mod=11, raw bytes); both zero the dest's upper 64
    // bits and keep the upper YMM; the VEX forms zero the upper YMM too.
    {
        unsigned long long want[4];
        // legacy load form via mnemonic
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovaps %2, %%xmm1\n\t"
                     "movq %%xmm1, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "x"(pat128) : "%ymm0", "%xmm1");
        memcpy(want, pat, 32);
        want[0] = pat[0];
        want[1] = 0;
        CHECK(eq(got, want, 32), "movq reg-reg load form");
        // legacy store form via raw bytes (66 0F D6 /r mod=11: C8 = xmm0<-xmm1)
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovaps %2, %%xmm1\n\t"
                     ".byte 0x66, 0x0f, 0xd6, 0xc8\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "x"(pat128) : "%ymm0", "%xmm1");
        memcpy(want, pat, 32);
        want[0] = pat[0];
        want[1] = 0;
        CHECK(eq(got, want, 32), "movq reg-reg store form");
        printf("movq-regreg-legacy done\n");
    }
    if (avx) {
        unsigned long long want[4];
        // VEX load form via mnemonic
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovaps %2, %%xmm1\n\t"
                     "vmovq %%xmm1, %%xmm0\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "x"(pat128) : "%ymm0", "%xmm1");
        memset(want, 0, 32);
        want[0] = pat[0];
        CHECK(eq(got, want, 32), "vmovq reg-reg load form zeroes upper ymm");
        // VEX store form via raw bytes (VEX.128.66.0F.D6: C5 F9 D6 C8)
        asm volatile("vmovups %1, %%ymm0\n\t"
                     "vmovaps %2, %%xmm1\n\t"
                     ".byte 0xc5, 0xf9, 0xd6, 0xc8\n\t"
                     "vmovups %%ymm0, %0"
                     : "=m"(got) : "m"(pat), "x"(pat128) : "%ymm0", "%xmm1");
        memset(want, 0, 32);
        want[0] = pat[0];
        CHECK(eq(got, want, 32), "vmovq reg-reg store form zeroes upper ymm");
        printf("movq-regreg-vex done\n");
    } else {
        printf("skip avx movq-regreg-vex\n");
    }

    if (failures == 0) {
        printf("insn-forms-ok\n");
        return 0;
    }
    printf("insn-forms FAILED: %d\n", failures);
    return 1;
}
