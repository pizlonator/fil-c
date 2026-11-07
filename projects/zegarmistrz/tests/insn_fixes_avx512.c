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
//   - VPSRLVQ/VPSLLVQ: variable shift counts are not masked -- a count >= 64
//     yields 0 (the old code masked to 6 bits, making count 64 a shift by 0)
//   - VPERMQ (EVEX.512, imm8): the four 2-bit selects apply within each
//     256-bit half (the old code indexed the immediate by the global qword
//     number, corrupting the upper half on zmm)
//   - VEX.128 VMOVD/VMOVQ (m64/r64 -> xmm): every bit of the containing YMM
//     above the loaded element is cleared (the old exec_movd cleared only
//     the 128-bit XMM image, leaving stale upper-YMM lanes that corrupted
//     openssl's poly1305 IFMA tag)
//   - EVEX immediate shifts (VPSRLQ/VPSLLQ/VPSRAQ/D/W imm8): the imm8 count
//     is NOT masked -- a count >= the element width zeroes the lane (the
//     arithmetic forms sign-fill); the old code masked the qword count to 6
//     bits, so vpsrlq $64 became a shift by 0
//   - VPROLD/VPRORD/VPROLQ/VPRORQ (AVX512F imm8 rotates) in both directions
//     and widths
// Every group asserts hand-computed, hardware-verified expected values and is
// gated on runtime CPUID so the guest cleanly skips groups the CPU lacks.
// Groups whose inputs gcc would constant-fold (the variable shifts, VPERMQ,
// the rotate intrinsics) feed the instruction from volatile memory and/or use
// inline asm, so the regression checks actually execute (and gcc's
// ror->rol rewrite cannot hide the ror forms).
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

    // ---- VPSRLVQ/VPSLLVQ: variable shift counts are NOT masked ----------
    // Intel VPSRLVQ/VPSLLVQ shift each lane by the FULL per-lane count; a
    // count >= 64 yields 0 (arithmetic form sign-fills).  The old emulator
    // masked the count to 6 bits first, so a count of exactly 64 became a
    // shift by 0 and passed the source through unchanged (this silently
    // corrupted glibc's vectorized poly1305 limb reduction).
    if (b & CPUID7_AVX512F) {
        {
            unsigned long long q[8];
            // src.q = 1, 2, 4, 8, 0x10, 0x20, 1<<63, 0xdeadbeefcafe
            // Both operands live in volatile memory: gcc constant-folds
            // _mm512_srlv_epi64/_mm512_sllv_epi64 on compile-time inputs
            // (the whole group vanished from the binary), so without this
            // the regression check never executes the instruction.
            volatile __m512i vsrc =
                _mm512_set_epi64((long long)0x0000deadbeefcafeULL,
                                 (long long)0x8000000000000000ULL,
                                 0x20, 0x10, 0x8, 0x4, 0x2, 0x1);
            // counts per lane: 64, 65, 63, 1, 0, 32, 64, 16
            volatile __m512i vcnt = _mm512_set_epi64(16, 64, 32, 0, 1, 63, 65, 64);
            _mm512_storeu_si512(q, _mm512_srlv_epi64(vsrc, vcnt));
            CHECK(q[0] == 0 && q[1] == 0 && q[2] == 0 &&
                      q[3] == 0x4 &&                    /* >> 1 */
                      q[4] == 0x10 &&                   /* count 0 */
                      q[5] == 0 &&                      /* >> 32 of 0x20 */
                      q[6] == 0 &&                      /* (1<<63) >> 64 */
                      q[7] == 0x0000deadbeefULL,        /* >> 16 */
                  "vpsrlvq count>=64 zeroes lane");
            _mm512_storeu_si512(q, _mm512_sllv_epi64(vsrc, vcnt));
            CHECK(q[0] == 0 && q[1] == 0 && q[2] == 0 &&
                      q[3] == 0x10 &&                   /* << 1 */
                      q[4] == 0x10 &&                   /* count 0 */
                      q[5] == 0x2000000000ULL &&        /* << 32 */
                      q[6] == 0 &&                      /* (1<<63) << 64 */
                      q[7] == 0xdeadbeefcafe0000ULL,    /* << 16 */
                  "vpsllvq count>=64 zeroes lane");
            printf("vpsllvq done\n");
        }
    } else {
        printf("skip vpsllvq (no host AVX512F)\n");
    }

    // ---- VPERMQ (EVEX.512, imm8): selects repeat per 256-bit half -------
    // dest.q[4H + j] = src.q[4H + ((imm8 >> 2j) & 3)] for each half H.  The
    // old emulator indexed the immediate by the global qword number, so the
    // upper half read imm bits 8-15 (always zero) instead of reusing the
    // low selects.  (glibc's vectorized poly1305 uses this form.)
    if (b & CPUID7_AVX512F) {
        {
            unsigned long long q[8];
            // src.q = 1,2,3,4,5,6,7,8.  Inline asm over a volatile source:
            // gcc constant-folds _mm512_permutex_epi64 on compile-time
            // inputs (the group vanished from the binary), so the check
            // never executed the instruction.
            volatile __m512i vsrc = _mm512_set_epi64(8, 7, 6, 5, 4, 3, 2, 1);
            __m512i o;
            // imm=0x02: each half becomes [q2, q0, q0, q0]:
            // low = 3,1,1,1  high = 7,5,5,5.
            asm volatile("vpermq $0x02, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 3 && q[1] == 1 && q[2] == 1 && q[3] == 1 &&
                      q[4] == 7 && q[5] == 5 && q[6] == 5 && q[7] == 5,
                  "vpermq zmm imm=0x02 upper half");
            // imm=0x1b (00 01 10 11): each half becomes [q3, q2, q1, q0]:
            // low = 4,3,2,1  high = 8,7,6,5.
            asm volatile("vpermq $0x1b, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 4 && q[1] == 3 && q[2] == 2 && q[3] == 1 &&
                      q[4] == 8 && q[5] == 7 && q[6] == 6 && q[7] == 5,
                  "vpermq zmm imm=0x1b upper half");
            printf("vpermq done\n");
        }
    } else {
        printf("skip vpermq (no host AVX512F)\n");
    }

    // ---- EVEX immediate shifts: the imm8 count is NOT masked --------------
    // VPSLL{W,D,Q}/VPSRL{W,D,Q}/VPSRA{W,D,Q} with an imm8 count do NOT mask
    // it: a count >= the element width zeroes the lane (the arithmetic forms
    // sign-fill).  The old emulator masked the qword immediate count to 6
    // bits first, so vpsrlq $64 became a shift by 0 and passed the source
    // through unchanged.  The immediates are inline asm (gcc folds the
    // intrinsic forms on constant inputs) with a volatile source operand.
    if (b & CPUID7_AVX512F) {
        {
            unsigned long long q[8];
            volatile __m512i vsrc =
                _mm512_set1_epi64((long long)0x8899aabbccddeeffULL);
            __m512i o;
            // Count == element width: zero (the old 6-bit mask made this a
            // shift by 0, i.e. a no-op).
            asm volatile("vpsrlq $0x40, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0 && q[7] == 0, "vpsrlq imm=64 zeroes lane");
            asm volatile("vpsllq $0x40, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0 && q[7] == 0, "vpsllq imm=64 zeroes lane");
            // Arithmetic form: count >= width sign-fills (0x8899... is
            // negative).
            asm volatile("vpsraq $0x40, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0xffffffffffffffffULL &&
                      q[7] == 0xffffffffffffffffULL,
                  "vpsraq imm=64 sign-fills");
            // Count 255: & 63 = 63 under the old mask (a shift by 63!);
            // hardware zeroes.
            asm volatile("vpsrlq $0xff, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0 && q[7] == 0, "vpsrlq imm=255 zeroes lane");
            // Dword and word forms at count == width.
            asm volatile("vpsrld $0x20, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0 && q[7] == 0, "vpsrld imm=32 zeroes lane");
            asm volatile("vpsrlw $0x10, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0 && q[7] == 0, "vpsrlw imm=16 zeroes lane");
            // Every word of 0x8899aabbccddeeff has bit 15 set.
            asm volatile("vpsraw $0x10, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0xffffffffffffffffULL &&
                      q[7] == 0xffffffffffffffffULL,
                  "vpsraw imm=16 sign-fills");
            // In-range controls (unchanged behavior).
            asm volatile("vpsrlq $1, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0x444cd55de66ef77fULL,
                  "vpsrlq imm=1 control");
            asm volatile("vpsllq $1, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0x1133557799bbddfeULL,
                  "vpsllq imm=1 control");
            // Byte-shift forms: count >= 16 zeroes the lane too.
            asm volatile("vpsrldq $0x10, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0 && q[7] == 0, "vpsrldq imm=16 zeroes lane");
            printf("evex-imm-shift done\n");
        }
    } else {
        printf("skip evex-imm-shift (no host AVX512F)\n");
    }

    // ---- VPROLD/VPRORD/VPROLQ/VPRORQ (AVX512F imm8 rotates) ---------------
    // Per-lane rotate by imm8 & (width - 1) (newly implemented; the chacha20
    // tests need it).  Every check is inline asm over a volatile source:
    // gcc constant-folds the rotate intrinsics on compile-time inputs and
    // even rewrites ror-by-imm into the equivalent rol (the old "vprord ror
    // 31" check compiled to vprold $1, so the VPRORD decode path was never
    // exercised).  ror 17 is checked because it is not equivalent to a small
    // rotate in the other direction, so an inverted rotate direction cannot
    // sneak through.
    if (b & CPUID7_AVX512F) {
        {
            unsigned int q[16];
            unsigned long long q8[8];
            volatile unsigned seed = 0x01234567u;
            volatile __m512i vsrc = _mm512_set1_epi32((int)seed);
            __m512i o;
            // rol 1 of 0x01234567 = 0x02468ace.
            asm volatile("vprold $1, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0x02468aceu, "vprold rol 1");
            // rol 7 of 0x01234567 = 0x91a2b380.
            asm volatile("vprold $7, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0x91a2b380u, "vprold rol 7");
            // ror 1 of 0x01234567 = 0x8091a2b3.
            asm volatile("vprord $1, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0x8091a2b3u, "vprord ror 1");
            // ror 17 of 0x01234567 = 0xa2b38091.
            asm volatile("vprord $17, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0xa2b38091u, "vprord ror 17");
            // ror 31 of 0x01234567 = rol 1 = 0x02468ace.
            asm volatile("vprord $31, %1, %0" : "=&x"(o) : "x"(vsrc));
            _mm512_storeu_si512(q, o);
            CHECK(q[0] == 0x02468aceu, "vprord ror 31");
            // Qword rotates of 0x0123456789abcdef.
            volatile __m512i vsrc64 =
                _mm512_set1_epi64((long long)0x0123456789abcdefULL);
            // rol 17 = 0x8acf13579bde0246.
            asm volatile("vprolq $17, %1, %0" : "=&x"(o) : "x"(vsrc64));
            _mm512_storeu_si512(q8, o);
            CHECK(q8[0] == 0x8acf13579bde0246ULL &&
                      q8[7] == 0x8acf13579bde0246ULL,
                  "vprolq rol 17");
            // ror 17 = 0xe6f78091a2b3c4d5.
            asm volatile("vprorq $17, %1, %0" : "=&x"(o) : "x"(vsrc64));
            _mm512_storeu_si512(q8, o);
            CHECK(q8[0] == 0xe6f78091a2b3c4d5ULL,
                  "vprorq ror 17");
            printf("vprol done\n");
        }
    } else {
        printf("skip vprol (no host AVX512F)\n");
    }

    // ---- VEX.128 VMOVD/VMOVQ zeroing rule --------------------------------
    // The VEX.128 m64/r64 -> xmm forms must clear every bit of the
    // containing YMM above the loaded element (legacy SSE MOVD/MOVQ only
    // clears within the XMM).  The old emulator cleared just the 128-bit
    // image, leaving stale upper-YMM lanes that openssl's poly1305 IFMA
    // path (VEX vmovq key-power reloads feeding VPMADD52) multiplied into
    // the tag.  Needs only AVX (VEX), not AVX512.
    cpuid(1, &a, &b, &c, &d);
    if (c & (1u << 28)) {
        {
            unsigned long long garbage = 0xddddddddddddddddULL;
            unsigned long long val = 0x8899aabbccddeeffULL;
            unsigned long long q[8];
            __asm__ volatile(
                "vbroadcastsd %1, %%ymm3\n"
                "vmovq %2, %%xmm3\n"
                "vmovdqu %%ymm3, %0\n"
                : "=m"(q)
                : "m"(garbage), "m"(val)
                : );
            CHECK(q[0] == 0x8899aabbccddeeffULL, "vex vmovq m64 loads qword");
            CHECK(q[1] == 0 && q[2] == 0 && q[3] == 0,
                  "vex vmovq m64 zeroes upper ymm");
        }
        {
            unsigned long long garbage = 0xeeeeeeeeeeeeeeeeULL;
            unsigned long long val = 0x0123456789abcdefULL;
            unsigned long long q[8];
            __asm__ volatile(
                "vbroadcastsd %1, %%ymm4\n"
                "movq %2, %%rax\n"
                "vmovq %%rax, %%xmm4\n"
                "vmovdqu %%ymm4, %0\n"
                : "=m"(q)
                : "m"(garbage), "m"(val)
                : );
            CHECK(q[0] == 0x0123456789abcdefULL, "vex vmovq r64 loads qword");
            CHECK(q[1] == 0 && q[2] == 0 && q[3] == 0,
                  "vex vmovq r64 zeroes upper ymm");
        }
        {
            unsigned long long garbage = 0xdfdfdfdfdfdfdfdfULL;
            unsigned long long val = 0x5566778899aabbccULL;
            unsigned long long q[8];
            __asm__ volatile(
                "vbroadcastsd %1, %%ymm6\n"
                "movq %2, %%xmm6\n"     /* legacy SSE load: no VEX zeroing */
                "vmovdqu %%ymm6, %0\n"
                : "=m"(q)
                : "m"(garbage), "m"(val)
                : );
            CHECK(q[0] == 0x5566778899aabbccULL, "legacy movq m64 loads qword");
            CHECK(q[1] == 0, "legacy movq m64 clears bits 127:64");
            CHECK(q[2] == garbage && q[3] == garbage,
                  "legacy movq m64 preserves upper ymm");
        }
        printf("vex-vmovq-zeroing done\n");
    } else {
        printf("skip vex-vmovq-zeroing (no host AVX)\n");
    }

    if (failures == 0) {
        printf("insn-fixes-avx512-ok\n");
        return 0;
    }
    printf("insn-fixes-avx512 FAILED: %d\n", failures);
    return 1;
}
