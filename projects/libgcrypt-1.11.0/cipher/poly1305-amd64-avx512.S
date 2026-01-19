/*
;;
;; Copyright (c) 2021-2022, Intel Corporation
;;
;; Redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are met:
;;
;;     * Redistributions of source code must retain the above copyright notice,
;;       this list of conditions and the following disclaimer.
;;     * Redistributions in binary form must reproduce the above copyright
;;       notice, this list of conditions and the following disclaimer in the
;;       documentation and/or other materials provided with the distribution.
;;     * Neither the name of Intel Corporation nor the names of its contributors
;;       may be used to endorse or promote products derived from this software
;;       without specific prior written permission.
;;
;; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;; AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;; DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
;; FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;; DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;; SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;; CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;; OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;; OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;
*/
/*
 * From:
 *  https://github.com/intel/intel-ipsec-mb/blob/f0cad21a644231c0f5d4af51f56061a5796343fb/lib/avx512/poly_fma_avx512.asm
 *
 * Conversion to GAS assembly and integration to libgcrypt
 *  by Jussi Kivilinna <jussi.kivilinna@iki.fi>
 */

#ifdef __x86_64
#include <config.h>
#if (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS)) && \
    defined(HAVE_INTEL_SYNTAX_PLATFORM_AS) && \
    defined(HAVE_GCC_INLINE_ASM_AVX512)
#include "asm-common-amd64.h"

.intel_syntax noprefix

SECTION_RODATA

ELF(.type _gcry_poly1305_avx512_consts,@object)
_gcry_poly1305_avx512_consts:

.align 64
.Lmask_44:
  .quad 0xfffffffffff, 0xfffffffffff, 0xfffffffffff, 0xfffffffffff
  .quad 0xfffffffffff, 0xfffffffffff, 0xfffffffffff, 0xfffffffffff

.align 64
.Lmask_42:
  .quad 0x3ffffffffff, 0x3ffffffffff, 0x3ffffffffff, 0x3ffffffffff
  .quad 0x3ffffffffff, 0x3ffffffffff, 0x3ffffffffff, 0x3ffffffffff

.align 64
.Lhigh_bit:
  .quad 0x10000000000, 0x10000000000, 0x10000000000, 0x10000000000
  .quad 0x10000000000, 0x10000000000, 0x10000000000, 0x10000000000

.Lbyte_len_to_mask_table:
  .short 0x0000, 0x0001, 0x0003, 0x0007
  .short 0x000f, 0x001f, 0x003f, 0x007f
  .short 0x00ff, 0x01ff, 0x03ff, 0x07ff
  .short 0x0fff, 0x1fff, 0x3fff, 0x7fff
  .short 0xffff

.align 64
.Lbyte64_len_to_mask_table:
  .quad 0x0000000000000000, 0x0000000000000001
  .quad 0x0000000000000003, 0x0000000000000007
  .quad 0x000000000000000f, 0x000000000000001f
  .quad 0x000000000000003f, 0x000000000000007f
  .quad 0x00000000000000ff, 0x00000000000001ff
  .quad 0x00000000000003ff, 0x00000000000007ff
  .quad 0x0000000000000fff, 0x0000000000001fff
  .quad 0x0000000000003fff, 0x0000000000007fff
  .quad 0x000000000000ffff, 0x000000000001ffff
  .quad 0x000000000003ffff, 0x000000000007ffff
  .quad 0x00000000000fffff, 0x00000000001fffff
  .quad 0x00000000003fffff, 0x00000000007fffff
  .quad 0x0000000000ffffff, 0x0000000001ffffff
  .quad 0x0000000003ffffff, 0x0000000007ffffff
  .quad 0x000000000fffffff, 0x000000001fffffff
  .quad 0x000000003fffffff, 0x000000007fffffff
  .quad 0x00000000ffffffff, 0x00000001ffffffff
  .quad 0x00000003ffffffff, 0x00000007ffffffff
  .quad 0x0000000fffffffff, 0x0000001fffffffff
  .quad 0x0000003fffffffff, 0x0000007fffffffff
  .quad 0x000000ffffffffff, 0x000001ffffffffff
  .quad 0x000003ffffffffff, 0x000007ffffffffff
  .quad 0x00000fffffffffff, 0x00001fffffffffff
  .quad 0x00003fffffffffff, 0x00007fffffffffff
  .quad 0x0000ffffffffffff, 0x0001ffffffffffff
  .quad 0x0003ffffffffffff, 0x0007ffffffffffff
  .quad 0x000fffffffffffff, 0x001fffffffffffff
  .quad 0x003fffffffffffff, 0x007fffffffffffff
  .quad 0x00ffffffffffffff, 0x01ffffffffffffff
  .quad 0x03ffffffffffffff, 0x07ffffffffffffff
  .quad 0x0fffffffffffffff, 0x1fffffffffffffff
  .quad 0x3fffffffffffffff, 0x7fffffffffffffff
  .quad 0xffffffffffffffff

.Lqword_high_bit_mask:
  .short 0, 0x1, 0x5, 0x15, 0x55, 0x57, 0x5f, 0x7f, 0xff

ELF(.size _gcry_poly1305_avx512_consts,.-_gcry_poly1305_avx512_consts)

#define raxd eax
#define rbxd ebx
#define rcxd ecx
#define rdxd edx
#define rsid esi
#define rdid edi
#define rbpd ebp
#define rspd esp
#define __DWORD(X) X##d
#define DWORD(R) __DWORD(R)

#define arg1    rdi
#define arg2    rsi
#define arg3    rdx
#define arg4    rcx

#define job     arg1
#define gp1     rsi
#define gp2     rcx

/* ;; don't use rdx and rax - they are needed for multiply operation */
#define gp3     rbp
#define gp4     r8
#define gp5     r9
#define gp6     r10
#define gp7     r11
#define gp8     r12
#define gp9     r13
#define gp10    r14
#define gp11    r15

#define len     gp11
#define msg     gp10

#define POLY1305_BLOCK_SIZE 16

#define STACK_r_save         0
#define STACK_r_save_size    (6 * 64)
#define STACK_gpr_save       (STACK_r_save + STACK_r_save_size)
#define STACK_gpr_save_size  (8 * 8)
#define STACK_rsp_save       (STACK_gpr_save + STACK_gpr_save_size)
#define STACK_rsp_save_size  (1 * 8)
#define STACK_SIZE           (STACK_rsp_save + STACK_rsp_save_size)

#define A2_ZERO(...) /**/
#define A2_ZERO_INVERT(...) __VA_ARGS__
#define A2_NOT_ZERO(...) __VA_ARGS__
#define A2_NOT_ZERO_INVERT(...) /**/

#define clear_zmm(vec) vpxord vec, vec, vec

/*
;; =============================================================================
;; =============================================================================
;; Computes hash for message length being multiple of block size
;; =============================================================================
;; Combining 64-bit x 64-bit multiplication with reduction steps
;;
;; NOTES:
;;   1) A2 here is only two bits so anything above is subject of reduction.
;;      Constant C1 = R1 + (R1 >> 2) simplifies multiply with less operations
;;   2) Magic 5x comes from mod 2^130-5 property and incorporating
;;      reduction into multiply phase.
;;      See "Cheating at modular arithmetic" and "Poly1305's prime: 2^130 - 5"
;;      paragraphs at https://loup-vaillant.fr/tutorials/poly1305-design for more details.
;;
;; Flow of the code below is as follows:
;;
;;          A2        A1        A0
;;        x           R1        R0
;;   -----------------------------
;;       A2×R0     A1×R0     A0×R0
;;   +             A0×R1
;;   +           5xA2xR1   5xA1xR1
;;   -----------------------------
;;     [0|L2L] [L1H|L1L] [L0H|L0L]
;;
;;   Registers:  T3:T2     T1:A0
;;
;; Completing the multiply and adding (with carry) 3x128-bit limbs into
;; 192-bits again (3x64-bits):
;; A0 = L0L
;; A1 = L0H + L1L
;; T3 = L1H + L2L
; A0     [in/out] GPR with accumulator bits 63:0
; A1     [in/out] GPR with accumulator bits 127:64
; A2     [in/out] GPR with accumulator bits 195:128
; R0     [in] GPR with R constant bits 63:0
; R1     [in] GPR with R constant bits 127:64
; C1     [in] C1 = R1 + (R1 >> 2)
; T1     [clobbered] GPR register
; T2     [clobbered] GPR register
; T3     [clobbered] GPR register
; GP_RAX [clobbered] RAX register
; GP_RDX [clobbered] RDX register
; IF_A2  [in] Used if input A2 is not 0
*/
#define POLY1305_MUL_REDUCE(A0, A1, A2, R0, R1, C1, T1, T2, T3, GP_RAX, GP_RDX, IF_A2) \
	/* T3:T2 = (A0 * R1) */ \
	mov     GP_RAX, R1; \
	mul     A0; \
	mov     T2, GP_RAX; \
	mov     GP_RAX, R0; \
	mov     T3, GP_RDX; \
	\
	/* T1:A0 = (A0 * R0) */ \
	mul     A0; \
	mov     A0, GP_RAX; /* A0 not used in other operations */ \
	mov     GP_RAX, R0; \
	mov     T1, GP_RDX; \
	\
	/* T3:T2 += (A1 * R0) */ \
	mul     A1; \
	add     T2, GP_RAX; \
	mov     GP_RAX, C1; \
	adc     T3, GP_RDX; \
	\
	/* T1:A0 += (A1 * R1x5) */ \
	mul     A1; \
	IF_A2(mov A1, A2); /* use A1 for A2 */ \
	add     A0, GP_RAX; \
	adc     T1, GP_RDX; \
	\
	/* NOTE: A2 is clamped to 2-bits, */ \
	/*       R1/R0 is clamped to 60-bits, */ \
	/*       their product is less than 2^64. */ \
	\
	IF_A2(/* T3:T2 += (A2 * R1x5) */); \
	IF_A2(imul    A1, C1); \
	IF_A2(add     T2, A1); \
	IF_A2(mov     A1, T1); /* T1:A0 => A1:A0 */ \
	IF_A2(adc     T3, 0); \
	\
	IF_A2(/* T3:A1 += (A2 * R0) */); \
	IF_A2(imul    A2, R0); \
	IF_A2(add     A1, T2); \
	IF_A2(adc     T3, A2); \
	\
	IF_A2##_INVERT(/* If A2 == 0, just move and add T1-T2 to A1 */); \
	IF_A2##_INVERT(mov     A1, T1); \
	IF_A2##_INVERT(add     A1, T2); \
	IF_A2##_INVERT(adc     T3, 0); \
	\
	/* At this point, 3 64-bit limbs are in T3:A1:A0 */ \
	/* T3 can span over more than 2 bits so final partial reduction step is needed. */ \
	\
	/* Partial reduction (just to fit into 130 bits) */ \
	/*    A2 = T3 & 3 */ \
	/*    k = (T3 & ~3) + (T3 >> 2) */ \
	/*         Y    x4  +  Y    x1 */ \
	/*    A2:A1:A0 += k */ \
	\
	/* Result will be in A2:A1:A0 */ \
	mov     T1, T3; \
	mov     DWORD(A2), DWORD(T3); \
	and     T1, ~3; \
	shr     T3, 2; \
	and     DWORD(A2), 3; \
	add     T1, T3; \
	\
	/* A2:A1:A0 += k (kept in T1) */ \
	add     A0, T1; \
	adc     A1, 0; \
	adc     DWORD(A2), 0

