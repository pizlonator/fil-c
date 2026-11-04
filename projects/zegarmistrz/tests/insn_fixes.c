// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// Instruction-semantics regression guest for the zegarmistrz emulator fixes:
//   - BSR (16/32/64) + BSF (the old 32/16-bit BSR subtracted (64 - w*8) twice)
//   - PUSHFQ preserving CF (old mask 0x00fcfffe dropped bit 0) + POPFQ round-trip
//   - VEX VPSLLW/VPSRLW/VPSLLD/VPSRLD/VPSLLQ/VPSRLQ 3-operand forms: the shift
//     applies to op1 (not dst's old value) and is per-lane
//   - VEX VPSLLDQ/VPSRLDQ (ymm, 3-operand: shift op1) per-128-bit-lane
//   - VPERMD/VPERMPS (op1 = index vector, op2 = data; indices mod lane count)
//   - VBROADCASTI128
//   - VPINSRB/W/D/Q + VPEXTRB/W/D/Q (VEX forms: unchanged lanes from op1,
//     inserted value from op2) and VINSERTPS (VEX: kept lanes from op1,
//     inserted element from op2)
// Every group asserts hand-computed, hardware-verified expected values that
// DISTINGUISH the fixed behavior from the old buggy behavior, and is gated on
// runtime CPUID so the guest cleanly skips groups the CPU lacks.  Prints
// "insn-fixes-ok" and exits 0 on success (same output native and emulated).
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, name)                                          \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("FAIL %s\n", (name));                           \
            failures++;                                            \
        }                                                          \
    } while (0)

static unsigned cpuid7_ebx(void) {
    unsigned a, b, c, d;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(7), "c"(0));
    return b;
}

#define CPUID7_AVX2 (1u << 5)

static int eq(const void* a, const void* b, size_t n) {
    return memcmp(a, b, n) == 0;
}

// Global m128 source for the VBROADCASTI128 memory-form checks.
static __m128i g_src128;

