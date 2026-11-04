// Copyright (c) 2026 Filip Pizlo. All Rights Reserved.
// EVEX/AVX512 instruction-semantics regression guest for the zegarmistrz
// emulator fixes:
//   - EVEX VPSLLDQ/VPSRLDQ: byte shifts within each 128-bit lane (the old
//     code shifted the whole register as one 64-byte lane, so bytes crossed
//     128-bit lane boundaries)
//   - VSHUFI64X2: dst's low 128-bit lanes come from src1's lanes (imm8[1:0],
//     imm8[3:2]) and the high lanes from src2's lanes (imm8[5:4], imm8[7:6]);
//     the write mask is per qword (the old code used per-256-bit granularity
//     and shuffled with the wrong lane halves)
//   - VPXORQ with an EVEX k1 write mask: qword granularity (the old code
//     masked per dword, so a qword-masked merge corrupted half of each kept
//     qword)
//   - VPMADD52LUQ/VPMADD52HUQ: dst.qword = dst.qword + (low|high) 52 bits of
//     (src2 & 2^52-1) * (src3 & 2^52-1), as a full 64-bit add (hardware-
//     verified; this instruction was not implemented before)
// Every group asserts hand-computed, hardware-verified expected values and is
// gated on runtime CPUID so the guest cleanly skips groups the CPU lacks.
// Prints "insn-fixes-avx512-ok" and exits 0 on success.
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

static void cpuid(unsigned leaf, unsigned* a, unsigned* b, unsigned* c,
                  unsigned* d) {
    asm volatile("cpuid"
                 : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
                 : "a"(leaf), "c"(0));
}

#define CPUID7_AVX512F (1u << 16)
#define CPUID7_AVX512IFMA (1u << 12)
#define CPUID7_AVX512DQ (1u << 17)
#define CPUID7_AVX512VL (1u << 31)