/*
;; =============================================================================
;; =============================================================================
;; Computes hash for 8 16-byte message blocks,
;; and adds new message blocks to accumulator.
;;
;; It first multiplies all 8 blocks with powers of R:
;;
;;      a2      a1      a0
;; ×    b2      b1      b0
;; ---------------------------------------
;;     a2×b0   a1×b0   a0×b0
;; +   a1×b1   a0×b1 5×a2×b1
;; +   a0×b2 5×a2×b2 5×a1×b2
;; ---------------------------------------
;;        p2      p1      p0
;;
;; Then, it propagates the carry (higher bits after bit 43) from lower limbs into higher limbs,
;; multiplying by 5 in case of the carry of p2.
;;
;A0    [in/out] ZMM register containing 1st 44-bit limb of the 8 blocks
;A1    [in/out] ZMM register containing 2nd 44-bit limb of the 8 blocks
;A2    [in/out] ZMM register containing 3rd 44-bit limb of the 8 blocks
;R0    [in] ZMM register (R0) to include the 1st limb of R
;R1    [in] ZMM register (R1) to include the 2nd limb of R
;R2    [in] ZMM register (R2) to include the 3rd limb of R
;R1P   [in] ZMM register (R1') to include the 2nd limb of R (multiplied by 5)
;R2P   [in] ZMM register (R2') to include the 3rd limb of R (multiplied by 5)
;P0_L  [clobbered] ZMM register to contain p[0] of the 8 blocks
;P0_H  [clobbered] ZMM register to contain p[0] of the 8 blocks
;P1_L  [clobbered] ZMM register to contain p[1] of the 8 blocks
;P1_H  [clobbered] ZMM register to contain p[1] of the 8 blocks
;P2_L  [clobbered] ZMM register to contain p[2] of the 8 blocks
;P2_H  [clobbered] ZMM register to contain p[2] of the 8 blocks
;ZTMP1 [clobbered] Temporary ZMM register
*/
#define POLY1305_MUL_REDUCE_VEC(A0, A1, A2, R0, R1, R2, R1P, R2P, P0_L, P0_H, \
				P1_L, P1_H, P2_L, P2_H, ZTMP1) \
	/* ;; Reset accumulator */ \
	vpxorq  P0_L, P0_L, P0_L; \
	vpxorq  P0_H, P0_H, P0_H; \
	vpxorq  P1_L, P1_L, P1_L; \
	vpxorq  P1_H, P1_H, P1_H; \
	vpxorq  P2_L, P2_L, P2_L; \
	vpxorq  P2_H, P2_H, P2_H; \
	\
	/* ; Reset accumulator and calculate products */ \
	vpmadd52luq P0_L, A2, R1P; \
	vpmadd52huq P0_H, A2, R1P; \
	vpmadd52luq P1_L, A2, R2P; \
	vpmadd52huq P1_H, A2, R2P; \
	vpmadd52luq P2_L, A2, R0; \
	vpmadd52huq P2_H, A2, R0; \
	\
	vpmadd52luq P1_L, A0, R1; \
	vpmadd52huq P1_H, A0, R1; \
	vpmadd52luq P2_L, A0, R2; \
	vpmadd52huq P2_H, A0, R2; \
	vpmadd52luq P0_L, A0, R0; \
	vpmadd52huq P0_H, A0, R0; \
	\
	vpmadd52luq P0_L, A1, R2P; \
	vpmadd52huq P0_H, A1, R2P; \
	vpmadd52luq P1_L, A1, R0; \
	vpmadd52huq P1_H, A1, R0; \
	vpmadd52luq P2_L, A1, R1; \
	vpmadd52huq P2_H, A1, R1; \
	\
	/* ; Carry propagation (first pass) */ \
	vpsrlq  ZTMP1, P0_L, 44; \
	vpandq  A0, P0_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpsllq  P0_H, P0_H, 8; \
	vpaddq  P0_H, P0_H, ZTMP1; \
	vpaddq  P1_L, P1_L, P0_H; \
	vpandq  A1, P1_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpsrlq  ZTMP1, P1_L, 44; \
	vpsllq  P1_H, P1_H, 8; \
	vpaddq  P1_H, P1_H, ZTMP1; \
	vpaddq  P2_L, P2_L, P1_H; \
	vpandq  A2, P2_L, [.Lmask_42 ADD_RIP]; /* ; Clear top 22 bits */ \
	vpsrlq  ZTMP1, P2_L, 42; \
	vpsllq  P2_H, P2_H, 10; \
	vpaddq  P2_H, P2_H, ZTMP1; \
	\
	/* ; Carry propagation (second pass) */ \
	\
	/* ; Multiply by 5 the highest bits (above 130 bits) */ \
	vpaddq  A0, A0, P2_H; \
	vpsllq  P2_H, P2_H, 2; \
	vpaddq  A0, A0, P2_H; \
	vpsrlq  ZTMP1, A0, 44; \
	vpandq  A0, A0, [.Lmask_44 ADD_RIP]; \
	vpaddq  A1, A1, ZTMP1;