int main(void) {
    // ---- BSR (16/32/64) + BSF --------------------------------------------
    // The old code computed 63 - clzll(v) - (64 - w*8), which for 32-bit
    // BSR(0x3f) returned 0xffffffe5 instead of 5.
    {
        unsigned o32, b32;
        unsigned short o16;
        unsigned long long o64;
        asm("bsrl %k1, %k0" : "=r"(o32) : "r"(0x3fu));
        CHECK(o32 == 5, "bsr32(0x3f)==5");
        asm("bsrl %k1, %k0" : "=r"(o32) : "r"(0x80000000u));
        CHECK(o32 == 31, "bsr32(0x80000000)==31");
        asm("bsrw %w1, %w0" : "=r"(o16) : "r"((unsigned short)0x8000u));
        CHECK(o16 == 15, "bsr16(0x8000)==15");
        asm("bsrw %w1, %w0" : "=r"(o16) : "r"((unsigned short)0x3fu));
        CHECK(o16 == 5, "bsr16(0x3f)==5");
        asm("bsrq %1, %0" : "=r"(o64) : "r"(1ULL << 63));
        CHECK(o64 == 63, "bsr64(1<<63)==63");
        asm("bsrq %1, %0" : "=r"(o64) : "r"(0x123456789abcdefULL));
        CHECK(o64 == 56, "bsr64(0x123456789abcdef)==56");
        asm("bsfl %k1, %k0" : "=r"(b32) : "r"(0x40u));
        CHECK(b32 == 6, "bsf32(0x40)==6");
        asm("bsfl %k1, %k0" : "=r"(b32) : "r"(0x3fu));
        CHECK(b32 == 0, "bsf32(0x3f)==0");
        printf("bsr-bsf done\n");
    }

    // ---- PUSHFQ preserves CF + POPFQ round-trip ---------------------------
    {
        unsigned long f1, f0, saved, probe = 0;
        asm volatile("stc; pushfq; popq %0" : "=r"(f1) :: "memory");
        CHECK((f1 & 1) == 1, "pushfq after stc has CF=1");
        asm volatile("clc; pushfq; popq %0" : "=r"(f0) :: "memory");
        CHECK((f0 & 1) == 0, "pushfq after clc has CF=0");
        asm volatile("stc; pushfq; popq %0; clc" : "=r"(saved) :: "memory");
        asm volatile("pushq %1; popfq; adc $0, %0"
                     : "+r"(probe)
                     : "r"(saved)
                     : "memory", "cc");
        CHECK(probe == 1, "popfq round-trip restores CF");
        printf("pushfq-popfq done\n");
    }

    if (cpuid7_ebx() & CPUID7_AVX2) {
        // ---- VEX VPSLLQ/VPSRLQ/VPSLLW/VPSRLW/VPSLLD (3-operand forms) -----
        // The old code shifted dst's OLD value (op0) instead of op1.  Each
        // case puts a value in op1 whose per-lane result differs from what
        // shifting op0's old value (or a whole-register shift) would give.
        {
            unsigned long long r[4];
            unsigned short wr[16];
            unsigned int dr[8];
            __m256i o, a;

            // {1, 1} << 1 per-lane = {2, 2}; a whole-register shift of
            // {1, 1} would also be {2, 2}, but {2^63, 0} << 1 is {0, 0}
            // per-lane and {0, 1} whole-register.
            a = _mm256_setr_epi64x(0x8000000000000000ULL, 0, 1, 1);
            asm("vpsllq $1, %1, %0" : "=&x"(o) : "x"(a));
            _mm256_storeu_si256((__m256i*)r, o);
            CHECK(r[0] == 0 && r[1] == 0 && r[2] == 2 && r[3] == 2,
                  "vpsllq 3op per-lane");
            a = _mm256_setr_epi64x(0, 1, 5, 0);
            asm("vpsrlq $1, %1, %0" : "=&x"(o) : "x"(a));
            _mm256_storeu_si256((__m256i*)r, o);
            CHECK(r[0] == 0 && r[1] == 0 && r[2] == 2 && r[3] == 0,
                  "vpsrlq 3op per-lane");
            // Word granularity: 0x8000 << 1 per-lane = 0x0000 (whole-register
            // would leak the bit into the next element).
            a = _mm256_setr_epi16(0x8000, 0, 0, 0, 0, 0, 0, 0,
                                  0x1234, 0, 0, 0, 0, 0, 0, 0);
            asm("vpsllw $1, %1, %0" : "=&x"(o) : "x"(a));
            _mm256_storeu_si256((__m256i*)wr, o);
            CHECK(wr[0] == 0x0000 && wr[1] == 0 && wr[8] == 0x2468,
                  "vpsllw 3op per-lane");
            a = _mm256_setr_epi16(0, 1, 0, 0, 0, 0, 0, 0,
                                  0, 0, 0x0080, 0, 0, 0, 0, 0);
            asm("vpsrlw $1, %1, %0" : "=&x"(o) : "x"(a));
            _mm256_storeu_si256((__m256i*)wr, o);
            CHECK(wr[0] == 0 && wr[1] == 0 && wr[10] == 0x0040,
                  "vpsrlw 3op per-lane");
            // Dword granularity, immediate count and xmm-count forms.
            a = _mm256_setr_epi32(0xdef0, 0x9abc, 0x5678, 0x1234,
                                  1, 0xffffffffu, 0x40000000u, 0x80000000u);
            asm("vpslld $4, %1, %0" : "=&x"(o) : "x"(a));
            _mm256_storeu_si256((__m256i*)dr, o);
            CHECK(dr[0] == 0xdef00u && dr[1] == 0x9abc0u && dr[2] == 0x56780u &&
                      dr[3] == 0x12340u && dr[4] == 0x10u &&
                      dr[5] == 0xfffffff0u && dr[6] == 0u && dr[7] == 0u,
                  "vpslld 3op imm");
            __m128i cnt = _mm_set_epi32(0, 0, 0, 4);
            asm("vpslld %2, %1, %0" : "=&x"(o) : "x"(a), "x"(cnt));
            _mm256_storeu_si256((__m256i*)dr, o);
            CHECK(dr[0] == 0xdef00u && dr[4] == 0x10u && dr[7] == 0u,
                  "vpslld 3op xmm-count");
            asm("vpsrld $4, %1, %0" : "=&x"(o) : "x"(a));
            _mm256_storeu_si256((__m256i*)dr, o);
            CHECK(dr[0] == 0xdefu && dr[1] == 0x9abu && dr[4] == 0u &&
                      dr[5] == 0x0fffffffu,
                  "vpsrld 3op imm");
            // Count 0 must pass op1 through unchanged (and still from op1,
            // not op0's old value).
            asm("vpslld $0, %1, %0" : "=&x"(o) : "x"(a));
            _mm256_storeu_si256((__m256i*)dr, o);
            CHECK(dr[0] == 0xdef0u && dr[7] == 0x80000000u, "vpslld count 0");
            printf("vex-pshift done\n");
        }

        // ---- VEX VPSLLDQ/VPSRLDQ (ymm): per-128-bit-lane byte shifts -------
        {
            unsigned char br[32];
            __m256i o;
            __m256i bytes = _mm256_setr_epi8(
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32);
            // Left by 3: lane 0 -> 00 00 00 01 .. 0d; lane 1 -> 00 00 00 11 ..
            // 1d.  A whole-register shift would put 0e 0f 10 at bytes 16..18.
            asm("vpslldq $3, %1, %0" : "=&x"(o) : "x"(bytes));
            _mm256_storeu_si256((__m256i*)br, o);
            static const unsigned char el[32] = {
                0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
                0, 0, 0, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29};
            CHECK(eq(br, el, 32), "vpslldq ymm 3op per-lane");
            // Right by 3: lane 0 -> 04..10 00 00 00; lane 1 -> 14..20 00 00 00.
            asm("vpsrldq $3, %1, %0" : "=&x"(o) : "x"(bytes));
            _mm256_storeu_si256((__m256i*)br, o);
            static const unsigned char er[32] = {
                4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0, 0, 0,
                20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 0, 0, 0};
            CHECK(eq(br, er, 32), "vpsrldq ymm 3op per-lane");
            printf("vex-pshiftdq done\n");
        }

        // ---- VPERMD/VPERMPS -------------------------------------------------
        {
            unsigned int dr[8];
            __m256i o;
            __m256i idx = _mm256_setr_epi32(7, 6, 5, 4, 3, 2, 1, 0);
            __m256i data = _mm256_setr_epi32(100, 101, 102, 103, 104, 105, 106, 107);
            // dst[i] = data[idx[i]]: indices from op1, data from op2.
            asm("vpermd %2, %1, %0" : "=&x"(o) : "x"(idx), "x"(data));
            _mm256_storeu_si256((__m256i*)dr, o);
            CHECK(dr[0] == 107 && dr[1] == 106 && dr[2] == 105 && dr[3] == 104 &&
                      dr[4] == 103 && dr[5] == 102 && dr[6] == 101 && dr[7] == 100,
                  "vpermd idx from op1 data from op2");
            // Hardware ignores the index elements' high bits (mod lane count).
            __m256i idx2 = _mm256_setr_epi32(0xfffffff9u, 0x00000008u, 1, 2,
                                             3, 4, 5, 6);
            asm("vpermps %2, %1, %0" : "=&x"(o) : "x"(idx2), "x"(data));
            _mm256_storeu_si256((__m256i*)dr, o);
            CHECK(dr[0] == 101 && dr[1] == 100 && dr[2] == 101 && dr[7] == 106,
                  "vpermps indices mod lane count");
            printf("vpermd-vpermps done\n");
        }

        // ---- VBROADCASTI128 --------------------------------------------------
        {
            unsigned char br[32];
            __m256i o;
            // VBROADCASTI128 takes an m128 source only (there is no xmm->ymm
            // register form in the ISA).  Read both sources from globals:
            // handing the compiler an "m" operand tied to a materialized
            // constant lets it rematerialize the constant in a fresh slot,
            // so the sources are stored explicitly first.
            static const unsigned char e1[16] = {
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
            memcpy((void*)&g_src128, e1, 16);
            asm("vbroadcasti128 %1, %0" : "=&x"(o) : "m"(g_src128));
            _mm256_storeu_si256((__m256i*)br, o);
            CHECK(eq(br, e1, 16) && eq(br + 16, e1, 16),
                  "vbroadcasti128 m128 (globals)");
            printf("vbroadcasti128 done\n");
        }

        // ---- VPINSRB/W/D/Q + VPEXTRB/W/D/Q (VEX forms) ------------------------
        {
            __m128i o, base = _mm_setr_epi8(
                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f);
            unsigned char eb[16];
            unsigned short ew[8];
            unsigned int ed[4];
            unsigned long long eq_[2];
            unsigned v = 0xabu;
            unsigned long long vq = 0x1122334455667788ULL;
            // VPINSRB xmm1, xmm2, r32, imm8: unchanged lanes from op1 (base),
            // inserted byte from op2 (v).  The old code inserted op1's lane
            // and kept dst's old value in the other lanes.
            asm("vpinsrb $3, %2, %1, %0" : "=&x"(o) : "x"(base), "r"(v));
            _mm_storeu_si128((__m128i*)eb, o);
            CHECK(eb[3] == 0xab && eb[0] == 0x10 && eb[2] == 0x12 &&
                      eb[4] == 0x14 && eb[15] == 0x1f,
                  "vpinsrb 3op");
            asm("vpinsrw $7, %2, %1, %0" : "=&x"(o) : "x"(base), "r"(v));
            _mm_storeu_si128((__m128i*)ew, o);
            CHECK(ew[7] == 0x00ab && ew[0] == 0x1110 && ew[6] == 0x1d1c,
                  "vpinsrw 3op");
            asm("vpinsrd $2, %2, %1, %0" : "=&x"(o) : "x"(base), "r"(v));
            _mm_storeu_si128((__m128i*)ed, o);
            CHECK(ed[2] == 0x000000abu && ed[0] == 0x13121110u &&
                      ed[3] == 0x1f1e1d1cu,
                  "vpinsrd 3op");
            asm("vpinsrq $1, %2, %1, %0" : "=&x"(o) : "x"(base), "r"(vq));
            _mm_storeu_si128((__m128i*)eq_, o);
            CHECK(eq_[0] == 0x1716151413121110ULL &&
                      eq_[1] == 0x1122334455667788ULL,
                  "vpinsrq 3op");
            // VPEXTR* r32, xmm, imm8.
            unsigned ex8, exw, exd;
            unsigned long long exq;
            __m128i src = _mm_setr_epi8(
                0, 0, 0, 0xab, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            asm("vpextrb $3, %1, %0" : "=r"(ex8) : "x"(src));
            CHECK(ex8 == 0xab, "vpextrb");
            __m128i srcw = _mm_setr_epi16(0, 0, 0, 0, 0, 0, 0, 0xbeef);
            asm("vpextrw $7, %1, %0" : "=r"(exw) : "x"(srcw));
            CHECK(exw == 0xbeef, "vpextrw");
            __m128i srcd = _mm_set_epi32(0, 0, 0, 0xdeadbeef);
            asm("vpextrd $0, %1, %0" : "=r"(exd) : "x"(srcd));
            CHECK(exd == 0xdeadbeefu, "vpextrd");
            __m128i srcq = _mm_set_epi64x(0x1122334455667788LL, 0);
            asm("vpextrq $1, %1, %0" : "=r"(exq) : "x"(srcq));
            CHECK(exq == 0x1122334455667788ULL, "vpextrq");
            printf("vpinsr-vpextr done\n");
        }

        // ---- VINSERTPS (VEX 4-operand) ----------------------------------------
        {
            unsigned char eb[16];
            __m128i o;
            __m128i keep = _mm_setr_epi8(
                0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
                0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf);
            __m128i ins = _mm_setr_epi8(
                0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
                0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf);
            // imm 0x10: COUNT_S=0 (insert src element 0), COUNT_D=1 (into dst
            // element 1), ZMASK=0.  Kept lanes come from op1; the inserted
            // 32-bit element comes from op2.  The old code kept dst's old
            // value and inserted op1's element.
            asm("vinsertps $0x10, %2, %1, %0" : "=&x"(o) : "x"(keep), "x"(ins));
            _mm_storeu_si128((__m128i*)eb, o);
            CHECK(eb[0] == 0xa0 && eb[1] == 0xa1 && eb[2] == 0xa2 &&
                      eb[3] == 0xa3 && eb[4] == 0xd0 && eb[5] == 0xd1 &&
                      eb[6] == 0xd2 && eb[7] == 0xd3 && eb[8] == 0xa8 &&
                      eb[15] == 0xaf,
                  "vinsertps kept from op1 inserted from op2");
            // imm 0x14: COUNT_D=1, ZMASK=4 (dst element 2 zeroed).
            asm("vinsertps $0x14, %2, %1, %0" : "=&x"(o) : "x"(keep), "x"(ins));
            _mm_storeu_si128((__m128i*)eb, o);
            CHECK(eb[4] == 0xd0 && eb[8] == 0x00 && eb[9] == 0x00 &&
                      eb[11] == 0x00 && eb[12] == 0xac,
                  "vinsertps zmask");
            printf("vinsertps done\n");
        }
    } else {
        printf("skip avx2 groups (no host AVX2)\n");
    }

    if (failures == 0) {
        printf("insn-fixes-ok\n");
        return 0;
    }
    printf("insn-fixes FAILED: %d\n", failures);
    return 1;
}