int main(void) {
    unsigned a, b, c, d;
    cpuid(7, &a, &b, &c, &d);

    if (b & CPUID7_AVX512F) {
        // ---- EVEX VPSLLDQ/VPSRLDQ: per-128-bit-lane byte shifts ------------
        {
            unsigned char br[64];
            __m512i o;
            // a[i] = 64 - i (so byte 0 = 0x40 ... byte 63 = 0x01).
            __m512i av = _mm512_set_epi8(
                1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
                33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
                49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64);
            // Left by 3 within each 16-byte lane: bytes 16L..16L+2 = 0 and
            // byte 16L+3 = a[16L].  A whole-register shift would put
            // a[13], a[14], a[15] at bytes 16..18 instead of zeros.
            asm("vpslldq $3, %1, %0" : "=&x"(o) : "x"(av));
            _mm512_storeu_si512(br, o);
            int ok = 1;
            for (int lane = 0; lane < 4 && ok; lane++) {
                for (int i = 0; i < 16; i++) {
                    unsigned char want =
                        (i < 3) ? 0 : (unsigned char)(64 - 16 * lane - (i - 3));
                    if (br[16 * lane + i] != want) {
                        ok = 0;
                        break;
                    }
                }
            }
            CHECK(ok, "evex vpslldq per-128-bit-lane");
            // Right by 3 within each lane: byte 16L+i = a[16L+i+3] for i<13,
            // zeros at 16L+13..15.  Whole-register would pull bytes from the
            // NEXT lane into 16L+13..15.
            asm("vpsrldq $3, %1, %0" : "=&x"(o) : "x"(av));
            _mm512_storeu_si512(br, o);
            ok = 1;
            for (int lane = 0; lane < 4 && ok; lane++) {
                for (int i = 0; i < 16; i++) {
                    unsigned char want =
                        (i < 13) ? (unsigned char)(64 - 16 * lane - (i + 3)) : 0;
                    if (br[16 * lane + i] != want) {
                        ok = 0;
                        break;
                    }
                }
            }
            CHECK(ok, "evex vpsrldq per-128-bit-lane");
            printf("evex-pshiftdq done\n");
        }

        // ---- VPXORQ with EVEX k1 mask (qword granularity) --------------------
        {
            unsigned long long q[8];
            const unsigned long long oldq = 0xBBBBBBBBAAAAAAAAULL;
            const unsigned long long xr = 0x0F0F0F0F0F0F0F0FULL ^ 0x3333333333333333ULL;
            __m512i oldm = _mm512_set1_epi64((long long)oldq);
            __m512i x = _mm512_set1_epi64((long long)0x0F0F0F0F0F0F0F0FULL);
            __m512i y = _mm512_set1_epi64((long long)0x3333333333333333ULL);
            // k = 0x0F: qwords 0..3 from the xor, qwords 4..7 from dst.  The
            // old dword-granularity code would have kept only qwords 2..7.
            _mm512_storeu_si512(q, _mm512_mask_xor_epi64(oldm, 0x0F, x, y));
            CHECK(q[0] == xr && q[3] == xr && q[4] == oldq && q[7] == oldq,
                  "vpxorq k1 qword granularity 0x0F");
            // k = 0xF0: qwords 4..7 from the xor, qwords 0..3 from dst.
            _mm512_storeu_si512(q, _mm512_mask_xor_epi64(oldm, 0xF0, x, y));
            CHECK(q[3] == oldq && q[4] == xr && q[7] == xr && q[0] == oldq,
                  "vpxorq k1 qword granularity 0xF0");
            printf("vpxorq-kmask done\n");
        }
    } else {
        printf("skip avx512f groups (no host AVX512F)\n");
    }

    if ((b & CPUID7_AVX512F) && (b & CPUID7_AVX512DQ) && (b & CPUID7_AVX512VL)) {
        // ---- VSHUFI64X2 (512 and 256) -----------------------------------------
        {
            unsigned long long q[8];
            // Lanes (128-bit): A = {A0,A1,A2,A3} = {0x90,0x92,0xA0,0xA2}
            // (each lane's two qwords differ in the low nibble so the checks
            // catch qword-level mixups), B likewise.
            __m512i av = _mm512_set_epi64(
                0xA3, 0xA2, 0xA1, 0xA0, 0x93, 0x92, 0x91, 0x90);
            __m512i bv = _mm512_set_epi64(
                0x53, 0x52, 0x51, 0x50, 0x43, 0x42, 0x41, 0x40);
            // imm 0x6C = 01 10 11 00: dst lane0 <- A lane0 ({90,91}),
            // lane1 <- A lane3 ({A2,A3}), lane2 <- B lane2 ({50,51}),
            // lane3 <- B lane1 ({42,43}).
            _mm512_storeu_si512(q, _mm512_shuffle_i64x2(av, bv, 0x6C));
            CHECK(q[0] == 0x90 && q[1] == 0x91 && q[2] == 0xA2 && q[3] == 0xA3 &&
                      q[4] == 0x50 && q[5] == 0x51 && q[6] == 0x42 && q[7] == 0x43,
                  "vshufi64x2 low half from src1 high half from src2");
            // Masked (merging) form with k = 0x5: qwords 0,2 from the shuffle,
            // the rest from dst.  The old per-256-bit mask granularity would
            // have kept only qwords 1,3,5,7.
            __m512i oldm = _mm512_set1_epi64(0x1111111111111111LL);
            _mm512_storeu_si512(q, _mm512_mask_shuffle_i64x2(oldm, 0x5, av, bv, 0x6C));
            CHECK(q[0] == 0x90 && q[1] == 0x1111111111111111ULL &&
                      q[2] == 0xA2 && q[3] == 0x1111111111111111ULL &&
                      q[4] == 0x1111111111111111ULL && q[6] == 0x1111111111111111ULL,
                  "vshufi64x2 k-mask qword granularity");
            // 256-bit form: dst lane0 <- A lane imm8[0], lane1 <- B lane imm8[1].
            __m256i a2 = _mm256_set_epi64x(0xABABABABABABABABULL,
                                           0xABABABABABABABABULL,
                                           0xAAAAAAAAAAAAAAAAULL,
                                           0xAAAAAAAAAAAAAAAAULL);
            __m256i b2 = _mm256_set_epi64x(0xBBBBBBBBBBBBBBBBULL,
                                           0xBBBBBBBBBBBBBBBBULL,
                                           0xBABABABABABABABAULL,
                                           0xBABABABABABABABAULL);
            unsigned long long q2[4];
            _mm256_storeu_si256((__m256i*)q2, _mm256_shuffle_i64x2(a2, b2, 0));
            CHECK(q2[0] == 0xAAAAAAAAAAAAAAAAULL && q2[2] == 0xBABABABABABABABAULL,
                  "vshufi64x2 256 imm=0");
            _mm256_storeu_si256((__m256i*)q2, _mm256_shuffle_i64x2(a2, b2, 3));
            CHECK(q2[0] == 0xABABABABABABABABULL && q2[2] == 0xBBBBBBBBBBBBBBBBULL,
                  "vshufi64x2 256 imm=3");
            printf("vshufi64x2 done\n");
        }
    } else {
        printf("skip vshufi64x2 (no host AVX512DQ/VL)\n");
    }

    if ((b & CPUID7_AVX512F) && (b & CPUID7_AVX512IFMA)) {
        // ---- VPMADD52LUQ/VPMADD52HUQ ------------------------------------------
        {
            const uint64_t xv = 0xFFFFFFFFFFFFFULL, yv = 3, accv = 100;
            unsigned long long q[8];
            __m512i acc = _mm512_set1_epi64((long long)accv);
            __m512i X = _mm512_set1_epi64((long long)xv);
            __m512i Y = _mm512_set1_epi64((long long)yv);
            // LUQ: dst = acc + prod[51:0] = 100 + 0xffffffffffffd
            //     = 0x10000000000061 (a full 64-bit add: the carry out of bit
            // 51 lands in bit 52, which is what real hardware does).
            _mm512_storeu_si512(q, _mm512_madd52lo_epu64(acc, X, Y));
            CHECK(q[0] == 0x10000000000061ULL, "vpmadd52luq low 52 bits");
            // HUQ: dst = acc + prod[103:52] = 100 + 2 = 102.
            _mm512_storeu_si512(q, _mm512_madd52hi_epu64(acc, X, Y));
            CHECK(q[0] == 102, "vpmadd52huq high 52 bits");
            // The accumulator's bits 63:52 participate in the add (hardware:
            // 0xABC0000000000064 + 0xffffffffffffd = 0xABD0000000000061).
            __m512i acc2 = _mm512_set1_epi64((long long)0xABC0000000000064ULL);
            _mm512_storeu_si512(q, _mm512_madd52lo_epu64(acc2, X, Y));
            CHECK(q[0] == 0xABD0000000000061ULL, "vpmadd52luq full 64-bit add");
            // Masked (merging) form, k = 0x33: masked qwords accumulate into
            // dst's old value, unmasked qwords keep it.
            __m512i mold = _mm512_set1_epi64(0x2222222222222222LL);
            _mm512_storeu_si512(q, _mm512_mask_madd52lo_epu64(mold, 0x33, X, Y));
            CHECK(q[0] == 0x223222222222221FULL &&   // old + prod[51:0]
                      q[2] == 0x2222222222222222ULL,  // unmasked: old
                  "vpmadd52luq k-mask");
            _mm512_storeu_si512(q, _mm512_mask_madd52hi_epu64(mold, 0x33, X, Y));
            CHECK(q[0] == 0x2222222222222224ULL &&   // old + prod[103:52] (=2)
                      q[2] == 0x2222222222222222ULL,
                  "vpmadd52huq k-mask");
            printf("vpmadd52 done\n");
        }
    } else {
        printf("skip vpmadd52 (no host AVX512IFMA)\n");
    }

    if (failures == 0) {
        printf("insn-fixes-avx512-ok\n");
        return 0;
    }
    printf("insn-fixes-avx512 FAILED: %d\n", failures);
    return 1;
}