/*
;; =============================================================================
;; =============================================================================
;; Computes hash for 16 16-byte message blocks,
;; and adds new message blocks to accumulator,
;; interleaving this computation with the loading and splatting
;; of new data.
;;
;; It first multiplies all 16 blocks with powers of R (8 blocks from A0-A2
;; and 8 blocks from B0-B2, multiplied by R0-R2)
;;
;;      a2      a1      a0
;; ×    b2      b1      b0
;; ---------------------------------------
;;     a2×b0   a1×b0   a0×b0
;; +   a1×b1   a0×b1 5×a2×b1
;; +   a0×b2 5×a2×b2 5×a1×b2
;; ---------------------------------------
;;        p2      p1      p0
;;
;; Then, it propagates the carry (higher bits after bit 43)
;; from lower limbs into higher limbs,
;; multiplying by 5 in case of the carry of p2, and adds
;; the results to A0-A2 and B0-B2.
;;
;; =============================================================================
;A0    [in/out] ZMM register containing 1st 44-bit limb of blocks 1-8
;A1    [in/out] ZMM register containing 2nd 44-bit limb of blocks 1-8
;A2    [in/out] ZMM register containing 3rd 44-bit limb of blocks 1-8
;B0    [in/out] ZMM register containing 1st 44-bit limb of blocks 9-16
;B1    [in/out] ZMM register containing 2nd 44-bit limb of blocks 9-16
;B2    [in/out] ZMM register containing 3rd 44-bit limb of blocks 9-16
;R0    [in] ZMM register (R0) to include the 1st limb of R
;R1    [in] ZMM register (R1) to include the 2nd limb of R
;R2    [in] ZMM register (R2) to include the 3rd limb of R
;R1P   [in] ZMM register (R1') to include the 2nd limb of R (multiplied by 5)
;R2P   [in] ZMM register (R2') to include the 3rd limb of R (multiplied by 5)
;P0_L  [clobbered] ZMM register to contain p[0] of the 8 blocks 1-8
;P0_H  [clobbered] ZMM register to contain p[0] of the 8 blocks 1-8
;P1_L  [clobbered] ZMM register to contain p[1] of the 8 blocks 1-8
;P1_H  [clobbered] ZMM register to contain p[1] of the 8 blocks 1-8
;P2_L  [clobbered] ZMM register to contain p[2] of the 8 blocks 1-8
;P2_H  [clobbered] ZMM register to contain p[2] of the 8 blocks 1-8
;Q0_L  [clobbered] ZMM register to contain p[0] of the 8 blocks 9-16
;Q0_H  [clobbered] ZMM register to contain p[0] of the 8 blocks 9-16
;Q1_L  [clobbered] ZMM register to contain p[1] of the 8 blocks 9-16
;Q1_H  [clobbered] ZMM register to contain p[1] of the 8 blocks 9-16
;Q2_L  [clobbered] ZMM register to contain p[2] of the 8 blocks 9-16
;Q2_H  [clobbered] ZMM register to contain p[2] of the 8 blocks 9-16
;ZTMP1 [clobbered] Temporary ZMM register
;ZTMP2 [clobbered] Temporary ZMM register
;ZTMP3 [clobbered] Temporary ZMM register
;ZTMP4 [clobbered] Temporary ZMM register
;ZTMP5 [clobbered] Temporary ZMM register
;ZTMP6 [clobbered] Temporary ZMM register
;ZTMP7 [clobbered] Temporary ZMM register
;ZTMP8 [clobbered] Temporary ZMM register
;ZTMP9 [clobbered] Temporary ZMM register
;MSG   [in/out] Pointer to message
;LEN   [in/out] Length left of message
*/
#define POLY1305_MSG_MUL_REDUCE_VEC16(A0, A1, A2, B0, B1, B2, R0, R1, R2, R1P, \
				      R2P, P0_L, P0_H, P1_L, P1_H, P2_L, P2_H, \
				      Q0_L, Q0_H, Q1_L, Q1_H, Q2_L, Q2_H, \
				      ZTMP1, ZTMP2, ZTMP3, ZTMP4, ZTMP5, \
				      ZTMP6, ZTMP7, ZTMP8, ZTMP9, MSG, LEN) \
	/* ;; Reset accumulator */ \
	vpxorq  P0_L, P0_L, P0_L; \
	vpxorq  P0_H, P0_H, P0_H; \
	vpxorq  P1_L, P1_L, P1_L; \
	vpxorq  P1_H, P1_H, P1_H; \
	vpxorq  P2_L, P2_L, P2_L; \
	vpxorq  P2_H, P2_H, P2_H; \
	vpxorq  Q0_L, Q0_L, Q0_L; \
	vpxorq  Q0_H, Q0_H, Q0_H; \
	vpxorq  Q1_L, Q1_L, Q1_L; \
	vpxorq  Q1_H, Q1_H, Q1_H; \
	vpxorq  Q2_L, Q2_L, Q2_L; \
	vpxorq  Q2_H, Q2_H, Q2_H; \
	\
	/* ;; This code interleaves hash computation with input loading/splatting */ \
	\
		/* ; Calculate products */ \
		vpmadd52luq P0_L, A2, R1P; \
		vpmadd52huq P0_H, A2, R1P; \
	/* ;; input loading of new blocks */ \
	add     MSG, POLY1305_BLOCK_SIZE*16; \
	sub     LEN, POLY1305_BLOCK_SIZE*16; \
	\
		vpmadd52luq Q0_L, B2, R1P; \
		vpmadd52huq Q0_H, B2, R1P; \
		\
		vpmadd52luq P1_L, A2, R2P; \
		vpmadd52huq P1_H, A2, R2P; \
	/* ; Load next block of data (128 bytes) */ \
	vmovdqu64 ZTMP5, [MSG]; \
	vmovdqu64 ZTMP2, [MSG + 64]; \
	\
		vpmadd52luq Q1_L, B2, R2P; \
		vpmadd52huq Q1_H, B2, R2P; \
	\
	/* ; Interleave new blocks of data */ \
	vpunpckhqdq ZTMP3, ZTMP5, ZTMP2; \
	vpunpcklqdq ZTMP5, ZTMP5, ZTMP2; \
	\
		vpmadd52luq P0_L, A0, R0; \
		vpmadd52huq P0_H, A0, R0; \
	/* ; Highest 42-bit limbs of new blocks */ \
	vpsrlq  ZTMP6, ZTMP3, 24; \
	vporq   ZTMP6, ZTMP6, [.Lhigh_bit ADD_RIP]; /* ; Add 2^128 to all 8 final qwords of the message */ \
	\
		vpmadd52luq Q0_L, B0, R0; \
		vpmadd52huq Q0_H, B0, R0; \
		\
	/* ; Middle 44-bit limbs of new blocks */ \
	vpsrlq  ZTMP2, ZTMP5, 44; \
	vpsllq  ZTMP4, ZTMP3, 20; \
	\
		vpmadd52luq P2_L, A2, R0; \
		vpmadd52huq P2_H, A2, R0; \
	vpternlogq ZTMP2, ZTMP4, [.Lmask_44 ADD_RIP], 0xA8; /* ; (A OR B AND C) */ \
	\
	/* ; Lowest 44-bit limbs of new blocks */ \
	vpandq  ZTMP5, ZTMP5, [.Lmask_44 ADD_RIP]; \
	\
		vpmadd52luq Q2_L, B2, R0; \
		vpmadd52huq Q2_H, B2, R0; \
		\
	/* ; Load next block of data (128 bytes) */ \
	vmovdqu64 ZTMP8, [MSG + 64*2]; \
	vmovdqu64 ZTMP9, [MSG + 64*3]; \
	\
		vpmadd52luq P1_L, A0, R1; \
		vpmadd52huq P1_H, A0, R1; \
	/* ; Interleave new blocks of data */ \
	vpunpckhqdq ZTMP3, ZTMP8, ZTMP9; \
	vpunpcklqdq ZTMP8, ZTMP8, ZTMP9; \
	\
		vpmadd52luq Q1_L, B0, R1; \
		vpmadd52huq Q1_H, B0, R1; \
	\
	/* ; Highest 42-bit limbs of new blocks */ \
	vpsrlq  ZTMP7, ZTMP3, 24; \
	vporq   ZTMP7, ZTMP7, [.Lhigh_bit ADD_RIP]; /* ; Add 2^128 to all 8 final qwords of the message */ \
	\
		vpmadd52luq P0_L, A1, R2P; \
		vpmadd52huq P0_H, A1, R2P; \
		\
	/* ; Middle 44-bit limbs of new blocks */ \
	vpsrlq  ZTMP9, ZTMP8, 44; \
	vpsllq  ZTMP4, ZTMP3, 20; \
	\
		vpmadd52luq Q0_L, B1, R2P; \
		vpmadd52huq Q0_H, B1, R2P; \
		\
	vpternlogq ZTMP9, ZTMP4, [.Lmask_44 ADD_RIP], 0xA8; /* ; (A OR B AND C) */ \
	\
	/* ; Lowest 44-bit limbs of new blocks */ \
	vpandq  ZTMP8, ZTMP8, [.Lmask_44 ADD_RIP]; \
	\
		vpmadd52luq P2_L, A0, R2; \
		vpmadd52huq P2_H, A0, R2; \
	/* ; Carry propagation (first pass) */ \
	vpsrlq  ZTMP1, P0_L, 44; \
	vpsllq  P0_H, P0_H, 8; \
		vpmadd52luq Q2_L, B0, R2; \
		vpmadd52huq Q2_H, B0, R2; \
		\
	vpsrlq  ZTMP3, Q0_L, 44; \
	vpsllq  Q0_H, Q0_H, 8; \
	\
		vpmadd52luq P1_L, A1, R0; \
		vpmadd52huq P1_H, A1, R0; \
	/* ; Carry propagation (first pass) - continue */ \
	vpandq  A0, P0_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpaddq  P0_H, P0_H, ZTMP1; \
		vpmadd52luq Q1_L, B1, R0; \
		vpmadd52huq Q1_H, B1, R0; \
	\
	vpandq  B0, Q0_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpaddq  Q0_H, Q0_H, ZTMP3; \
	\
		vpmadd52luq P2_L, A1, R1; \
		vpmadd52huq P2_H, A1, R1; \
	/* ; Carry propagation (first pass) - continue */ \
	vpaddq  P1_L, P1_L, P0_H; \
	vpsllq  P1_H, P1_H, 8; \
	vpsrlq  ZTMP1, P1_L, 44; \
		vpmadd52luq Q2_L, B1, R1; \
		vpmadd52huq Q2_H, B1, R1; \
	\
	vpandq  A1, P1_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpaddq  Q1_L, Q1_L, Q0_H; \
	vpsllq  Q1_H, Q1_H, 8; \
	vpsrlq  ZTMP3, Q1_L, 44; \
	vpandq  B1, Q1_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	\
	vpaddq  P2_L, P2_L, P1_H; /* ; P2_L += P1_H + P1_L[63:44] */ \
	vpaddq  P2_L, P2_L, ZTMP1; \
	vpandq  A2, P2_L, [.Lmask_42 ADD_RIP]; /* ; Clear top 22 bits */ \
	vpaddq  A2, A2, ZTMP6; /* ; Add highest bits from new blocks to accumulator */ \
	vpsrlq  ZTMP1, P2_L, 42; \
	vpsllq  P2_H, P2_H, 10; \
	vpaddq  P2_H, P2_H, ZTMP1; \
	\
	vpaddq  Q2_L, Q2_L, Q1_H; /* ; Q2_L += P1_H + P1_L[63:44] */ \
	vpaddq  Q2_L, Q2_L, ZTMP3; \
	vpandq  B2, Q2_L, [.Lmask_42 ADD_RIP]; /* ; Clear top 22 bits */ \
	vpaddq  B2, B2, ZTMP7; /* ; Add highest bits from new blocks to accumulator */ \
	vpsrlq  ZTMP3, Q2_L, 42; \
	vpsllq  Q2_H, Q2_H, 10; \
	vpaddq  Q2_H, Q2_H, ZTMP3; \
	\
	/* ; Carry propagation (second pass) */ \
	/* ; Multiply by 5 the highest bits (above 130 bits) */ \
	vpaddq  A0, A0, P2_H; \
	vpsllq  P2_H, P2_H, 2; \
	vpaddq  A0, A0, P2_H; \
	vpaddq  B0, B0, Q2_H; \
	vpsllq  Q2_H, Q2_H, 2; \
	vpaddq  B0, B0, Q2_H; \
	\
	vpsrlq  ZTMP1, A0, 44; \
	vpandq  A0, A0, [.Lmask_44 ADD_RIP]; \
	vpaddq  A0, A0, ZTMP5; /* ; Add low 42-bit bits from new blocks to accumulator */ \
	vpaddq  A1, A1, ZTMP2; /* ; Add medium 42-bit bits from new blocks to accumulator */ \
	vpaddq  A1, A1, ZTMP1; \
	vpsrlq  ZTMP3, B0, 44; \
	vpandq  B0, B0, [.Lmask_44 ADD_RIP]; \
	vpaddq  B0, B0, ZTMP8; /* ; Add low 42-bit bits from new blocks to accumulator */ \
	vpaddq  B1, B1, ZTMP9; /* ; Add medium 42-bit bits from new blocks to accumulator */ \
	vpaddq  B1, B1, ZTMP3

/*
;; =============================================================================
;; =============================================================================
;; Computes hash for 16 16-byte message blocks.
;;
;; It first multiplies all 16 blocks with powers of R (8 blocks from A0-A2
;; and 8 blocks from B0-B2, multiplied by R0-R2 and S0-S2)
;;
;;
;;      a2      a1      a0
;; ×    b2      b1      b0
;; ---------------------------------------
;;     a2×b0   a1×b0   a0×b0
;; +   a1×b1   a0×b1 5×a2×b1
;; +   a0×b2 5×a2×b2 5×a1×b2
;; ---------------------------------------
;;        p2      p1      p0
;;
;; Then, it propagates the carry (higher bits after bit 43) from lower limbs into higher limbs,
;; multiplying by 5 in case of the carry of p2.
;;
;; =============================================================================
;A0    [in/out] ZMM register containing 1st 44-bit limb of the 8 blocks
;A1    [in/out] ZMM register containing 2nd 44-bit limb of the 8 blocks
;A2    [in/out] ZMM register containing 3rd 44-bit limb of the 8 blocks
;B0    [in/out] ZMM register containing 1st 44-bit limb of the 8 blocks
;B1    [in/out] ZMM register containing 2nd 44-bit limb of the 8 blocks
;B2    [in/out] ZMM register containing 3rd 44-bit limb of the 8 blocks
;R0    [in] ZMM register (R0) to include the 1st limb in IDX
;R1    [in] ZMM register (R1) to include the 2nd limb in IDX
;R2    [in] ZMM register (R2) to include the 3rd limb in IDX
;R1P   [in] ZMM register (R1') to include the 2nd limb (multiplied by 5) in IDX
;R2P   [in] ZMM register (R2') to include the 3rd limb (multiplied by 5) in IDX
;S0    [in] ZMM register (R0) to include the 1st limb in IDX
;S1    [in] ZMM register (R1) to include the 2nd limb in IDX
;S2    [in] ZMM register (R2) to include the 3rd limb in IDX
;S1P   [in] ZMM register (R1') to include the 2nd limb (multiplied by 5) in IDX
;S2P   [in] ZMM register (R2') to include the 3rd limb (multiplied by 5) in IDX
;P0_L  [clobbered] ZMM register to contain p[0] of the 8 blocks
;P0_H  [clobbered] ZMM register to contain p[0] of the 8 blocks
;P1_L  [clobbered] ZMM register to contain p[1] of the 8 blocks
;P1_H  [clobbered] ZMM register to contain p[1] of the 8 blocks
;P2_L  [clobbered] ZMM register to contain p[2] of the 8 blocks
;P2_H  [clobbered] ZMM register to contain p[2] of the 8 blocks
;Q0_L  [clobbered] ZMM register to contain p[0] of the 8 blocks
;Q0_H  [clobbered] ZMM register to contain p[0] of the 8 blocks
;Q1_L  [clobbered] ZMM register to contain p[1] of the 8 blocks
;Q1_H  [clobbered] ZMM register to contain p[1] of the 8 blocks
;Q2_L  [clobbered] ZMM register to contain p[2] of the 8 blocks
;Q2_H  [clobbered] ZMM register to contain p[2] of the 8 blocks
;ZTMP1 [clobbered] Temporary ZMM register
;ZTMP2 [clobbered] Temporary ZMM register
*/
#define POLY1305_MUL_REDUCE_VEC16(A0, A1, A2, B0, B1, B2, R0, R1, R2, R1P, R2P,\
				  S0, S1, S2, S1P, S2P, P0_L, P0_H, P1_L, P1_H,\
				  P2_L, P2_H, Q0_L, Q0_H, Q1_L, Q1_H, Q2_L,\
				  Q2_H, ZTMP1, ZTMP2) \
	/* ;; Reset accumulator */ \
	vpxorq  P0_L, P0_L, P0_L; \
	vpxorq  P0_H, P0_H, P0_H; \
	vpxorq  P1_L, P1_L, P1_L; \
	vpxorq  P1_H, P1_H, P1_H; \
	vpxorq  P2_L, P2_L, P2_L; \
	vpxorq  P2_H, P2_H, P2_H; \
	vpxorq  Q0_L, Q0_L, Q0_L; \
	vpxorq  Q0_H, Q0_H, Q0_H; \
	vpxorq  Q1_L, Q1_L, Q1_L; \
	vpxorq  Q1_H, Q1_H, Q1_H; \
	vpxorq  Q2_L, Q2_L, Q2_L; \
	vpxorq  Q2_H, Q2_H, Q2_H; \
	\
	/* ;; This code interleaves hash computation with input loading/splatting */ \
	\
	/* ; Calculate products */ \
	vpmadd52luq P0_L, A2, R1P; \
	vpmadd52huq P0_H, A2, R1P; \
	\
	vpmadd52luq Q0_L, B2, S1P; \
	vpmadd52huq Q0_H, B2, S1P; \
	\
	vpmadd52luq P1_L, A2, R2P; \
	vpmadd52huq P1_H, A2, R2P; \
	\
	vpmadd52luq Q1_L, B2, S2P; \
	vpmadd52huq Q1_H, B2, S2P; \
	\
	vpmadd52luq P0_L, A0, R0; \
	vpmadd52huq P0_H, A0, R0; \
	\
	vpmadd52luq Q0_L, B0, S0; \
	vpmadd52huq Q0_H, B0, S0; \
	\
	vpmadd52luq P2_L, A2, R0; \
	vpmadd52huq P2_H, A2, R0; \
	vpmadd52luq Q2_L, B2, S0; \
	vpmadd52huq Q2_H, B2, S0; \
	\
	vpmadd52luq P1_L, A0, R1; \
	vpmadd52huq P1_H, A0, R1; \
	vpmadd52luq Q1_L, B0, S1; \
	vpmadd52huq Q1_H, B0, S1; \
	\
	vpmadd52luq P0_L, A1, R2P; \
	vpmadd52huq P0_H, A1, R2P; \
	\
	vpmadd52luq Q0_L, B1, S2P; \
	vpmadd52huq Q0_H, B1, S2P; \
	\
	vpmadd52luq P2_L, A0, R2; \
	vpmadd52huq P2_H, A0, R2; \
	\
	vpmadd52luq Q2_L, B0, S2; \
	vpmadd52huq Q2_H, B0, S2; \
	\
	/* ; Carry propagation (first pass) */ \
	vpsrlq  ZTMP1, P0_L, 44; \
	vpsllq  P0_H, P0_H, 8; \
	vpsrlq  ZTMP2, Q0_L, 44; \
	vpsllq  Q0_H, Q0_H, 8; \
	\
	vpmadd52luq P1_L, A1, R0; \
	vpmadd52huq P1_H, A1, R0; \
	vpmadd52luq Q1_L, B1, S0; \
	vpmadd52huq Q1_H, B1, S0; \
	\
	/* ; Carry propagation (first pass) - continue */ \
	vpandq  A0, P0_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpaddq  P0_H, P0_H, ZTMP1; \
	vpandq  B0, Q0_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpaddq  Q0_H, Q0_H, ZTMP2; \
	\
	vpmadd52luq P2_L, A1, R1; \
	vpmadd52huq P2_H, A1, R1; \
	vpmadd52luq Q2_L, B1, S1; \
	vpmadd52huq Q2_H, B1, S1; \
	\
	/* ; Carry propagation (first pass) - continue */ \
	vpaddq  P1_L, P1_L, P0_H; \
	vpsllq  P1_H, P1_H, 8; \
	vpsrlq  ZTMP1, P1_L, 44; \
	vpandq  A1, P1_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpaddq  Q1_L, Q1_L, Q0_H; \
	vpsllq  Q1_H, Q1_H, 8; \
	vpsrlq  ZTMP2, Q1_L, 44; \
	vpandq  B1, Q1_L, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	\
	vpaddq  P2_L, P2_L, P1_H; /* ; P2_L += P1_H + P1_L[63:44] */ \
	vpaddq  P2_L, P2_L, ZTMP1; \
	vpandq  A2, P2_L, [.Lmask_42 ADD_RIP]; /* ; Clear top 22 bits */ \
	vpsrlq  ZTMP1, P2_L, 42; \
	vpsllq  P2_H, P2_H, 10; \
	vpaddq  P2_H, P2_H, ZTMP1; \
	\
	vpaddq  Q2_L, Q2_L, Q1_H; /* ; Q2_L += P1_H + P1_L[63:44] */ \
	vpaddq  Q2_L, Q2_L, ZTMP2; \
	vpandq  B2, Q2_L, [.Lmask_42 ADD_RIP]; /* ; Clear top 22 bits */ \
	vpsrlq  ZTMP2, Q2_L, 42; \
	vpsllq  Q2_H, Q2_H, 10; \
	vpaddq  Q2_H, Q2_H, ZTMP2; \
	\
	/* ; Carry propagation (second pass) */ \
	/* ; Multiply by 5 the highest bits (above 130 bits) */ \
	vpaddq  A0, A0, P2_H; \
	vpsllq  P2_H, P2_H, 2; \
	vpaddq  A0, A0, P2_H; \
	vpaddq  B0, B0, Q2_H; \
	vpsllq  Q2_H, Q2_H, 2; \
	vpaddq  B0, B0, Q2_H; \
	\
	vpsrlq  ZTMP1, A0, 44; \
	vpandq  A0, A0, [.Lmask_44 ADD_RIP]; \
	vpaddq  A1, A1, ZTMP1; \
	vpsrlq  ZTMP2, B0, 44; \
	vpandq  B0, B0, [.Lmask_44 ADD_RIP]; \
	vpaddq  B1, B1, ZTMP2;

/*
;; =============================================================================
;; =============================================================================
;; Shuffle data blocks, so they match the right power of R.
;; Powers of R are in this order: R^8 R^4 R^7 R^3 R^6 R^2 R^5 R
;; Data blocks are coming in this order: A0 A4 A1 A5 A2 A6 A3 A7
;; Generally the computation is: A0*R^8 + A1*R^7 + A2*R^6 + A3*R^5 +
;;                               A4*R^4 + A5*R^3 + A6*R^2 + A7*R
;; When there are less data blocks, less powers of R are used, so data needs to
;; be shuffled. Example: if 4 blocks are left, only A0-A3 are available and only
;; R-R^4 are used (A0*R^4 + A1*R^3 + A2*R^2 + A3*R), so A0-A3 need to be shifted
;; =============================================================================
;A_L      [in/out] 0-43 bits of input data
;A_M      [in/out] 44-87 bits of input data
;A_H      [in/out] 88-129 bits of input data
;TMP      [clobbered] Temporary GP register
;N_BLOCKS [in] Number of remaining input blocks
*/
#define SHUFFLE_DATA_SMASK_1 0x39
#define SHUFFLE_DATA_KMASK_1 0xffff
#define SHUFFLE_DATA_SMASK_2 0x4E
#define SHUFFLE_DATA_KMASK_2 0xffff
#define SHUFFLE_DATA_SMASK_3 0x93
#define SHUFFLE_DATA_KMASK_3 0xffff
#define SHUFFLE_DATA_KMASK_4 0xffff
#define SHUFFLE_DATA_SMASK_5 0x39
#define SHUFFLE_DATA_KMASK_5 0xfff0
#define SHUFFLE_DATA_SMASK_6 0x4E
#define SHUFFLE_DATA_KMASK_6 0xff00
#define SHUFFLE_DATA_SMASK_7 0x93
#define SHUFFLE_DATA_KMASK_7 0xf000

#define SHUFFLE_DATA_BLOCKS_GENERIC(A_L, A_M, A_H, TMP, N_BLOCKS) \
	mov     TMP, SHUFFLE_DATA_KMASK_##N_BLOCKS; \
	kmovq   k1, TMP; \
	vpshufd A_L{k1}, A_L, 0x4E; \
	vpshufd A_M{k1}, A_M, 0x4E; \
	vpshufd A_H{k1}, A_H, 0x4E; \
	vshufi64x2 A_L, A_L, A_L, SHUFFLE_DATA_SMASK_##N_BLOCKS; \
	vshufi64x2 A_M, A_M, A_M, SHUFFLE_DATA_SMASK_##N_BLOCKS; \
	vshufi64x2 A_H, A_H, A_H, SHUFFLE_DATA_SMASK_##N_BLOCKS

#define SHUFFLE_DATA_BLOCKS_1(A_L, A_M, A_H, TMP) \
	SHUFFLE_DATA_BLOCKS_GENERIC(A_L, A_M, A_H, TMP, 1)

#define SHUFFLE_DATA_BLOCKS_2(A_L, A_M, A_H, TMP) \
	SHUFFLE_DATA_BLOCKS_GENERIC(A_L, A_M, A_H, TMP, 2)

#define SHUFFLE_DATA_BLOCKS_3(A_L, A_M, A_H, TMP) \
	SHUFFLE_DATA_BLOCKS_GENERIC(A_L, A_M, A_H, TMP, 3)

#define SHUFFLE_DATA_BLOCKS_4(A_L, A_M, A_H, TMP) \
	mov     TMP, SHUFFLE_DATA_KMASK_4; \
	kmovq   k1, TMP; \
	vpshufd A_L{k1}, A_L, 0x4E; \
	vpshufd A_M{k1}, A_M, 0x4E; \
	vpshufd A_H{k1}, A_H, 0x4E;

#define SHUFFLE_DATA_BLOCKS_5(A_L, A_M, A_H, TMP) \
	SHUFFLE_DATA_BLOCKS_GENERIC(A_L, A_M, A_H, TMP, 5)

#define SHUFFLE_DATA_BLOCKS_6(A_L, A_M, A_H, TMP) \
	SHUFFLE_DATA_BLOCKS_GENERIC(A_L, A_M, A_H, TMP, 6)

#define SHUFFLE_DATA_BLOCKS_7(A_L, A_M, A_H, TMP) \
	SHUFFLE_DATA_BLOCKS_GENERIC(A_L, A_M, A_H, TMP, 7)

/*
;; =============================================================================
;; =============================================================================
;; Computes hash for message length being multiple of block size
;; =============================================================================
;MSG    [in/out] GPR pointer to input message (updated)
;LEN    [in/out] GPR in: length in bytes / out: length mod 16
;A0     [in/out] accumulator bits 63..0
;A1     [in/out] accumulator bits 127..64
;A2     [in/out] accumulator bits 195..128
;R0     [in] R constant bits 63..0
;R1     [in] R constant bits 127..64
;T0     [clobbered] GPR register
;T1     [clobbered] GPR register
;T2     [clobbered] GPR register
;T3     [clobbered] GPR register
;GP_RAX [clobbered] RAX register
;GP_RDX [clobbered] RDX register
*/
#define POLY1305_BLOCKS(MSG, LEN, A0, A1, A2, R0, R1, T0, T1, T2, T3, \
			GP_RAX, GP_RDX) \
	/* ; Minimum of 256 bytes to run vectorized code */ \
	cmp     LEN, POLY1305_BLOCK_SIZE*16; \
	jb      .L_final_loop; \
	\
	/* ; Spread accumulator into 44-bit limbs in quadwords */ \
	mov     T0, A0; \
	and     T0, [.Lmask_44 ADD_RIP]; /* ;; First limb (A[43:0]) */ \
	vmovq   xmm5, T0; \
	\
	mov     T0, A1; \
	shrd    A0, T0, 44; \
	and     A0, [.Lmask_44 ADD_RIP]; /* ;; Second limb (A[77:52]) */ \
	vmovq   xmm6, A0; \
	\
	shrd    A1, A2, 24; \
	and     A1, [.Lmask_42 ADD_RIP]; /* ;; Third limb (A[129:88]) */ \
	vmovq   xmm7, A1; \
	\
	/* ; Load first block of data (128 bytes) */ \
	vmovdqu64 zmm0, [MSG]; \
	vmovdqu64 zmm1, [MSG + 64]; \
	\
	/* ; Interleave the data to form 44-bit limbs */ \
	/* ; */ \
	/* ; zmm13 to have bits 0-43 of all 8 blocks in 8 qwords */ \
	/* ; zmm14 to have bits 87-44 of all 8 blocks in 8 qwords */ \
	/* ; zmm15 to have bits 127-88 of all 8 blocks in 8 qwords */ \
	vpunpckhqdq zmm15, zmm0, zmm1; \
	vpunpcklqdq zmm13, zmm0, zmm1; \
	\
	vpsrlq  zmm14, zmm13, 44; \
	vpsllq  zmm18, zmm15, 20; \
	vpternlogq zmm14, zmm18, [.Lmask_44 ADD_RIP], 0xA8; /* ; (A OR B AND C) */ \
	\
	vpandq  zmm13, zmm13, [.Lmask_44 ADD_RIP]; \
	vpsrlq  zmm15, zmm15, 24; \
	\
	/* ; Add 2^128 to all 8 final qwords of the message */ \
	vporq   zmm15, zmm15, [.Lhigh_bit ADD_RIP]; \
	\
	vpaddq  zmm13, zmm13, zmm5; \
	vpaddq  zmm14, zmm14, zmm6; \
	vpaddq  zmm15, zmm15, zmm7; \
	\
	/* ; Load next blocks of data (128 bytes) */ \
	vmovdqu64 zmm0, [MSG + 64*2]; \
	vmovdqu64 zmm1, [MSG + 64*3]; \
	\
	/* ; Interleave the data to form 44-bit limbs */ \
	/* ; */ \
	/* ; zmm13 to have bits 0-43 of all 8 blocks in 8 qwords */ \
	/* ; zmm14 to have bits 87-44 of all 8 blocks in 8 qwords */ \
	/* ; zmm15 to have bits 127-88 of all 8 blocks in 8 qwords */ \
	vpunpckhqdq zmm18, zmm0, zmm1; \
	vpunpcklqdq zmm16, zmm0, zmm1; \
	\
	vpsrlq  zmm17, zmm16, 44; \
	vpsllq  zmm19, zmm18, 20; \
	vpternlogq zmm17, zmm19, [.Lmask_44 ADD_RIP], 0xA8; /* ; (A OR B AND C) */ \
	\
	vpandq  zmm16, zmm16, [.Lmask_44 ADD_RIP]; \
	vpsrlq  zmm18, zmm18, 24; \
	\
	/* ; Add 2^128 to all 8 final qwords of the message */ \
	vporq   zmm18, zmm18, [.Lhigh_bit ADD_RIP]; \
	\
	/* ; Use memory in stack to save powers of R, before loading them into ZMM registers */ \
	/* ; The first 16*8 bytes will contain the 16 bytes of the 8 powers of R */ \
	/* ; The last 64 bytes will contain the last 2 bits of powers of R, spread in 8 qwords, */ \
	/* ; to be OR'd with the highest qwords (in zmm26) */ \
	vmovq   xmm3, R0; \
	vpinsrq xmm3, xmm3, R1, 1; \
	vinserti32x4 zmm1, zmm1, xmm3, 3; \
	\
	vpxorq  zmm0, zmm0, zmm0; \
	vpxorq  zmm2, zmm2, zmm2; \
	\
	/* ; Calculate R^2 */ \
	mov     T0, R1; \
	shr     T0, 2; \
	add     T0, R1; /* ;; T0 = R1 + (R1 >> 2) */ \
	\
	mov     A0, R0; \
	mov     A1, R1; \
	\
	POLY1305_MUL_REDUCE(A0, A1, A2, R0, R1, T0, T1, T2, T3, GP_RAX, GP_RDX, A2_ZERO); \
	\
	vmovq   xmm3, A0; \
	vpinsrq xmm3, xmm3, A1, 1; \
	vinserti32x4 zmm1, zmm1, xmm3, 2; \
	\
	vmovq   xmm4, A2; \
	vinserti32x4 zmm2, zmm2, xmm4, 2; \
	\
	/* ; Calculate R^3 */ \
	POLY1305_MUL_REDUCE(A0, A1, A2, R0, R1, T0, T1, T2, T3, GP_RAX, GP_RDX, A2_NOT_ZERO); \
	\
	vmovq   xmm3, A0; \
	vpinsrq xmm3, xmm3, A1, 1; \
	vinserti32x4 zmm1, zmm1, xmm3, 1; \
	\
	vmovq   xmm4, A2; \
	vinserti32x4 zmm2, zmm2, xmm4, 1; \
	\
	/* ; Calculate R^4 */ \
	POLY1305_MUL_REDUCE(A0, A1, A2, R0, R1, T0, T1, T2, T3, GP_RAX, GP_RDX, A2_NOT_ZERO); \
	\
	vmovq   xmm3, A0; \
	vpinsrq xmm3, xmm3, A1, 1; \
	vinserti32x4 zmm1, zmm1, xmm3, 0; \
	\
	vmovq   xmm4, A2; \
	vinserti32x4 zmm2, zmm2, xmm4, 0; \
	\
	/* ; Move 2 MSbits to top 24 bits, to be OR'ed later */ \
	vpsllq  zmm2, zmm2, 40; \
	\
	vpunpckhqdq zmm21, zmm1, zmm0; \
	vpunpcklqdq zmm19, zmm1, zmm0; \
	\
	vpsrlq  zmm20, zmm19, 44; \
	vpsllq  zmm4, zmm21, 20; \
	vpternlogq zmm20, zmm4, [.Lmask_44 ADD_RIP], 0xA8; /* ; (A OR B AND C) */ \
	\
	vpandq  zmm19, zmm19, [.Lmask_44 ADD_RIP]; \
	vpsrlq  zmm21, zmm21, 24; \
	\
	/* ; zmm2 contains the 2 highest bits of the powers of R */ \
	vporq   zmm21, zmm21, zmm2; \
	\
	/* ; Broadcast 44-bit limbs of R^4 */ \
	mov     T0, A0; \
	and     T0, [.Lmask_44 ADD_RIP]; /* ;; First limb (R^4[43:0]) */ \
	vpbroadcastq zmm22, T0; \
	\
	mov     T0, A1; \
	shrd    A0, T0, 44; \
	and     A0, [.Lmask_44 ADD_RIP]; /* ;; Second limb (R^4[87:44]) */ \
	vpbroadcastq zmm23, A0; \
	\
	shrd    A1, A2, 24; \
	and     A1, [.Lmask_42 ADD_RIP]; /* ;; Third limb (R^4[129:88]) */ \
	vpbroadcastq zmm24, A1; \
	\
	/* ; Generate 4*5*R^4 */ \
	vpsllq  zmm25, zmm23, 2; \
	vpsllq  zmm26, zmm24, 2; \
	\
	/* ; 5*R^4 */ \
	vpaddq  zmm25, zmm25, zmm23; \
	vpaddq  zmm26, zmm26, zmm24; \
	\
	/* ; 4*5*R^4 */ \
	vpsllq  zmm25, zmm25, 2; \
	vpsllq  zmm26, zmm26, 2; \
	\
	vpslldq zmm29, zmm19, 8; \
	vpslldq zmm30, zmm20, 8; \
	vpslldq zmm31, zmm21, 8; \
	\
	/* ; Calculate R^8-R^5 */ \
	POLY1305_MUL_REDUCE_VEC(zmm19, zmm20, zmm21, \
				zmm22, zmm23, zmm24, \
				zmm25, zmm26, \
				zmm5, zmm6, zmm7, zmm8, zmm9, zmm10, \
				zmm11); \
	\
	/* ; Interleave powers of R: R^8 R^4 R^7 R^3 R^6 R^2 R^5 R */ \
	vporq   zmm19, zmm19, zmm29; \
	vporq   zmm20, zmm20, zmm30; \
	vporq   zmm21, zmm21, zmm31; \
	\
	/* ; Broadcast R^8 */ \
	vpbroadcastq zmm22, xmm19; \
	vpbroadcastq zmm23, xmm20; \
	vpbroadcastq zmm24, xmm21; \
	\
	/* ; Generate 4*5*R^8 */ \
	vpsllq  zmm25, zmm23, 2; \
	vpsllq  zmm26, zmm24, 2; \
	\
	/* ; 5*R^8 */ \
	vpaddq  zmm25, zmm25, zmm23; \
	vpaddq  zmm26, zmm26, zmm24; \
	\
	/* ; 4*5*R^8 */ \
	vpsllq  zmm25, zmm25, 2; \
	vpsllq  zmm26, zmm26, 2; \
	\
	cmp     LEN, POLY1305_BLOCK_SIZE*32; \
	jb      .L_len_256_511; \
	\
	/* ; Store R^8-R for later use */ \
	vmovdqa64 [rsp + STACK_r_save], zmm19; \
	vmovdqa64 [rsp + STACK_r_save + 64], zmm20; \
	vmovdqa64 [rsp + STACK_r_save + 64*2], zmm21; \
	\
	/* ; Calculate R^16-R^9 */ \
	POLY1305_MUL_REDUCE_VEC(zmm19, zmm20, zmm21, \
				zmm22, zmm23, zmm24, \
				zmm25, zmm26, \
				zmm5, zmm6, zmm7, zmm8, zmm9, zmm10, \
				zmm11); \
	\
	/* ; Store R^16-R^9 for later use */ \
	vmovdqa64 [rsp + STACK_r_save + 64*3], zmm19; \
	vmovdqa64 [rsp + STACK_r_save + 64*4], zmm20; \
	vmovdqa64 [rsp + STACK_r_save + 64*5], zmm21; \
	\
	/* ; Broadcast R^16 */ \
	vpbroadcastq zmm22, xmm19; \
	vpbroadcastq zmm23, xmm20; \
	vpbroadcastq zmm24, xmm21; \
	\
	/* ; Generate 4*5*R^16 */ \
	vpsllq  zmm25, zmm23, 2; \
	vpsllq  zmm26, zmm24, 2; \
	\
	/* ; 5*R^16 */ \
	vpaddq  zmm25, zmm25, zmm23; \
	vpaddq  zmm26, zmm26, zmm24; \
	\
	/* ; 4*5*R^16 */ \
	vpsllq  zmm25, zmm25, 2; \
	vpsllq  zmm26, zmm26, 2; \
	\
	mov     T0, LEN; \
	and     T0, 0xffffffffffffff00; /* ; multiple of 256 bytes */ \
	\
.L_poly1305_blocks_loop: \
	cmp     T0, POLY1305_BLOCK_SIZE*16; \
	jbe     .L_poly1305_blocks_loop_end; \
	\
	/* ; zmm13-zmm18 contain the 16 blocks of message plus the previous accumulator */ \
	/* ; zmm22-24 contain the 5x44-bit limbs of the powers of R */ \
	/* ; zmm25-26 contain the 5x44-bit limbs of the powers of R' (5*4*R) */ \
	POLY1305_MSG_MUL_REDUCE_VEC16(zmm13, zmm14, zmm15, zmm16, zmm17, zmm18, \
				      zmm22, zmm23, zmm24, zmm25, zmm26, \
				      zmm5, zmm6, zmm7, zmm8, zmm9, zmm10, \
				      zmm19, zmm20, zmm21, zmm27, zmm28, zmm29, \
				      zmm30, zmm31, zmm11, zmm0, zmm1, \
				      zmm2, zmm3, zmm4, zmm12, MSG, T0); \
	\
	jmp     .L_poly1305_blocks_loop; \
	\
.L_poly1305_blocks_loop_end: \
	\
	/* ;; Need to multiply by r^16, r^15, r^14... r */ \
	\
	/* ; First multiply by r^16-r^9 */ \
	\
	/* ; Read R^16-R^9 */ \
	vmovdqa64 zmm19, [rsp + STACK_r_save + 64*3]; \
	vmovdqa64 zmm20, [rsp + STACK_r_save + 64*4]; \
	vmovdqa64 zmm21, [rsp + STACK_r_save + 64*5]; \
	/* ; Read R^8-R */ \
	vmovdqa64 zmm22, [rsp + STACK_r_save]; \
	vmovdqa64 zmm23, [rsp + STACK_r_save + 64]; \
	vmovdqa64 zmm24, [rsp + STACK_r_save + 64*2]; \
	\
	/* ; zmm27 to have bits 87-44 of all 9-16th powers of R' in 8 qwords */ \
	/* ; zmm28 to have bits 129-88 of all 9-16th powers of R' in 8 qwords */ \
	vpsllq  zmm0, zmm20, 2; \
	vpaddq  zmm27, zmm20, zmm0; /* ; R1' (R1*5) */ \
	vpsllq  zmm1, zmm21, 2; \
	vpaddq  zmm28, zmm21, zmm1; /* ; R2' (R2*5) */ \
	\
	/* ; 4*5*R */ \
	vpsllq  zmm27, zmm27, 2; \
	vpsllq  zmm28, zmm28, 2; \
	\
	/* ; Then multiply by r^8-r */ \
	\
	/* ; zmm25 to have bits 87-44 of all 1-8th powers of R' in 8 qwords */ \
	/* ; zmm26 to have bits 129-88 of all 1-8th powers of R' in 8 qwords */ \
	vpsllq  zmm2, zmm23, 2; \
	vpaddq  zmm25, zmm23, zmm2; /* ; R1' (R1*5) */ \
	vpsllq  zmm3, zmm24, 2; \
	vpaddq  zmm26, zmm24, zmm3; /* ; R2' (R2*5) */ \
	\
	/* ; 4*5*R */ \
	vpsllq  zmm25, zmm25, 2; \
	vpsllq  zmm26, zmm26, 2; \
	\
	POLY1305_MUL_REDUCE_VEC16(zmm13, zmm14, zmm15, zmm16, zmm17, zmm18, \
				  zmm19, zmm20, zmm21, zmm27, zmm28, \
				  zmm22, zmm23, zmm24, zmm25, zmm26, \
				  zmm0, zmm1, zmm2, zmm3, zmm4, zmm5, zmm6, \
				  zmm7, zmm8, zmm9, zmm10, zmm11, zmm12, zmm29); \
	\
	/* ;; Add all blocks (horizontally) */ \
	vpaddq  zmm13, zmm13, zmm16; \
	vpaddq  zmm14, zmm14, zmm17; \
	vpaddq  zmm15, zmm15, zmm18; \
	\
	vextracti64x4   ymm0, zmm13, 1; \
	vextracti64x4   ymm1, zmm14, 1; \
	vextracti64x4   ymm2, zmm15, 1; \
	\
	vpaddq  ymm13, ymm13, ymm0; \
	vpaddq  ymm14, ymm14, ymm1; \
	vpaddq  ymm15, ymm15, ymm2; \
	\
	vextracti32x4   xmm10, ymm13, 1; \
	vextracti32x4   xmm11, ymm14, 1; \
	vextracti32x4   xmm12, ymm15, 1; \
	\
	vpaddq  xmm13, xmm13, xmm10; \
	vpaddq  xmm14, xmm14, xmm11; \
	vpaddq  xmm15, xmm15, xmm12; \
	\
	vpsrldq xmm10, xmm13, 8; \
	vpsrldq xmm11, xmm14, 8; \
	vpsrldq xmm12, xmm15, 8; \
	\
	/* ; Finish folding and clear second qword */ \
	mov     T0, 0xfd; \
	kmovq   k1, T0; \
	vpaddq  xmm13{k1}{z}, xmm13, xmm10; \
	vpaddq  xmm14{k1}{z}, xmm14, xmm11; \
	vpaddq  xmm15{k1}{z}, xmm15, xmm12; \
	\
	add     MSG, POLY1305_BLOCK_SIZE*16; \
	\
	and     LEN, (POLY1305_BLOCK_SIZE*16 - 1); /* ; Get remaining lengths (LEN < 256 bytes) */ \
	\
.L_less_than_256: \
	\
	cmp     LEN, POLY1305_BLOCK_SIZE*8; \
	jb      .L_less_than_128; \
	\
	/* ; Read next 128 bytes */ \
	/* ; Load first block of data (128 bytes) */ \
	vmovdqu64 zmm0, [MSG]; \
	vmovdqu64 zmm1, [MSG + 64]; \
	\
	/* ; Interleave the data to form 44-bit limbs */ \
	/* ; */ \
	/* ; zmm13 to have bits 0-43 of all 8 blocks in 8 qwords */ \
	/* ; zmm14 to have bits 87-44 of all 8 blocks in 8 qwords */ \
	/* ; zmm15 to have bits 127-88 of all 8 blocks in 8 qwords */ \
	vpunpckhqdq zmm5, zmm0, zmm1; \
	vpunpcklqdq zmm3, zmm0, zmm1; \
	\
	vpsrlq  zmm4, zmm3, 44; \
	vpsllq  zmm8, zmm5, 20; \
	vpternlogq zmm4, zmm8, [.Lmask_44 ADD_RIP], 0xA8; /* ; (A OR B AND C) */ \
	\
	vpandq  zmm3, zmm3, [.Lmask_44 ADD_RIP]; \
	vpsrlq  zmm5, zmm5, 24; \
	\
	/* ; Add 2^128 to all 8 final qwords of the message */ \
	vporq   zmm5, zmm5, [.Lhigh_bit ADD_RIP]; \
	\
	vpaddq  zmm13, zmm13, zmm3; \
	vpaddq  zmm14, zmm14, zmm4; \
	vpaddq  zmm15, zmm15, zmm5; \
	\
	add     MSG, POLY1305_BLOCK_SIZE*8; \
	sub     LEN, POLY1305_BLOCK_SIZE*8; \
	\
	POLY1305_MUL_REDUCE_VEC(zmm13, zmm14, zmm15, \
				zmm22, zmm23, zmm24, \
				zmm25, zmm26, \
				zmm5, zmm6, zmm7, zmm8, zmm9, zmm10, \
				zmm11); \
	\
	/* ;; Add all blocks (horizontally) */ \
	vextracti64x4   ymm0, zmm13, 1; \
	vextracti64x4   ymm1, zmm14, 1; \
	vextracti64x4   ymm2, zmm15, 1; \
	\
	vpaddq  ymm13, ymm13, ymm0; \
	vpaddq  ymm14, ymm14, ymm1; \
	vpaddq  ymm15, ymm15, ymm2; \
	\
	vextracti32x4   xmm10, ymm13, 1; \
	vextracti32x4   xmm11, ymm14, 1; \
	vextracti32x4   xmm12, ymm15, 1; \
	\
	vpaddq  xmm13, xmm13, xmm10; \
	vpaddq  xmm14, xmm14, xmm11; \
	vpaddq  xmm15, xmm15, xmm12; \
	\
	vpsrldq xmm10, xmm13, 8; \
	vpsrldq xmm11, xmm14, 8; \
	vpsrldq xmm12, xmm15, 8; \
	\
	/* ; Finish folding and clear second qword */ \
	mov     T0, 0xfd; \
	kmovq   k1, T0; \
	vpaddq  xmm13{k1}{z}, xmm13, xmm10; \
	vpaddq  xmm14{k1}{z}, xmm14, xmm11; \
	vpaddq  xmm15{k1}{z}, xmm15, xmm12; \
	\
.L_less_than_128: \
	cmp     LEN, 32; /* ; If remaining bytes is <= 32, perform last blocks in scalar */ \
	jbe     .L_simd_to_gp; \
	\
	mov     T0, LEN; \
	and     T0, 0x3f; \
	lea     T1, [.Lbyte64_len_to_mask_table ADD_RIP]; \
	mov     T1, [T1 + 8*T0]; \
	\
	/* ; Load default byte masks */ \
	mov     T2, 0xffffffffffffffff; \
	xor     T3, T3; \
	\
	cmp     LEN, 64; \
	cmovb   T2, T1; /* ; Load mask for first 64 bytes */ \
	cmovg   T3, T1; /* ; Load mask for second 64 bytes */ \
	\
	kmovq   k1, T2; \
	kmovq   k2, T3; \
	vmovdqu8 zmm0{k1}{z}, [MSG]; \
	vmovdqu8 zmm1{k2}{z}, [MSG + 64]; \
	\
	/* ; Pad last block message, if partial */ \
	mov     T0, LEN; \
	and     T0, 0x70; /* ; Multiple of 16 bytes */ \
	/* ; Load last block of data (up to 112 bytes) */ \
	shr     T0, 3; /* ; Get number of full qwords */ \
	\
	/* ; Interleave the data to form 44-bit limbs */ \
	/* ; */ \
	/* ; zmm13 to have bits 0-43 of all 8 blocks in 8 qwords */ \
	/* ; zmm14 to have bits 87-44 of all 8 blocks in 8 qwords */ \
	/* ; zmm15 to have bits 127-88 of all 8 blocks in 8 qwords */ \
	vpunpckhqdq zmm4, zmm0, zmm1; \
	vpunpcklqdq zmm2, zmm0, zmm1; \
	\
	vpsrlq  zmm3, zmm2, 44; \
	vpsllq  zmm28, zmm4, 20; \
	vpternlogq zmm3, zmm28, [.Lmask_44 ADD_RIP], 0xA8; /* ; (A OR B AND C) */ \
	\
	vpandq  zmm2, zmm2, [.Lmask_44 ADD_RIP]; \
	vpsrlq  zmm4, zmm4, 24; \
	\
	lea     T1, [.Lqword_high_bit_mask ADD_RIP]; \
	kmovb   k1, [T1 + T0]; \
	/* ; Add 2^128 to final qwords of the message (all full blocks and partial block, */ \
	/* ; if "pad_to_16" is selected) */ \
	vporq   zmm4{k1}, zmm4, [.Lhigh_bit ADD_RIP]; \
	\
	vpaddq  zmm13, zmm13, zmm2; \
	vpaddq  zmm14, zmm14, zmm3; \
	vpaddq  zmm15, zmm15, zmm4; \
	\
	mov     T0, LEN; \
	add     T0, 15; \
	shr     T0, 4;      /* ; Get number of 16-byte blocks (including partial blocks) */ \
	xor     LEN, LEN; /* ; All length will be consumed */ \
	\
	/* ; No need to shuffle data blocks (data is in the right order) */ \
	cmp     T0, 8; \
	je      .L_end_shuffle; \
	\
	cmp     T0, 4; \
	je      .L_shuffle_blocks_4; \
	jb      .L_shuffle_blocks_3; \
	\
	/* ; Number of 16-byte blocks > 4 */ \
	cmp     T0, 6; \
	je      .L_shuffle_blocks_6; \
	ja      .L_shuffle_blocks_7; \
	jmp     .L_shuffle_blocks_5; \
	\
.L_shuffle_blocks_3: \
	SHUFFLE_DATA_BLOCKS_3(zmm13, zmm14, zmm15, T1); \
	jmp     .L_end_shuffle; \
.L_shuffle_blocks_4: \
	SHUFFLE_DATA_BLOCKS_4(zmm13, zmm14, zmm15, T1); \
	jmp     .L_end_shuffle; \
.L_shuffle_blocks_5: \
	SHUFFLE_DATA_BLOCKS_5(zmm13, zmm14, zmm15, T1); \
	jmp     .L_end_shuffle; \
.L_shuffle_blocks_6: \
	SHUFFLE_DATA_BLOCKS_6(zmm13, zmm14, zmm15, T1); \
	jmp     .L_end_shuffle; \
.L_shuffle_blocks_7: \
	SHUFFLE_DATA_BLOCKS_7(zmm13, zmm14, zmm15, T1); \
	\
.L_end_shuffle: \
	\
	/* ; zmm13-zmm15 contain the 8 blocks of message plus the previous accumulator */ \
	/* ; zmm22-24 contain the 3x44-bit limbs of the powers of R */ \
	/* ; zmm25-26 contain the 3x44-bit limbs of the powers of R' (5*4*R) */ \
	POLY1305_MUL_REDUCE_VEC(zmm13, zmm14, zmm15, \
				zmm22, zmm23, zmm24, \
				zmm25, zmm26, \
				zmm5, zmm6, zmm7, zmm8, zmm9, zmm10, \
				zmm11); \
	\
	/* ;; Add all blocks (horizontally) */ \
	vextracti64x4   ymm0, zmm13, 1; \
	vextracti64x4   ymm1, zmm14, 1; \
	vextracti64x4   ymm2, zmm15, 1; \
	\
	vpaddq  ymm13, ymm13, ymm0; \
	vpaddq  ymm14, ymm14, ymm1; \
	vpaddq  ymm15, ymm15, ymm2; \
	\
	vextracti32x4   xmm10, ymm13, 1; \
	vextracti32x4   xmm11, ymm14, 1; \
	vextracti32x4   xmm12, ymm15, 1; \
	\
	vpaddq  xmm13, xmm13, xmm10; \
	vpaddq  xmm14, xmm14, xmm11; \
	vpaddq  xmm15, xmm15, xmm12; \
	\
	vpsrldq xmm10, xmm13, 8; \
	vpsrldq xmm11, xmm14, 8; \
	vpsrldq xmm12, xmm15, 8; \
	\
	vpaddq  xmm13, xmm13, xmm10; \
	vpaddq  xmm14, xmm14, xmm11; \
	vpaddq  xmm15, xmm15, xmm12; \
	\
.L_simd_to_gp: \
	/* ; Carry propagation */ \
	vpsrlq  xmm0, xmm13, 44; \
	vpandq  xmm13, xmm13, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpaddq  xmm14, xmm14, xmm0; \
	vpsrlq  xmm0, xmm14, 44; \
	vpandq  xmm14, xmm14, [.Lmask_44 ADD_RIP]; /* ; Clear top 20 bits */ \
	vpaddq  xmm15, xmm15, xmm0; \
	vpsrlq  xmm0, xmm15, 42; \
	vpandq  xmm15, xmm15, [.Lmask_42 ADD_RIP]; /* ; Clear top 22 bits */ \
	vpsllq  xmm1, xmm0, 2; \
	vpaddq  xmm0, xmm0, xmm1; \
	vpaddq  xmm13, xmm13, xmm0; \
	\
	/* ; Put together A */ \
	vmovq   A0, xmm13; \
	\
	vmovq   T0, xmm14; \
	mov     T1, T0; \
	shl     T1, 44; \
	or      A0, T1; \
	\
	shr     T0, 20; \
	vmovq   A2, xmm15; \
	mov     A1, A2; \
	shl     A1, 24; \
	or      A1, T0; \
	shr     A2, 40; \
	\
	/* ; Clear powers of R */ \
	vpxorq  zmm0, zmm0, zmm0; \
	vmovdqa64 [rsp + STACK_r_save], zmm0; \
	vmovdqa64 [rsp + STACK_r_save + 64], zmm0; \
	vmovdqa64 [rsp + STACK_r_save + 64*2], zmm0; \
	vmovdqa64 [rsp + STACK_r_save + 64*3], zmm0; \
	vmovdqa64 [rsp + STACK_r_save + 64*4], zmm0; \
	vmovdqa64 [rsp + STACK_r_save + 64*5], zmm0; \
	\
	vzeroall; \
	clear_zmm(ymm16); clear_zmm(ymm20); clear_zmm(ymm24); clear_zmm(ymm28); \
	clear_zmm(ymm17); clear_zmm(ymm21); clear_zmm(ymm25); clear_zmm(ymm29); \
	clear_zmm(ymm18); clear_zmm(ymm22); clear_zmm(ymm26); clear_zmm(ymm30); \
	clear_zmm(ymm19); clear_zmm(ymm23); clear_zmm(ymm27); clear_zmm(ymm31); \
	\
.L_final_loop: \
	cmp     LEN, POLY1305_BLOCK_SIZE; \
	jb      .L_poly1305_blocks_exit; \
	\
	/* ;; A += MSG[i] */ \
	add     A0, [MSG + 0]; \
	adc     A1, [MSG + 8]; \
	adc     A2, 1; /* ;; no padding bit */ \
	\
	mov     T0, R1; \
	shr     T0, 2; \
	add     T0, R1; /* ;; T0 = R1 + (R1 >> 2) */ \
	\
	POLY1305_MUL_REDUCE(A0, A1, A2, R0, R1, \
			    T0, T1, T2, T3, GP_RAX, GP_RDX, A2_NOT_ZERO); \
	\
	add     MSG, POLY1305_BLOCK_SIZE; \
	sub     LEN, POLY1305_BLOCK_SIZE; \
	\
	jmp     .L_final_loop; \
	\
.L_len_256_511: \
	\
	/* ; zmm13-zmm15 contain the 8 blocks of message plus the previous accumulator */ \
	/* ; zmm22-24 contain the 3x44-bit limbs of the powers of R */ \
	/* ; zmm25-26 contain the 3x44-bit limbs of the powers of R' (5*4*R) */ \
	POLY1305_MUL_REDUCE_VEC(zmm13, zmm14, zmm15, \
				zmm22, zmm23, zmm24, \
				zmm25, zmm26, \
				zmm5, zmm6, zmm7, zmm8, zmm9, zmm10, \
				zmm11); \
	\
	/* ; Then multiply by r^8-r */ \
	\
	/* ; zmm19-zmm21 contains R^8-R, need to move it to zmm22-24, */ \
	/* ; as it might be used in other part of the code */ \
	vmovdqa64 zmm22, zmm19; \
	vmovdqa64 zmm23, zmm20; \
	vmovdqa64 zmm24, zmm21; \
	\
	/* ; zmm25 to have bits 87-44 of all 8 powers of R' in 8 qwords */ \
	/* ; zmm26 to have bits 129-88 of all 8 powers of R' in 8 qwords */ \
	vpsllq  zmm0, zmm23, 2; \
	vpaddq  zmm25, zmm23, zmm0; /* ; R1' (R1*5) */ \
	vpsllq  zmm1, zmm24, 2; \
	vpaddq  zmm26, zmm24, zmm1; /* ; R2' (R2*5) */ \
	\
	/* ; 4*5*R^8 */ \
	vpsllq  zmm25, zmm25, 2; \
	vpsllq  zmm26, zmm26, 2; \
	\
	vpaddq  zmm13, zmm13, zmm16; \
	vpaddq  zmm14, zmm14, zmm17; \
	vpaddq  zmm15, zmm15, zmm18; \
	\
	/* ; zmm13-zmm15 contain the 8 blocks of message plus the previous accumulator */ \
	/* ; zmm22-24 contain the 3x44-bit limbs of the powers of R */ \
	/* ; zmm25-26 contain the 3x44-bit limbs of the powers of R' (5*4*R) */ \
	POLY1305_MUL_REDUCE_VEC(zmm13, zmm14, zmm15, \
				zmm22, zmm23, zmm24, \
				zmm25, zmm26, \
				zmm5, zmm6, zmm7, zmm8, zmm9, zmm10, \
				zmm11); \
	\
	/* ;; Add all blocks (horizontally) */ \
	vextracti64x4   ymm0, zmm13, 1; \
	vextracti64x4   ymm1, zmm14, 1; \
	vextracti64x4   ymm2, zmm15, 1; \
	\
	vpaddq  ymm13, ymm13, ymm0; \
	vpaddq  ymm14, ymm14, ymm1; \
	vpaddq  ymm15, ymm15, ymm2; \
	\
	vextracti32x4   xmm10, ymm13, 1; \
	vextracti32x4   xmm11, ymm14, 1; \
	vextracti32x4   xmm12, ymm15, 1; \
	\
	vpaddq  xmm13, xmm13, xmm10; \
	vpaddq  xmm14, xmm14, xmm11; \
	vpaddq  xmm15, xmm15, xmm12; \
	\
	vpsrldq xmm10, xmm13, 8; \
	vpsrldq xmm11, xmm14, 8; \
	vpsrldq xmm12, xmm15, 8; \
	\
	/* ; Finish folding and clear second qword */ \
	mov     T0, 0xfd; \
	kmovq   k1, T0; \
	vpaddq  xmm13{k1}{z}, xmm13, xmm10; \
	vpaddq  xmm14{k1}{z}, xmm14, xmm11; \
	vpaddq  xmm15{k1}{z}, xmm15, xmm12; \
	\
	add     MSG, POLY1305_BLOCK_SIZE*16; \
	sub     LEN, POLY1305_BLOCK_SIZE*16; \
	\
	jmp     .L_less_than_256; \
.L_poly1305_blocks_exit: \

/*
;; =============================================================================
;; =============================================================================
;; Creates stack frame and saves registers
;; =============================================================================
*/
#define FUNC_ENTRY() \
	mov     rax, rsp; \
	CFI_DEF_CFA_REGISTER(rax); \
	sub     rsp, STACK_SIZE; \
	and	rsp, -64; \
	\
	mov     [rsp + STACK_gpr_save + 8*0], rbx; \
	mov     [rsp + STACK_gpr_save + 8*1], rbp; \
	mov     [rsp + STACK_gpr_save + 8*2], r12; \
	mov     [rsp + STACK_gpr_save + 8*3], r13; \
	mov     [rsp + STACK_gpr_save + 8*4], r14; \
	mov     [rsp + STACK_gpr_save + 8*5], r15; \
	mov     [rsp + STACK_rsp_save], rax; \
	CFI_CFA_ON_STACK(STACK_rsp_save, 0)

/*
;; =============================================================================
;; =============================================================================
;; Restores registers and removes the stack frame
;; =============================================================================
*/
#define FUNC_EXIT() \
	mov     rbx, [rsp + STACK_gpr_save + 8*0]; \
	mov     rbp, [rsp + STACK_gpr_save + 8*1]; \
	mov     r12, [rsp + STACK_gpr_save + 8*2]; \
	mov     r13, [rsp + STACK_gpr_save + 8*3]; \
	mov     r14, [rsp + STACK_gpr_save + 8*4]; \
	mov     r15, [rsp + STACK_gpr_save + 8*5]; \
	mov     rsp, [rsp + STACK_rsp_save]; \
	CFI_DEF_CFA_REGISTER(rsp)

.text

/*
;; =============================================================================
;; =============================================================================
;; void poly1305_aead_update_fma_avx512(const void *msg, const uint64_t msg_len,
;;                                      void *hash, const void *key)
;; arg1 - Input message
;; arg2 - Message length
;; arg3 - Input/output hash
;; arg4 - Poly1305 key
*/
.align 32
.globl _gcry_poly1305_amd64_avx512_blocks
ELF(.type _gcry_poly1305_amd64_avx512_blocks,@function;)
_gcry_poly1305_amd64_avx512_blocks:
	CFI_STARTPROC()
	spec_stop_avx512_intel_syntax;
	FUNC_ENTRY()

#define _a0 gp3
#define _a0 gp3
#define _a1 gp4
#define _a2 gp5
#define _r0 gp6
#define _r1 gp7
#define _len arg2
#define _arg3 arg4             /* ; use rcx, arg3 = rdx */

	/* ;; load R */
	mov     _r0, [arg4 + 0 * 8]
	mov     _r1, [arg4 + 1 * 8]

	/* ;; load accumulator / current hash value */
	/* ;; note: arg4 can't be used beyond this point */
	mov     _arg3, arg3             /* ; note: _arg3 = arg4 (linux) */
	mov     _a0, [_arg3 + 0 * 8]
	mov     _a1, [_arg3 + 1 * 8]
	mov     DWORD(_a2), [_arg3 + 2 * 8]    /* ; note: _a2 = arg4 (win) */

	POLY1305_BLOCKS(arg1, _len, _a0, _a1, _a2, _r0, _r1,
			gp10, gp11, gp8, gp9, rax, rdx)

	/* ;; save accumulator back */
	mov     [_arg3 + 0 * 8], _a0
	mov     [_arg3 + 1 * 8], _a1
	mov     [_arg3 + 2 * 8], DWORD(_a2)

	FUNC_EXIT()
	xor eax, eax
	kxorw k1, k1, k1
	kxorw k2, k2, k2
	ret_spec_stop
	CFI_ENDPROC()
ELF(.size _gcry_poly1305_amd64_avx512_blocks,
	  .-_gcry_poly1305_amd64_avx512_blocks;)

#endif
#endif
