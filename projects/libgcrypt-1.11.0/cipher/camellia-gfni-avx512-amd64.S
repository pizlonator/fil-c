/* camellia-gfni-avx512-amd64.S - GFNI/AVX512 implementation of Camellia
 *
 * Copyright (C) 2022-2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#ifdef __x86_64
#if (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS)) && \
    defined(ENABLE_GFNI_SUPPORT) && defined(ENABLE_AVX512_SUPPORT)

#include "asm-common-amd64.h"

#define CAMELLIA_TABLE_BYTE_LEN 272

/* struct CAMELLIA_context: */
#define key_table 0
#define key_bitlength CAMELLIA_TABLE_BYTE_LEN

/* register macros */
#define CTX %rdi
#define RIO %r8

/**********************************************************************
  helper macros
 **********************************************************************/

#define zmm0_x xmm0
#define zmm1_x xmm1
#define zmm2_x xmm2
#define zmm3_x xmm3
#define zmm4_x xmm4
#define zmm5_x xmm5
#define zmm6_x xmm6
#define zmm7_x xmm7
#define zmm8_x xmm8
#define zmm9_x xmm9
#define zmm10_x xmm10
#define zmm11_x xmm11
#define zmm12_x xmm12
#define zmm13_x xmm13
#define zmm14_x xmm14
#define zmm15_x xmm15

#define zmm0_y ymm0
#define zmm1_y ymm1
#define zmm2_y ymm2
#define zmm3_y ymm3
#define zmm4_y ymm4
#define zmm5_y ymm5
#define zmm6_y ymm6
#define zmm7_y ymm7
#define zmm8_y ymm8
#define zmm9_y ymm9
#define zmm10_y ymm10
#define zmm11_y ymm11
#define zmm12_y ymm12
#define zmm13_y ymm13
#define zmm14_y ymm14
#define zmm15_y ymm15

#define mem_ab_0 %zmm16
#define mem_ab_1 %zmm17
#define mem_ab_2 %zmm31
#define mem_ab_3 %zmm18
#define mem_ab_4 %zmm19
#define mem_ab_5 %zmm20
#define mem_ab_6 %zmm21
#define mem_ab_7 %zmm22
#define mem_cd_0 %zmm23
#define mem_cd_1 %zmm24
#define mem_cd_2 %zmm30
#define mem_cd_3 %zmm25
#define mem_cd_4 %zmm26
#define mem_cd_5 %zmm27
#define mem_cd_6 %zmm28
#define mem_cd_7 %zmm29

#define clear_vec4(v0,v1,v2,v3) \
	vpxord v0, v0, v0; \
	vpxord v1, v1, v1; \
	vpxord v2, v2, v2; \
	vpxord v3, v3, v3

#define clear_zmm16_zmm31() \
	clear_vec4(%ymm16, %ymm20, %ymm24, %ymm28); \
	clear_vec4(%ymm17, %ymm21, %ymm25, %ymm29); \
	clear_vec4(%ymm18, %ymm22, %ymm26, %ymm30); \
	clear_vec4(%ymm19, %ymm23, %ymm27, %ymm31)

#define clear_regs() \
	vzeroall; \
	clear_zmm16_zmm31()

/**********************************************************************
  GFNI helper macros and constants
 **********************************************************************/

#define BV8(a0,a1,a2,a3,a4,a5,a6,a7) \
	( (((a0) & 1) << 0) | \
	  (((a1) & 1) << 1) | \
	  (((a2) & 1) << 2) | \
	  (((a3) & 1) << 3) | \
	  (((a4) & 1) << 4) | \
	  (((a5) & 1) << 5) | \
	  (((a6) & 1) << 6) | \
	  (((a7) & 1) << 7) )

#define BM8X8(l0,l1,l2,l3,l4,l5,l6,l7) \
	( ((l7) << (0 * 8)) | \
	  ((l6) << (1 * 8)) | \
	  ((l5) << (2 * 8)) | \
	  ((l4) << (3 * 8)) | \
	  ((l3) << (4 * 8)) | \
	  ((l2) << (5 * 8)) | \
	  ((l1) << (6 * 8)) | \
	  ((l0) << (7 * 8)) )

/* Pre-filters and post-filters constants for Camellia sboxes s1, s2, s3 and s4.
 *   See http://urn.fi/URN:NBN:fi:oulu-201305311409, pages 43-48.
 *
 * Pre-filters are directly from above source, "θ₁"/"θ₄". Post-filters are
 * combination of function "A" (AES SubBytes affine transformation) and
 * "ψ₁"/"ψ₂"/"ψ₃".
 */

/* Constant from "θ₁(x)" and "θ₄(x)" functions. */
#define pre_filter_constant_s1234 BV8(1, 0, 1, 0, 0, 0, 1, 0)

/* Constant from "ψ₁(A(x))" function: */
#define post_filter_constant_s14  BV8(0, 1, 1, 1, 0, 1, 1, 0)

/* Constant from "ψ₂(A(x))" function: */
#define post_filter_constant_s2   BV8(0, 0, 1, 1, 1, 0, 1, 1)

/* Constant from "ψ₃(A(x))" function: */
#define post_filter_constant_s3   BV8(1, 1, 1, 0, 1, 1, 0, 0)

/**********************************************************************
  64-way parallel camellia
 **********************************************************************/

/* roundsm64 (GFNI/AVX512 version)
 * IN:
 *   x0..x7: byte-sliced AB state
 *   mem_cd: register pointer storing CD state
 *   key: index for key material
 * OUT:
 *   x0..x7: new byte-sliced CD state
 */
#define roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, t0, t1, t2, t3, t4, t5, \
		  t6, t7, mem_cd, key) \
	/* \
	 * S-function with AES subbytes \
	 */ \
	vpbroadcastq .Lpre_filter_bitmatrix_s123 rRIP, t5; \
	vpbroadcastq .Lpre_filter_bitmatrix_s4 rRIP, t2; \
	vpbroadcastq .Lpost_filter_bitmatrix_s14 rRIP, t4; \
	vpbroadcastq .Lpost_filter_bitmatrix_s2 rRIP, t3; \
	vpbroadcastq .Lpost_filter_bitmatrix_s3 rRIP, t6; \
	\
	/* prefilter sboxes */ \
	vgf2p8affineqb $(pre_filter_constant_s1234), t5, x0, x0; \
	vgf2p8affineqb $(pre_filter_constant_s1234), t5, x7, x7; \
	vgf2p8affineqb $(pre_filter_constant_s1234), t2, x3, x3; \
	vgf2p8affineqb $(pre_filter_constant_s1234), t2, x6, x6; \
	vgf2p8affineqb $(pre_filter_constant_s1234), t5, x2, x2; \
	vgf2p8affineqb $(pre_filter_constant_s1234), t5, x5, x5; \
	vgf2p8affineqb $(pre_filter_constant_s1234), t5, x1, x1; \
	vgf2p8affineqb $(pre_filter_constant_s1234), t5, x4, x4; \
	\
	/* sbox GF8 inverse + postfilter sboxes 1 and 4 */ \
	vgf2p8affineinvqb $(post_filter_constant_s14), t4, x0, x0; \
	vgf2p8affineinvqb $(post_filter_constant_s14), t4, x7, x7; \
	vgf2p8affineinvqb $(post_filter_constant_s14), t4, x3, x3; \
	vgf2p8affineinvqb $(post_filter_constant_s14), t4, x6, x6; \
	\
	/* sbox GF8 inverse + postfilter sbox 3 */ \
	vgf2p8affineinvqb $(post_filter_constant_s3), t6, x2, x2; \
	vgf2p8affineinvqb $(post_filter_constant_s3), t6, x5, x5; \
	\
	/* sbox GF8 inverse + postfilter sbox 2 */ \
	vgf2p8affineinvqb $(post_filter_constant_s2), t3, x1, x1; \
	vgf2p8affineinvqb $(post_filter_constant_s2), t3, x4, x4; \
	\
	vpbroadcastb 7+key, t7; \
	vpbroadcastb 6+key, t6; \
	\
	/* P-function */ \
	vpxorq x5, x0, x0; \
	vpxorq x6, x1, x1; \
	vpxorq x7, x2, x2; \
	vpxorq x4, x3, x3; \
	\
	vpbroadcastb 5+key, t5; \
	vpbroadcastb 4+key, t4; \
	\
	vpxorq x2, x4, x4; \
	vpxorq x3, x5, x5; \
	vpxorq x0, x6, x6; \
	vpxorq x1, x7, x7; \
	\
	vpbroadcastb 3+key, t3; \
	vpbroadcastb 2+key, t2; \
	\
	vpxorq x7, x0, x0; \
	vpxorq x4, x1, x1; \
	vpxorq x5, x2, x2; \
	vpxorq x6, x3, x3; \
	\
	vpbroadcastb 1+key, t1; \
	vpbroadcastb 0+key, t0; \
	\
	vpxorq x3, x4, x4; \
	vpxorq x0, x5, x5; \
	vpxorq x1, x6, x6; \
	vpxorq x2, x7, x7; /* note: high and low parts swapped */ \
	\
	/* Add key material and result to CD (x becomes new CD) */ \
	\
	vpternlogq $0x96, mem_cd##_4, t7, x0; \
	vpternlogq $0x96, mem_cd##_5, t6, x1; \
	vpternlogq $0x96, mem_cd##_6, t5, x2; \
	vpternlogq $0x96, mem_cd##_7, t4, x3; \
	vpternlogq $0x96, mem_cd##_0, t3, x4; \
	vpternlogq $0x96, mem_cd##_1, t2, x5; \
	vpternlogq $0x96, mem_cd##_2, t1, x6; \
	vpternlogq $0x96, mem_cd##_3, t0, x7;

/*
 * IN/OUT:
 *  x0..x7: byte-sliced AB state preloaded
 *  mem_ab: byte-sliced AB state in memory
 *  mem_cb: byte-sliced CD state in memory
 */
#define two_roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i, dir, store_ab) \
	roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		  y6, y7, mem_cd, (key_table + (i) * 8)(CTX)); \
	\
	vmovdqu64 x0, mem_cd##_4; \
	vmovdqu64 x1, mem_cd##_5; \
	vmovdqu64 x2, mem_cd##_6; \
	vmovdqu64 x3, mem_cd##_7; \
	vmovdqu64 x4, mem_cd##_0; \
	vmovdqu64 x5, mem_cd##_1; \
	vmovdqu64 x6, mem_cd##_2; \
	vmovdqu64 x7, mem_cd##_3; \
	\
	roundsm64(x4, x5, x6, x7, x0, x1, x2, x3, y0, y1, y2, y3, y4, y5, \
		  y6, y7, mem_ab, (key_table + ((i) + (dir)) * 8)(CTX)); \
	\
	store_ab(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab);

#define dummy_store(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab) /* do nothing */

#define store_ab_state(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab) \
	/* Store new AB state */ \
	vmovdqu64 x4, mem_ab##_4; \
	vmovdqu64 x5, mem_ab##_5; \
	vmovdqu64 x6, mem_ab##_6; \
	vmovdqu64 x7, mem_ab##_7; \
	vmovdqu64 x0, mem_ab##_0; \
	vmovdqu64 x1, mem_ab##_1; \
	vmovdqu64 x2, mem_ab##_2; \
	vmovdqu64 x3, mem_ab##_3;

#define enc_rounds64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i) \
	two_roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 2, 1, store_ab_state); \
	two_roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 4, 1, store_ab_state); \
	two_roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 6, 1, dummy_store);

#define dec_rounds64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i) \
	two_roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 7, -1, store_ab_state); \
	two_roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 5, -1, store_ab_state); \
	two_roundsm64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 3, -1, dummy_store);

/*
 * IN:
 *  v0..3: byte-sliced 32-bit integers
 * OUT:
 *  v0..3: (IN << 1)
 *  t0, t1, t2, zero: (IN >> 7)
 */
#define rol32_1_64(v0, v1, v2, v3, t0, t1, t2, t3, right_shift_by_7) \
	vgf2p8affineqb $0, right_shift_by_7, v0, t0; \
	vpaddb v0, v0, v0; \
	\
	vgf2p8affineqb $0, right_shift_by_7, v1, t1; \
	vpaddb v1, v1, v1; \
	\
	vgf2p8affineqb $0, right_shift_by_7, v2, t2; \
	vpaddb v2, v2, v2; \
	\
	vgf2p8affineqb $0, right_shift_by_7, v3, t3; \
	vpaddb v3, v3, v3;

/*
 * IN:
 *   r: byte-sliced AB state in memory
 *   l: byte-sliced CD state in memory
 * OUT:
 *   x0..x7: new byte-sliced CD state
 */
#define fls64(l, l0, l1, l2, l3, l4, l5, l6, l7, r, t0, t1, t2, t3, tt0, \
	      tt1, tt2, tt3, kll, klr, krl, krr, tmp) \
	/* \
	 * t0 = kll; \
	 * t0 &= ll; \
	 * lr ^= rol32(t0, 1); \
	 */ \
	vpbroadcastq .Lright_shift_by_7 rRIP, tmp; \
	vpbroadcastb 0+kll, t3; \
	vpbroadcastb 1+kll, t2; \
	vpbroadcastb 2+kll, t1; \
	vpbroadcastb 3+kll, t0; \
	\
	vpandq l0, t0, t0; \
	vpandq l1, t1, t1; \
	vpandq l2, t2, t2; \
	vpandq l3, t3, t3; \
	\
	rol32_1_64(t3, t2, t1, t0, tt0, tt1, tt2, tt3, tmp); \
	\
	vpternlogq $0x96, tt2, t0, l4; \
	vmovdqu64 l4, l##_4; \
	vpternlogq $0x96, tt1, t1, l5; \
	vmovdqu64 l5, l##_5; \
	vpternlogq $0x96, tt0, t2, l6; \
	vmovdqu64 l6, l##_6; \
	vpternlogq $0x96, tt3, t3, l7; \
	vmovdqu64 l7, l##_7; \
	\
	/* \
	 * t2 = krr; \
	 * t2 |= rr; \
	 * rl ^= t2; \
	 */ \
	\
	vpbroadcastb 0+krr, t3; \
	vpbroadcastb 1+krr, t2; \
	vpbroadcastb 2+krr, t1; \
	vpbroadcastb 3+krr, t0; \
	\
	vpternlogq $0x1e, r##_4, t0, r##_0; \
	vpternlogq $0x1e, r##_5, t1, r##_1; \
	vpternlogq $0x1e, r##_6, t2, r##_2; \
	vpternlogq $0x1e, r##_7, t3, r##_3; \
	\
	/* \
	 * t2 = krl; \
	 * t2 &= rl; \
	 * rr ^= rol32(t2, 1); \
	 */ \
	vpbroadcastb 0+krl, t3; \
	vpbroadcastb 1+krl, t2; \
	vpbroadcastb 2+krl, t1; \
	vpbroadcastb 3+krl, t0; \
	\
	vpandq r##_0, t0, t0; \
	vpandq r##_1, t1, t1; \
	vpandq r##_2, t2, t2; \
	vpandq r##_3, t3, t3; \
	\
	rol32_1_64(t3, t2, t1, t0, tt0, tt1, tt2, tt3, tmp); \
	\
	vpternlogq $0x96, tt2, t0, r##_4; \
	vpternlogq $0x96, tt1, t1, r##_5; \
	vpternlogq $0x96, tt0, t2, r##_6; \
	vpternlogq $0x96, tt3, t3, r##_7; \
	\
	/* \
	 * t0 = klr; \
	 * t0 |= lr; \
	 * ll ^= t0; \
	 */ \
	\
	vpbroadcastb 0+klr, t3; \
	vpbroadcastb 1+klr, t2; \
	vpbroadcastb 2+klr, t1; \
	vpbroadcastb 3+klr, t0; \
	\
	vpternlogq $0x1e, l4, t0, l0; \
	vmovdqu64 l0, l##_0; \
	vpternlogq $0x1e, l5, t1, l1; \
	vmovdqu64 l1, l##_1; \
	vpternlogq $0x1e, l6, t2, l2; \
	vmovdqu64 l2, l##_2; \
	vpternlogq $0x1e, l7, t3, l3; \
	vmovdqu64 l3, l##_3;

#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
	vpunpckhdq x1, x0, t2; \
	vpunpckldq x1, x0, x0; \
	\
	vpunpckldq x3, x2, t1; \
	vpunpckhdq x3, x2, x2; \
	\
	vpunpckhqdq t1, x0, x1; \
	vpunpcklqdq t1, x0, x0; \
	\
	vpunpckhqdq x2, t2, x3; \
	vpunpcklqdq x2, t2, x2;

#define byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, \
			      a3, b3, c3, d3, st0, st1) \
	transpose_4x4(a0, a1, a2, a3, st0, st1); \
	transpose_4x4(b0, b1, b2, b3, st0, st1); \
	\
	transpose_4x4(c0, c1, c2, c3, st0, st1); \
	transpose_4x4(d0, d1, d2, d3, st0, st1); \
	\
	vbroadcasti64x2 .Lshufb_16x16b rRIP, st0; \
	vpshufb st0, a0, a0; \
	vpshufb st0, a1, a1; \
	vpshufb st0, a2, a2; \
	vpshufb st0, a3, a3; \
	vpshufb st0, b0, b0; \
	vpshufb st0, b1, b1; \
	vpshufb st0, b2, b2; \
	vpshufb st0, b3, b3; \
	vpshufb st0, c0, c0; \
	vpshufb st0, c1, c1; \
	vpshufb st0, c2, c2; \
	vpshufb st0, c3, c3; \
	vpshufb st0, d0, d0; \
	vpshufb st0, d1, d1; \
	vpshufb st0, d2, d2; \
	vpshufb st0, d3, d3; \
	\
	transpose_4x4(a0, b0, c0, d0, st0, st1); \
	transpose_4x4(a1, b1, c1, d1, st0, st1); \
	\
	transpose_4x4(a2, b2, c2, d2, st0, st1); \
	transpose_4x4(a3, b3, c3, d3, st0, st1); \
	/* does not adjust output bytes inside vectors */

/* load blocks to registers and apply pre-whitening */
#define inpack64_pre(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		     y6, y7, rio, key) \
	vpbroadcastq key, x0; \
	vpshufb .Lpack_bswap rRIP, x0, x0; \
	\
	vpxorq 0 * 64(rio), x0, y7; \
	vpxorq 1 * 64(rio), x0, y6; \
	vpxorq 2 * 64(rio), x0, y5; \
	vpxorq 3 * 64(rio), x0, y4; \
	vpxorq 4 * 64(rio), x0, y3; \
	vpxorq 5 * 64(rio), x0, y2; \
	vpxorq 6 * 64(rio), x0, y1; \
	vpxorq 7 * 64(rio), x0, y0; \
	vpxorq 8 * 64(rio), x0, x7; \
	vpxorq 9 * 64(rio), x0, x6; \
	vpxorq 10 * 64(rio), x0, x5; \
	vpxorq 11 * 64(rio), x0, x4; \
	vpxorq 12 * 64(rio), x0, x3; \
	vpxorq 13 * 64(rio), x0, x2; \
	vpxorq 14 * 64(rio), x0, x1; \
	vpxorq 15 * 64(rio), x0, x0;

/* byteslice pre-whitened blocks and store to temporary memory */
#define inpack64_post(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, tmp0, tmp1) \
	byteslice_16x16b_fast(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, \
			      y4, y5, y6, y7, tmp0, tmp1); \
	\
	vmovdqu64 x0, mem_ab##_0; \
	vmovdqu64 x1, mem_ab##_1; \
	vmovdqu64 x2, mem_ab##_2; \
	vmovdqu64 x3, mem_ab##_3; \
	vmovdqu64 x4, mem_ab##_4; \
	vmovdqu64 x5, mem_ab##_5; \
	vmovdqu64 x6, mem_ab##_6; \
	vmovdqu64 x7, mem_ab##_7; \
	vmovdqu64 y0, mem_cd##_0; \
	vmovdqu64 y1, mem_cd##_1; \
	vmovdqu64 y2, mem_cd##_2; \
	vmovdqu64 y3, mem_cd##_3; \
	vmovdqu64 y4, mem_cd##_4; \
	vmovdqu64 y5, mem_cd##_5; \
	vmovdqu64 y6, mem_cd##_6; \
	vmovdqu64 y7, mem_cd##_7;

/* de-byteslice, apply post-whitening and store blocks */
#define outunpack64(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, \
		    y5, y6, y7, key, tmp0, tmp1) \
	byteslice_16x16b_fast(y0, y4, x0, x4, y1, y5, x1, x5, y2, y6, x2, x6, \
			      y3, y7, x3, x7, tmp0, tmp1); \
	\
	vpbroadcastq key, tmp0; \
	vpshufb .Lpack_bswap rRIP, tmp0, tmp0; \
	\
	vpxorq tmp0, y7, y7; \
	vpxorq tmp0, y6, y6; \
	vpxorq tmp0, y5, y5; \
	vpxorq tmp0, y4, y4; \
	vpxorq tmp0, y3, y3; \
	vpxorq tmp0, y2, y2; \
	vpxorq tmp0, y1, y1; \
	vpxorq tmp0, y0, y0; \
	vpxorq tmp0, x7, x7; \
	vpxorq tmp0, x6, x6; \
	vpxorq tmp0, x5, x5; \
	vpxorq tmp0, x4, x4; \
	vpxorq tmp0, x3, x3; \
	vpxorq tmp0, x2, x2; \
	vpxorq tmp0, x1, x1; \
	vpxorq tmp0, x0, x0;

#define write_output(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		     y6, y7, rio) \
	vmovdqu64 x0, 0 * 64(rio); \
	vmovdqu64 x1, 1 * 64(rio); \
	vmovdqu64 x2, 2 * 64(rio); \
	vmovdqu64 x3, 3 * 64(rio); \
	vmovdqu64 x4, 4 * 64(rio); \
	vmovdqu64 x5, 5 * 64(rio); \
	vmovdqu64 x6, 6 * 64(rio); \
	vmovdqu64 x7, 7 * 64(rio); \
	vmovdqu64 y0, 8 * 64(rio); \
	vmovdqu64 y1, 9 * 64(rio); \
	vmovdqu64 y2, 10 * 64(rio); \
	vmovdqu64 y3, 11 * 64(rio); \
	vmovdqu64 y4, 12 * 64(rio); \
	vmovdqu64 y5, 13 * 64(rio); \
	vmovdqu64 y6, 14 * 64(rio); \
	vmovdqu64 y7, 15 * 64(rio);

SECTION_RODATA

#define SHUFB_BYTES(idx) \
	0 + (idx), 4 + (idx), 8 + (idx), 12 + (idx)

_gcry_camellia_gfni_avx512__constants:
ELF(.type   _gcry_camellia_gfni_avx512__constants,@object;)

.align 64
.Lpack_bswap:
	.long 0x00010203, 0x04050607, 0x80808080, 0x80808080
	.long 0x00010203, 0x04050607, 0x80808080, 0x80808080
	.long 0x00010203, 0x04050607, 0x80808080, 0x80808080
	.long 0x00010203, 0x04050607, 0x80808080, 0x80808080

.Lcounter0123_lo:
	.quad 0, 0
	.quad 1, 0
	.quad 2, 0
	.quad 3, 0

.align 16
.Lcounter4444_lo:
	.quad 4, 0
.Lcounter8888_lo:
	.quad 8, 0
.Lcounter16161616_lo:
	.quad 16, 0
.Lcounter1111_hi:
	.quad 0, 1

.Lshufb_16x16b:
	.byte SHUFB_BYTES(0), SHUFB_BYTES(1), SHUFB_BYTES(2), SHUFB_BYTES(3)

/* For CTR-mode IV byteswap */
.Lbswap128_mask:
	.byte 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

/* Pre-filters and post-filters bit-matrixes for Camellia sboxes s1, s2, s3
 * and s4.
 *   See http://urn.fi/URN:NBN:fi:oulu-201305311409, pages 43-48.
 *
 * Pre-filters are directly from above source, "θ₁"/"θ₄". Post-filters are
 * combination of function "A" (AES SubBytes affine transformation) and
 * "ψ₁"/"ψ₂"/"ψ₃".
 */

/* Bit-matrix from "θ₁(x)" function: */
.Lpre_filter_bitmatrix_s123:
	.quad BM8X8(BV8(1, 1, 1, 0, 1, 1, 0, 1),
		    BV8(0, 0, 1, 1, 0, 0, 1, 0),
		    BV8(1, 1, 0, 1, 0, 0, 0, 0),
		    BV8(1, 0, 1, 1, 0, 0, 1, 1),
		    BV8(0, 0, 0, 0, 1, 1, 0, 0),
		    BV8(1, 0, 1, 0, 0, 1, 0, 0),
		    BV8(0, 0, 1, 0, 1, 1, 0, 0),
		    BV8(1, 0, 0, 0, 0, 1, 1, 0))

/* Bit-matrix from "θ₄(x)" function: */
.Lpre_filter_bitmatrix_s4:
	.quad BM8X8(BV8(1, 1, 0, 1, 1, 0, 1, 1),
		    BV8(0, 1, 1, 0, 0, 1, 0, 0),
		    BV8(1, 0, 1, 0, 0, 0, 0, 1),
		    BV8(0, 1, 1, 0, 0, 1, 1, 1),
		    BV8(0, 0, 0, 1, 1, 0, 0, 0),
		    BV8(0, 1, 0, 0, 1, 0, 0, 1),
		    BV8(0, 1, 0, 1, 1, 0, 0, 0),
		    BV8(0, 0, 0, 0, 1, 1, 0, 1))

/* Bit-matrix from "ψ₁(A(x))" function: */
.Lpost_filter_bitmatrix_s14:
	.quad BM8X8(BV8(0, 0, 0, 0, 0, 0, 0, 1),
		    BV8(0, 1, 1, 0, 0, 1, 1, 0),
		    BV8(1, 0, 1, 1, 1, 1, 1, 0),
		    BV8(0, 0, 0, 1, 1, 0, 1, 1),
		    BV8(1, 0, 0, 0, 1, 1, 1, 0),
		    BV8(0, 1, 0, 1, 1, 1, 1, 0),
		    BV8(0, 1, 1, 1, 1, 1, 1, 1),
		    BV8(0, 0, 0, 1, 1, 1, 0, 0))

/* Bit-matrix from "ψ₂(A(x))" function: */
.Lpost_filter_bitmatrix_s2:
	.quad BM8X8(BV8(0, 0, 0, 1, 1, 1, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 1),
		    BV8(0, 1, 1, 0, 0, 1, 1, 0),
		    BV8(1, 0, 1, 1, 1, 1, 1, 0),
		    BV8(0, 0, 0, 1, 1, 0, 1, 1),
		    BV8(1, 0, 0, 0, 1, 1, 1, 0),
		    BV8(0, 1, 0, 1, 1, 1, 1, 0),
		    BV8(0, 1, 1, 1, 1, 1, 1, 1))

/* Bit-matrix from "ψ₃(A(x))" function: */
.Lpost_filter_bitmatrix_s3:
	.quad BM8X8(BV8(0, 1, 1, 0, 0, 1, 1, 0),
		    BV8(1, 0, 1, 1, 1, 1, 1, 0),
		    BV8(0, 0, 0, 1, 1, 0, 1, 1),
		    BV8(1, 0, 0, 0, 1, 1, 1, 0),
		    BV8(0, 1, 0, 1, 1, 1, 1, 0),
		    BV8(0, 1, 1, 1, 1, 1, 1, 1),
		    BV8(0, 0, 0, 1, 1, 1, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 1))

/* Bit-matrix for right shifting uint8_t values in vector by 7. */
.Lright_shift_by_7:
	.quad BM8X8(BV8(0, 0, 0, 0, 0, 0, 0, 1),
		    BV8(0, 0, 0, 0, 0, 0, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 0))

/* CTR byte addition constants */
.align 64
.Lbige_addb_0_1:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1
.Lbige_addb_2_3:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3
.Lbige_addb_4_5:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5
.Lbige_addb_6_7:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7
.Lbige_addb_8_9:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9
.Lbige_addb_10_11:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11
.Lbige_addb_12_13:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 12
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 13
.Lbige_addb_14_15:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15
.Lbige_addb_16:
	.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16

ELF(.size _gcry_camellia_gfni_avx512__constants,.-_gcry_camellia_gfni_avx512__constants;)

.text

.align 16
ELF(.type   __camellia_gfni_avx512_enc_blk64,@function;)

__camellia_gfni_avx512_enc_blk64:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%r8d: 24 for 16 byte key, 32 for larger
	 *	%zmm0..%zmm15: 64 plaintext blocks
	 * output:
	 *	%zmm0..%zmm15: 64 encrypted blocks, order swapped:
	 *       7, 8, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8
	 */
	CFI_STARTPROC();

	leaq (-8 * 8)(CTX, %r8, 8), %r8;

	inpack64_post(%zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
		      %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
		      %zmm15, mem_ab, mem_cd, %zmm30, %zmm31);

.align 8
.Lenc_loop:
	enc_rounds64(%zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
		     %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
		     %zmm15, mem_ab, mem_cd, 0);

	cmpq %r8, CTX;
	je .Lenc_done;
	leaq (8 * 8)(CTX), CTX;

	fls64(mem_ab, %zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
	      mem_cd, %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
	      %zmm15,
	      ((key_table) + 0)(CTX),
	      ((key_table) + 4)(CTX),
	      ((key_table) + 8)(CTX),
	      ((key_table) + 12)(CTX),
	      %zmm31);
	jmp .Lenc_loop;

.align 8
.Lenc_done:
	/* load CD for output */
	vmovdqu64 mem_cd_0, %zmm8;
	vmovdqu64 mem_cd_1, %zmm9;
	vmovdqu64 mem_cd_2, %zmm10;
	vmovdqu64 mem_cd_3, %zmm11;
	vmovdqu64 mem_cd_4, %zmm12;
	vmovdqu64 mem_cd_5, %zmm13;
	vmovdqu64 mem_cd_6, %zmm14;
	vmovdqu64 mem_cd_7, %zmm15;

	outunpack64(%zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
		    %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
		    %zmm15, ((key_table) + 8 * 8)(%r8), %zmm30, %zmm31);

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __camellia_gfni_avx512_enc_blk64,.-__camellia_gfni_avx512_enc_blk64;)

.align 16
ELF(.type   __camellia_gfni_avx512_dec_blk64,@function;)

__camellia_gfni_avx512_dec_blk64:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%r8d: 24 for 16 byte key, 32 for larger
	 *	%zmm0..%zmm15: 64 encrypted blocks
	 * output:
	 *	%zmm0..%zmm15: 64 plaintext blocks, order swapped:
	 *       7, 8, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8
	 */
	CFI_STARTPROC();

	movq %r8, %rcx;
	movq CTX, %r8
	leaq (-8 * 8)(CTX, %rcx, 8), CTX;

	inpack64_post(%zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
		      %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
		      %zmm15, mem_ab, mem_cd, %zmm30, %zmm31);

.align 8
.Ldec_loop:
	dec_rounds64(%zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
		     %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
		     %zmm15, mem_ab, mem_cd, 0);

	cmpq %r8, CTX;
	je .Ldec_done;

	fls64(mem_ab, %zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
	      mem_cd, %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
	      %zmm15,
	      ((key_table) + 8)(CTX),
	      ((key_table) + 12)(CTX),
	      ((key_table) + 0)(CTX),
	      ((key_table) + 4)(CTX),
	      %zmm31);

	leaq (-8 * 8)(CTX), CTX;
	jmp .Ldec_loop;

.align 8
.Ldec_done:
	/* load CD for output */
	vmovdqu64 mem_cd_0, %zmm8;
	vmovdqu64 mem_cd_1, %zmm9;
	vmovdqu64 mem_cd_2, %zmm10;
	vmovdqu64 mem_cd_3, %zmm11;
	vmovdqu64 mem_cd_4, %zmm12;
	vmovdqu64 mem_cd_5, %zmm13;
	vmovdqu64 mem_cd_6, %zmm14;
	vmovdqu64 mem_cd_7, %zmm15;

	outunpack64(%zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
		    %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
		    %zmm15, (key_table)(CTX), %zmm30, %zmm31);

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __camellia_gfni_avx512_dec_blk64,.-__camellia_gfni_avx512_dec_blk64;)

#define add_le128(out, in, lo_counter, hi_counter1) \
	vpaddq lo_counter, in, out; \
	vpcmpuq $1, lo_counter, out, %k1; \
	kaddb %k1, %k1, %k1; \
	vpaddq hi_counter1, out, out{%k1};

.align 16
.globl _gcry_camellia_gfni_avx512_ctr_enc
ELF(.type   _gcry_camellia_gfni_avx512_ctr_enc,@function;)

_gcry_camellia_gfni_avx512_ctr_enc:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (64 blocks)
	 *	%rdx: src (64 blocks)
	 *	%rcx: iv (big endian, 128bit)
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	cmpl $128, key_bitlength(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d; /* max */

	cmpb $(0x100 - 64), 15(%rcx);
	jbe .Lctr_byteadd;

	vbroadcasti64x2 .Lbswap128_mask rRIP, %zmm19;
	vmovdqa64 .Lcounter0123_lo rRIP, %zmm21;
	vbroadcasti64x2 .Lcounter4444_lo rRIP, %zmm22;
	vbroadcasti64x2 .Lcounter8888_lo rRIP, %zmm23;
	vbroadcasti64x2 .Lcounter16161616_lo rRIP, %zmm24;
	vbroadcasti64x2 .Lcounter1111_hi rRIP, %zmm25;

	/* load IV and byteswap */
	movq 8(%rcx), %r11;
	movq (%rcx), %r10;
	bswapq %r11;
	bswapq %r10;
	vbroadcasti64x2 (%rcx), %zmm0;
	vpshufb %zmm19, %zmm0, %zmm0;

	/* check need for handling 64-bit overflow and carry */
	cmpq $(0xffffffffffffffff - 64), %r11;
	ja .Lload_ctr_carry;

	/* construct IVs */
	vpaddq %zmm21, %zmm0, %zmm15;  /* +0:+1:+2:+3 */
	vpaddq %zmm22, %zmm15, %zmm14; /* +4:+5:+6:+7 */
	vpaddq %zmm23, %zmm15, %zmm13; /* +8:+9:+10:+11 */
	vpaddq %zmm23, %zmm14, %zmm12; /* +12:+13:+14:+15 */
	vpaddq %zmm24, %zmm15, %zmm11; /* +16... */
	vpaddq %zmm24, %zmm14, %zmm10; /* +20... */
	vpaddq %zmm24, %zmm13, %zmm9; /* +24... */
	vpaddq %zmm24, %zmm12, %zmm8; /* +28... */
	vpaddq %zmm24, %zmm11, %zmm7; /* +32... */
	vpaddq %zmm24, %zmm10, %zmm6; /* +36... */
	vpaddq %zmm24, %zmm9, %zmm5; /* +40... */
	vpaddq %zmm24, %zmm8, %zmm4; /* +44... */
	vpaddq %zmm24, %zmm7, %zmm3; /* +48... */
	vpaddq %zmm24, %zmm6, %zmm2; /* +52... */
	vpaddq %zmm24, %zmm5, %zmm1; /* +56... */
	vpaddq %zmm24, %zmm4, %zmm0; /* +60... */
	jmp .Lload_ctr_done;

.align 4
.Lload_ctr_carry:
	/* construct IVs */
	add_le128(%zmm15, %zmm0, %zmm21, %zmm25);  /* +0:+1:+2:+3 */
	add_le128(%zmm14, %zmm15, %zmm22, %zmm25); /* +4:+5:+6:+7 */
	add_le128(%zmm13, %zmm15, %zmm23, %zmm25); /* +8:+9:+10:+11 */
	add_le128(%zmm12, %zmm14, %zmm23, %zmm25); /* +12:+13:+14:+15 */
	add_le128(%zmm11, %zmm15, %zmm24, %zmm25); /* +16... */
	add_le128(%zmm10, %zmm14, %zmm24, %zmm25); /* +20... */
	add_le128(%zmm9, %zmm13, %zmm24, %zmm25); /* +24... */
	add_le128(%zmm8, %zmm12, %zmm24, %zmm25); /* +28... */
	add_le128(%zmm7, %zmm11, %zmm24, %zmm25); /* +32... */
	add_le128(%zmm6, %zmm10, %zmm24, %zmm25); /* +36... */
	add_le128(%zmm5, %zmm9, %zmm24, %zmm25); /* +40... */
	add_le128(%zmm4, %zmm8, %zmm24, %zmm25); /* +44... */
	add_le128(%zmm3, %zmm7, %zmm24, %zmm25); /* +48... */
	add_le128(%zmm2, %zmm6, %zmm24, %zmm25); /* +52... */
	add_le128(%zmm1, %zmm5, %zmm24, %zmm25); /* +56... */
	add_le128(%zmm0, %zmm4, %zmm24, %zmm25); /* +60... */
	kxorq %k1, %k1, %k1;

.align 4
.Lload_ctr_done:
	vbroadcasti64x2 .Lpack_bswap rRIP, %zmm17;
	vpbroadcastq (key_table)(CTX), %zmm16;
	vpshufb %zmm17, %zmm16, %zmm16;

	/* Byte-swap IVs and update counter. */
	addq $64, %r11;
	adcq $0, %r10;
	vpshufb %zmm19, %zmm15, %zmm15;
	vpshufb %zmm19, %zmm14, %zmm14;
	vpshufb %zmm19, %zmm13, %zmm13;
	vpshufb %zmm19, %zmm12, %zmm12;
	vpshufb %zmm19, %zmm11, %zmm11;
	vpshufb %zmm19, %zmm10, %zmm10;
	vpshufb %zmm19, %zmm9, %zmm9;
	vpshufb %zmm19, %zmm8, %zmm8;
	bswapq %r11;
	bswapq %r10;
	vpshufb %zmm19, %zmm7, %zmm7;
	vpshufb %zmm19, %zmm6, %zmm6;
	vpshufb %zmm19, %zmm5, %zmm5;
	vpshufb %zmm19, %zmm4, %zmm4;
	vpshufb %zmm19, %zmm3, %zmm3;
	vpshufb %zmm19, %zmm2, %zmm2;
	vpshufb %zmm19, %zmm1, %zmm1;
	vpshufb %zmm19, %zmm0, %zmm0;
	movq %r11, 8(%rcx);
	movq %r10, (%rcx);

.align 16
.Lctr_inpack64_pre:
	/* inpack64_pre: */
	vpxorq %zmm0, %zmm16, %zmm0;
	vpxorq %zmm1, %zmm16, %zmm1;
	vpxorq %zmm2, %zmm16, %zmm2;
	vpxorq %zmm3, %zmm16, %zmm3;
	vpxorq %zmm4, %zmm16, %zmm4;
	vpxorq %zmm5, %zmm16, %zmm5;
	vpxorq %zmm6, %zmm16, %zmm6;
	vpxorq %zmm7, %zmm16, %zmm7;
	vpxorq %zmm8, %zmm16, %zmm8;
	vpxorq %zmm9, %zmm16, %zmm9;
	vpxorq %zmm10, %zmm16, %zmm10;
	vpxorq %zmm11, %zmm16, %zmm11;
	vpxorq %zmm12, %zmm16, %zmm12;
	vpxorq %zmm13, %zmm16, %zmm13;
	vpxorq %zmm14, %zmm16, %zmm14;
	vpxorq %zmm15, %zmm16, %zmm15;

	call __camellia_gfni_avx512_enc_blk64;

	vpxorq 0 * 64(%rdx), %zmm7, %zmm7;
	vpxorq 1 * 64(%rdx), %zmm6, %zmm6;
	vpxorq 2 * 64(%rdx), %zmm5, %zmm5;
	vpxorq 3 * 64(%rdx), %zmm4, %zmm4;
	vpxorq 4 * 64(%rdx), %zmm3, %zmm3;
	vpxorq 5 * 64(%rdx), %zmm2, %zmm2;
	vpxorq 6 * 64(%rdx), %zmm1, %zmm1;
	vpxorq 7 * 64(%rdx), %zmm0, %zmm0;
	vpxorq 8 * 64(%rdx), %zmm15, %zmm15;
	vpxorq 9 * 64(%rdx), %zmm14, %zmm14;
	vpxorq 10 * 64(%rdx), %zmm13, %zmm13;
	vpxorq 11 * 64(%rdx), %zmm12, %zmm12;
	vpxorq 12 * 64(%rdx), %zmm11, %zmm11;
	vpxorq 13 * 64(%rdx), %zmm10, %zmm10;
	vpxorq 14 * 64(%rdx), %zmm9, %zmm9;
	vpxorq 15 * 64(%rdx), %zmm8, %zmm8;

	write_output(%zmm7, %zmm6, %zmm5, %zmm4, %zmm3, %zmm2, %zmm1, %zmm0,
		     %zmm15, %zmm14, %zmm13, %zmm12, %zmm11, %zmm10, %zmm9,
		     %zmm8, %rsi);

	clear_regs();

	ret_spec_stop;

.align 16
.Lctr_byteadd_full_ctr_carry:
	movq 8(%rcx), %r11;
	movq (%rcx), %r10;
	bswapq %r11;
	bswapq %r10;
	addq $64, %r11;
	adcq $0, %r10;
	bswapq %r11;
	bswapq %r10;
	movq %r11, 8(%rcx);
	movq %r10, (%rcx);
	jmp .Lctr_byteadd_zmm;
.align 16
.Lctr_byteadd:
	vbroadcasti64x2 (%rcx), %zmm12;
	je .Lctr_byteadd_full_ctr_carry;
	addb $64, 15(%rcx);
.Lctr_byteadd_zmm:
	vbroadcasti64x2 .Lbige_addb_16 rRIP, %zmm16;
	vmovdqa64 .Lbige_addb_0_1 rRIP, %zmm17;
	vmovdqa64 .Lbige_addb_4_5 rRIP, %zmm18;
	vmovdqa64 .Lbige_addb_8_9 rRIP, %zmm19;
	vmovdqa64 .Lbige_addb_12_13 rRIP, %zmm20;
	vpaddb %zmm16, %zmm12, %zmm8;
	vpaddb %zmm17, %zmm12, %zmm15;
	vpaddb %zmm18, %zmm12, %zmm14;
	vpaddb %zmm19, %zmm12, %zmm13;
	vpaddb %zmm20, %zmm12, %zmm12;
	vpaddb %zmm16, %zmm8, %zmm4;
	vpaddb %zmm17, %zmm8, %zmm11;
	vpaddb %zmm18, %zmm8, %zmm10;
	vpaddb %zmm19, %zmm8, %zmm9;
	vpaddb %zmm20, %zmm8, %zmm8;
	vpaddb %zmm16, %zmm4, %zmm0;
	vpaddb %zmm17, %zmm4, %zmm7;
	vpaddb %zmm18, %zmm4, %zmm6;
	vpaddb %zmm19, %zmm4, %zmm5;
	vpaddb %zmm20, %zmm4, %zmm4;
	vpaddb %zmm17, %zmm0, %zmm3;
	vpaddb %zmm18, %zmm0, %zmm2;
	vpaddb %zmm19, %zmm0, %zmm1;
	vpaddb %zmm20, %zmm0, %zmm0;

	vbroadcasti64x2 .Lpack_bswap rRIP, %zmm17
	vpbroadcastq (key_table)(CTX), %zmm16;
	vpshufb %zmm17, %zmm16, %zmm16;

	jmp .Lctr_inpack64_pre;
	CFI_ENDPROC();
ELF(.size _gcry_camellia_gfni_avx512_ctr_enc,.-_gcry_camellia_gfni_avx512_ctr_enc;)

.align 16
.globl _gcry_camellia_gfni_avx512_cbc_dec
ELF(.type   _gcry_camellia_gfni_avx512_cbc_dec,@function;)

_gcry_camellia_gfni_avx512_cbc_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (64 blocks)
	 *	%rdx: src (64 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	movq %rcx, %r9;

	cmpl $128, key_bitlength(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d; /* max */

	inpack64_pre(%zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
		     %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
		     %zmm15, %rdx, (key_table)(CTX, %r8, 8));

	call __camellia_gfni_avx512_dec_blk64;

	/* XOR output with IV */
	vmovdqu64 (%r9), %xmm16;
	vinserti64x2 $1, (0 * 16)(%rdx), %ymm16, %ymm16;
	vinserti64x4 $1, (1 * 16)(%rdx), %zmm16, %zmm16;
	vpxorq %zmm16, %zmm7, %zmm7;
	vpxorq (0 * 64 + 48)(%rdx), %zmm6, %zmm6;
	vpxorq (1 * 64 + 48)(%rdx), %zmm5, %zmm5;
	vpxorq (2 * 64 + 48)(%rdx), %zmm4, %zmm4;
	vpxorq (3 * 64 + 48)(%rdx), %zmm3, %zmm3;
	vpxorq (4 * 64 + 48)(%rdx), %zmm2, %zmm2;
	vpxorq (5 * 64 + 48)(%rdx), %zmm1, %zmm1;
	vpxorq (6 * 64 + 48)(%rdx), %zmm0, %zmm0;
	vpxorq (7 * 64 + 48)(%rdx), %zmm15, %zmm15;
	vpxorq (8 * 64 + 48)(%rdx), %zmm14, %zmm14;
	vpxorq (9 * 64 + 48)(%rdx), %zmm13, %zmm13;
	vpxorq (10 * 64 + 48)(%rdx), %zmm12, %zmm12;
	vpxorq (11 * 64 + 48)(%rdx), %zmm11, %zmm11;
	vpxorq (12 * 64 + 48)(%rdx), %zmm10, %zmm10;
	vpxorq (13 * 64 + 48)(%rdx), %zmm9, %zmm9;
	vpxorq (14 * 64 + 48)(%rdx), %zmm8, %zmm8;
	vmovdqu64 (15 * 64 + 48)(%rdx), %xmm16;

	write_output(%zmm7, %zmm6, %zmm5, %zmm4, %zmm3, %zmm2, %zmm1, %zmm0,
		     %zmm15, %zmm14, %zmm13, %zmm12, %zmm11, %zmm10, %zmm9,
		     %zmm8, %rsi);

	/* store new IV */
	vmovdqu64 %xmm16, (0)(%r9);

	clear_regs();

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_camellia_gfni_avx512_cbc_dec,.-_gcry_camellia_gfni_avx512_cbc_dec;)

.align 16
.globl _gcry_camellia_gfni_avx512_cfb_dec
ELF(.type   _gcry_camellia_gfni_avx512_cfb_dec,@function;)

_gcry_camellia_gfni_avx512_cfb_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (32 blocks)
	 *	%rdx: src (32 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	cmpl $128, key_bitlength(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d; /* max */

	/* inpack64_pre: */
	vpbroadcastq (key_table)(CTX), %zmm0;
	vpshufb .Lpack_bswap rRIP, %zmm0, %zmm0;
	vmovdqu64 (%rcx), %xmm15;
	vinserti64x2 $1, (%rdx), %ymm15, %ymm15;
	vinserti64x4 $1, 16(%rdx), %zmm15, %zmm15;
	vpxorq %zmm15, %zmm0, %zmm15;
	vpxorq (0 * 64 + 48)(%rdx), %zmm0, %zmm14;
	vpxorq (1 * 64 + 48)(%rdx), %zmm0, %zmm13;
	vpxorq (2 * 64 + 48)(%rdx), %zmm0, %zmm12;
	vpxorq (3 * 64 + 48)(%rdx), %zmm0, %zmm11;
	vpxorq (4 * 64 + 48)(%rdx), %zmm0, %zmm10;
	vpxorq (5 * 64 + 48)(%rdx), %zmm0, %zmm9;
	vpxorq (6 * 64 + 48)(%rdx), %zmm0, %zmm8;
	vpxorq (7 * 64 + 48)(%rdx), %zmm0, %zmm7;
	vpxorq (8 * 64 + 48)(%rdx), %zmm0, %zmm6;
	vpxorq (9 * 64 + 48)(%rdx), %zmm0, %zmm5;
	vpxorq (10 * 64 + 48)(%rdx), %zmm0, %zmm4;
	vpxorq (11 * 64 + 48)(%rdx), %zmm0, %zmm3;
	vpxorq (12 * 64 + 48)(%rdx), %zmm0, %zmm2;
	vpxorq (13 * 64 + 48)(%rdx), %zmm0, %zmm1;
	vpxorq (14 * 64 + 48)(%rdx), %zmm0, %zmm0;
	vmovdqu64 (15 * 64 + 48)(%rdx), %xmm16;
	vmovdqu64 %xmm16, (%rcx); /* store new IV */

	call __camellia_gfni_avx512_enc_blk64;

	vpxorq 0 * 64(%rdx), %zmm7, %zmm7;
	vpxorq 1 * 64(%rdx), %zmm6, %zmm6;
	vpxorq 2 * 64(%rdx), %zmm5, %zmm5;
	vpxorq 3 * 64(%rdx), %zmm4, %zmm4;
	vpxorq 4 * 64(%rdx), %zmm3, %zmm3;
	vpxorq 5 * 64(%rdx), %zmm2, %zmm2;
	vpxorq 6 * 64(%rdx), %zmm1, %zmm1;
	vpxorq 7 * 64(%rdx), %zmm0, %zmm0;
	vpxorq 8 * 64(%rdx), %zmm15, %zmm15;
	vpxorq 9 * 64(%rdx), %zmm14, %zmm14;
	vpxorq 10 * 64(%rdx), %zmm13, %zmm13;
	vpxorq 11 * 64(%rdx), %zmm12, %zmm12;
	vpxorq 12 * 64(%rdx), %zmm11, %zmm11;
	vpxorq 13 * 64(%rdx), %zmm10, %zmm10;
	vpxorq 14 * 64(%rdx), %zmm9, %zmm9;
	vpxorq 15 * 64(%rdx), %zmm8, %zmm8;

	write_output(%zmm7, %zmm6, %zmm5, %zmm4, %zmm3, %zmm2, %zmm1, %zmm0,
		     %zmm15, %zmm14, %zmm13, %zmm12, %zmm11, %zmm10, %zmm9,
		     %zmm8, %rsi);

	clear_regs();

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_camellia_gfni_avx512_cfb_dec,.-_gcry_camellia_gfni_avx512_cfb_dec;)

.align 16
.globl _gcry_camellia_gfni_avx512_ocb_enc
ELF(.type   _gcry_camellia_gfni_avx512_ocb_enc,@function;)

_gcry_camellia_gfni_avx512_ocb_enc:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (64 blocks)
	 *	%rdx: src (64 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[64])
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	pushq %r12;
	CFI_PUSH(%r12);
	pushq %r13;
	CFI_PUSH(%r13);
	pushq %r14;
	CFI_PUSH(%r14);
	pushq %r15;
	CFI_PUSH(%r15);
	pushq %rbx;
	CFI_PUSH(%rbx);

	vmovdqu64 (%rcx), %xmm30;

	/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
	/* Checksum_i = Checksum_{i-1} xor P_i  */
	/* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */

#define OCB_INPUT(n, l0reg, l1reg, l2reg, l3reg, zreg, zplain) \
	  vmovdqu64 (n * 64)(%rdx), zplain; \
	  vpxorq (l0reg), %xmm30, %xmm16; \
	  vpxorq (l1reg), %xmm16, %xmm30; \
	  vinserti64x2 $1, %xmm30, %ymm16, %ymm16; \
	  vpxorq (l2reg), %xmm30, %xmm30; \
	  vinserti64x2 $2, %xmm30, %zmm16, %zmm16; \
	  vpxorq (l3reg), %xmm30, %xmm30; \
	  vinserti64x2 $3, %xmm30, %zmm16, %zmm16; \
	  vpxorq zplain, %zmm16, zreg; \
	  vmovdqu64 %zmm16, (n * 64)(%rsi);

#define OCB_LOAD_PTRS(n) \
	  movq ((n * 4 * 8) + (0 * 8))(%r9), %r10; \
	  movq ((n * 4 * 8) + (1 * 8))(%r9), %r11; \
	  movq ((n * 4 * 8) + (2 * 8))(%r9), %r12; \
	  movq ((n * 4 * 8) + (3 * 8))(%r9), %r13; \
	  movq ((n * 4 * 8) + (4 * 8))(%r9), %r14; \
	  movq ((n * 4 * 8) + (5 * 8))(%r9), %r15; \
	  movq ((n * 4 * 8) + (6 * 8))(%r9), %rax; \
	  movq ((n * 4 * 8) + (7 * 8))(%r9), %rbx;

	OCB_LOAD_PTRS(0);
	OCB_INPUT(0, %r10, %r11, %r12, %r13, %zmm15, %zmm20);
	OCB_INPUT(1, %r14, %r15, %rax, %rbx, %zmm14, %zmm21);
	OCB_LOAD_PTRS(2);
	OCB_INPUT(2, %r10, %r11, %r12, %r13, %zmm13, %zmm22);
	vpternlogq $0x96, %zmm20, %zmm21, %zmm22;
	OCB_INPUT(3, %r14, %r15, %rax, %rbx, %zmm12, %zmm23);
	OCB_LOAD_PTRS(4);
	OCB_INPUT(4, %r10, %r11, %r12, %r13, %zmm11, %zmm24);
	OCB_INPUT(5, %r14, %r15, %rax, %rbx, %zmm10, %zmm25);
	vpternlogq $0x96, %zmm23, %zmm24, %zmm25;
	OCB_LOAD_PTRS(6);
	OCB_INPUT(6, %r10, %r11, %r12, %r13, %zmm9, %zmm20);
	OCB_INPUT(7, %r14, %r15, %rax, %rbx, %zmm8, %zmm21);
	OCB_LOAD_PTRS(8);
	OCB_INPUT(8, %r10, %r11, %r12, %r13, %zmm7, %zmm26);
	vpternlogq $0x96, %zmm20, %zmm21, %zmm26;
	OCB_INPUT(9, %r14, %r15, %rax, %rbx, %zmm6, %zmm23);
	OCB_LOAD_PTRS(10);
	OCB_INPUT(10, %r10, %r11, %r12, %r13, %zmm5, %zmm24);
	OCB_INPUT(11, %r14, %r15, %rax, %rbx, %zmm4, %zmm27);
	vpternlogq $0x96, %zmm23, %zmm24, %zmm27;
	OCB_LOAD_PTRS(12);
	OCB_INPUT(12, %r10, %r11, %r12, %r13, %zmm3, %zmm20);
	OCB_INPUT(13, %r14, %r15, %rax, %rbx, %zmm2, %zmm21);
	OCB_LOAD_PTRS(14);
	OCB_INPUT(14, %r10, %r11, %r12, %r13, %zmm1, %zmm23);
	vpternlogq $0x96, %zmm20, %zmm21, %zmm23;
	OCB_INPUT(15, %r14, %r15, %rax, %rbx, %zmm0, %zmm24);
#undef OCB_LOAD_PTRS
#undef OCB_INPUT

	vpbroadcastq (key_table)(CTX), %zmm16;
	vpshufb .Lpack_bswap rRIP, %zmm16, %zmm16;

	vpternlogq $0x96, %zmm24, %zmm22, %zmm25;
	vpternlogq $0x96, %zmm26, %zmm27, %zmm23;
	vpxorq %zmm25, %zmm23, %zmm20;
	vextracti64x4 $1, %zmm20, %ymm21;
	vpxorq %ymm21, %ymm20, %ymm20;
	vextracti64x2 $1, %ymm20, %xmm21;
	vpternlogq $0x96, (%r8), %xmm21, %xmm20;
	vmovdqu64 %xmm30, (%rcx);
	vmovdqu64 %xmm20, (%r8);

	cmpl $128, key_bitlength(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d; /* max */

	/* inpack64_pre: */
	vpxorq %zmm0, %zmm16, %zmm0;
	vpxorq %zmm1, %zmm16, %zmm1;
	vpxorq %zmm2, %zmm16, %zmm2;
	vpxorq %zmm3, %zmm16, %zmm3;
	vpxorq %zmm4, %zmm16, %zmm4;
	vpxorq %zmm5, %zmm16, %zmm5;
	vpxorq %zmm6, %zmm16, %zmm6;
	vpxorq %zmm7, %zmm16, %zmm7;
	vpxorq %zmm8, %zmm16, %zmm8;
	vpxorq %zmm9, %zmm16, %zmm9;
	vpxorq %zmm10, %zmm16, %zmm10;
	vpxorq %zmm11, %zmm16, %zmm11;
	vpxorq %zmm12, %zmm16, %zmm12;
	vpxorq %zmm13, %zmm16, %zmm13;
	vpxorq %zmm14, %zmm16, %zmm14;
	vpxorq %zmm15, %zmm16, %zmm15;

	call __camellia_gfni_avx512_enc_blk64;

	vpxorq 0 * 64(%rsi), %zmm7, %zmm7;
	vpxorq 1 * 64(%rsi), %zmm6, %zmm6;
	vpxorq 2 * 64(%rsi), %zmm5, %zmm5;
	vpxorq 3 * 64(%rsi), %zmm4, %zmm4;
	vpxorq 4 * 64(%rsi), %zmm3, %zmm3;
	vpxorq 5 * 64(%rsi), %zmm2, %zmm2;
	vpxorq 6 * 64(%rsi), %zmm1, %zmm1;
	vpxorq 7 * 64(%rsi), %zmm0, %zmm0;
	vpxorq 8 * 64(%rsi), %zmm15, %zmm15;
	vpxorq 9 * 64(%rsi), %zmm14, %zmm14;
	vpxorq 10 * 64(%rsi), %zmm13, %zmm13;
	vpxorq 11 * 64(%rsi), %zmm12, %zmm12;
	vpxorq 12 * 64(%rsi), %zmm11, %zmm11;
	vpxorq 13 * 64(%rsi), %zmm10, %zmm10;
	vpxorq 14 * 64(%rsi), %zmm9, %zmm9;
	vpxorq 15 * 64(%rsi), %zmm8, %zmm8;

	write_output(%zmm7, %zmm6, %zmm5, %zmm4, %zmm3, %zmm2, %zmm1, %zmm0,
		     %zmm15, %zmm14, %zmm13, %zmm12, %zmm11, %zmm10, %zmm9,
		     %zmm8, %rsi);

	clear_regs();

	popq %rbx;
	CFI_RESTORE(%rbx);
	popq %r15;
	CFI_RESTORE(%r15);
	popq %r14;
	CFI_RESTORE(%r14);
	popq %r13;
	CFI_RESTORE(%r12);
	popq %r12;
	CFI_RESTORE(%r13);

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_camellia_gfni_avx512_ocb_enc,.-_gcry_camellia_gfni_avx512_ocb_enc;)

.align 16
.globl _gcry_camellia_gfni_avx512_ocb_dec
ELF(.type   _gcry_camellia_gfni_avx512_ocb_dec,@function;)

_gcry_camellia_gfni_avx512_ocb_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (64 blocks)
	 *	%rdx: src (64 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[64])
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	pushq %r12;
	CFI_PUSH(%r12);
	pushq %r13;
	CFI_PUSH(%r13);
	pushq %r14;
	CFI_PUSH(%r14);
	pushq %r15;
	CFI_PUSH(%r15);
	pushq %rbx;
	CFI_PUSH(%rbx);
	pushq %r8;
	CFI_PUSH(%r8);

	vmovdqu64 (%rcx), %xmm30;

	/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
	/* C_i = Offset_i xor DECIPHER(K, P_i xor Offset_i)  */

#define OCB_INPUT(n, l0reg, l1reg, l2reg, l3reg, zreg) \
	  vpxorq (l0reg), %xmm30, %xmm16; \
	  vpxorq (l1reg), %xmm16, %xmm30; \
	  vinserti64x2 $1, %xmm30, %ymm16, %ymm16; \
	  vpxorq (l2reg), %xmm30, %xmm30; \
	  vinserti64x2 $2, %xmm30, %zmm16, %zmm16; \
	  vpxorq (l3reg), %xmm30, %xmm30; \
	  vinserti64x2 $3, %xmm30, %zmm16, %zmm16; \
	  vpxorq (n * 64)(%rdx), %zmm16, zreg; \
	  vmovdqu64 %zmm16, (n * 64)(%rsi);

#define OCB_LOAD_PTRS(n) \
	  movq ((n * 4 * 8) + (0 * 8))(%r9), %r10; \
	  movq ((n * 4 * 8) + (1 * 8))(%r9), %r11; \
	  movq ((n * 4 * 8) + (2 * 8))(%r9), %r12; \
	  movq ((n * 4 * 8) + (3 * 8))(%r9), %r13; \
	  movq ((n * 4 * 8) + (4 * 8))(%r9), %r14; \
	  movq ((n * 4 * 8) + (5 * 8))(%r9), %r15; \
	  movq ((n * 4 * 8) + (6 * 8))(%r9), %rax; \
	  movq ((n * 4 * 8) + (7 * 8))(%r9), %rbx;

	OCB_LOAD_PTRS(0);
	OCB_INPUT(0, %r10, %r11, %r12, %r13, %zmm15);
	OCB_INPUT(1, %r14, %r15, %rax, %rbx, %zmm14);
	OCB_LOAD_PTRS(2);
	OCB_INPUT(2, %r10, %r11, %r12, %r13, %zmm13);
	OCB_INPUT(3, %r14, %r15, %rax, %rbx, %zmm12);
	OCB_LOAD_PTRS(4);
	OCB_INPUT(4, %r10, %r11, %r12, %r13, %zmm11);
	OCB_INPUT(5, %r14, %r15, %rax, %rbx, %zmm10);
	OCB_LOAD_PTRS(6);
	OCB_INPUT(6, %r10, %r11, %r12, %r13, %zmm9);
	OCB_INPUT(7, %r14, %r15, %rax, %rbx, %zmm8);
	OCB_LOAD_PTRS(8);
	OCB_INPUT(8, %r10, %r11, %r12, %r13, %zmm7);
	OCB_INPUT(9, %r14, %r15, %rax, %rbx, %zmm6);
	OCB_LOAD_PTRS(10);
	OCB_INPUT(10, %r10, %r11, %r12, %r13, %zmm5);
	OCB_INPUT(11, %r14, %r15, %rax, %rbx, %zmm4);
	OCB_LOAD_PTRS(12);
	OCB_INPUT(12, %r10, %r11, %r12, %r13, %zmm3);
	OCB_INPUT(13, %r14, %r15, %rax, %rbx, %zmm2);
	OCB_LOAD_PTRS(14);
	OCB_INPUT(14, %r10, %r11, %r12, %r13, %zmm1);
	OCB_INPUT(15, %r14, %r15, %rax, %rbx, %zmm0);
#undef OCB_LOAD_PTRS
#undef OCB_INPUT

	vmovdqu64 %xmm30, (%rcx);

	cmpl $128, key_bitlength(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d; /* max */

	vpbroadcastq (key_table)(CTX, %r8, 8), %zmm16;
	vpshufb .Lpack_bswap rRIP, %zmm16, %zmm16;

	/* inpack64_pre: */
	vpxorq %zmm0, %zmm16, %zmm0;
	vpxorq %zmm1, %zmm16, %zmm1;
	vpxorq %zmm2, %zmm16, %zmm2;
	vpxorq %zmm3, %zmm16, %zmm3;
	vpxorq %zmm4, %zmm16, %zmm4;
	vpxorq %zmm5, %zmm16, %zmm5;
	vpxorq %zmm6, %zmm16, %zmm6;
	vpxorq %zmm7, %zmm16, %zmm7;
	vpxorq %zmm8, %zmm16, %zmm8;
	vpxorq %zmm9, %zmm16, %zmm9;
	vpxorq %zmm10, %zmm16, %zmm10;
	vpxorq %zmm11, %zmm16, %zmm11;
	vpxorq %zmm12, %zmm16, %zmm12;
	vpxorq %zmm13, %zmm16, %zmm13;
	vpxorq %zmm14, %zmm16, %zmm14;
	vpxorq %zmm15, %zmm16, %zmm15;

	call __camellia_gfni_avx512_dec_blk64;

	vpxorq 0 * 64(%rsi), %zmm7, %zmm7;
	vpxorq 1 * 64(%rsi), %zmm6, %zmm6;
	vpxorq 2 * 64(%rsi), %zmm5, %zmm5;
	vpxorq 3 * 64(%rsi), %zmm4, %zmm4;
	vpxorq 4 * 64(%rsi), %zmm3, %zmm3;
	vpxorq 5 * 64(%rsi), %zmm2, %zmm2;
	vpxorq 6 * 64(%rsi), %zmm1, %zmm1;
	vpxorq 7 * 64(%rsi), %zmm0, %zmm0;
	vpxorq 8 * 64(%rsi), %zmm15, %zmm15;
	vpxorq 9 * 64(%rsi), %zmm14, %zmm14;
	vpxorq 10 * 64(%rsi), %zmm13, %zmm13;
	vpxorq 11 * 64(%rsi), %zmm12, %zmm12;
	vpxorq 12 * 64(%rsi), %zmm11, %zmm11;
	vpxorq 13 * 64(%rsi), %zmm10, %zmm10;
	vpxorq 14 * 64(%rsi), %zmm9, %zmm9;
	vpxorq 15 * 64(%rsi), %zmm8, %zmm8;

	write_output(%zmm7, %zmm6, %zmm5, %zmm4, %zmm3, %zmm2, %zmm1, %zmm0,
		     %zmm15, %zmm14, %zmm13, %zmm12, %zmm11, %zmm10, %zmm9,
		     %zmm8, %rsi);

	popq %r8;
	CFI_RESTORE(%r8);

	/* Checksum_i = Checksum_{i-1} xor C_i  */
	vpternlogq $0x96, %zmm7, %zmm6, %zmm5;
	vpternlogq $0x96, %zmm4, %zmm3, %zmm2;
	vpternlogq $0x96, %zmm1, %zmm0, %zmm15;
	vpternlogq $0x96, %zmm14, %zmm13, %zmm12;
	vpternlogq $0x96, %zmm11, %zmm10, %zmm9;
	vpternlogq $0x96, %zmm5, %zmm2, %zmm15;
	vpternlogq $0x96, %zmm12, %zmm9, %zmm8;
	vpxorq %zmm15, %zmm8, %zmm8;

	vextracti64x4 $1, %zmm8, %ymm0;
	vpxor %ymm0, %ymm8, %ymm8;
	vextracti128 $1, %ymm8, %xmm0;
	vpternlogq $0x96, (%r8), %xmm0, %xmm8;
	vmovdqu64 %xmm8, (%r8);

	clear_regs();

	popq %rbx;
	CFI_RESTORE(%rbx);
	popq %r15;
	CFI_RESTORE(%r15);
	popq %r14;
	CFI_RESTORE(%r14);
	popq %r13;
	CFI_RESTORE(%r12);
	popq %r12;
	CFI_RESTORE(%r13);

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_camellia_gfni_avx512_ocb_dec,.-_gcry_camellia_gfni_avx512_ocb_dec;)

.align 16
.globl _gcry_camellia_gfni_avx512_enc_blk64
ELF(.type   _gcry_camellia_gfni_avx512_enc_blk64,@function;)

_gcry_camellia_gfni_avx512_enc_blk64:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (64 blocks)
	 *	%rdx: src (64 blocks)
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	cmpl $128, key_bitlength(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d; /* max */
	xorl %eax, %eax;

	vpbroadcastq (key_table)(CTX), %zmm0;
	vpshufb .Lpack_bswap rRIP, %zmm0, %zmm0;

	vpxorq (0) * 64(%rdx), %zmm0, %zmm15;
	vpxorq (1) * 64(%rdx), %zmm0, %zmm14;
	vpxorq (2) * 64(%rdx), %zmm0, %zmm13;
	vpxorq (3) * 64(%rdx), %zmm0, %zmm12;
	vpxorq (4) * 64(%rdx), %zmm0, %zmm11;
	vpxorq (5) * 64(%rdx), %zmm0, %zmm10;
	vpxorq (6) * 64(%rdx), %zmm0, %zmm9;
	vpxorq (7) * 64(%rdx), %zmm0, %zmm8;
	vpxorq (8) * 64(%rdx), %zmm0, %zmm7;
	vpxorq (9) * 64(%rdx), %zmm0, %zmm6;
	vpxorq (10) * 64(%rdx), %zmm0, %zmm5;
	vpxorq (11) * 64(%rdx), %zmm0, %zmm4;
	vpxorq (12) * 64(%rdx), %zmm0, %zmm3;
	vpxorq (13) * 64(%rdx), %zmm0, %zmm2;
	vpxorq (14) * 64(%rdx), %zmm0, %zmm1;
	vpxorq (15) * 64(%rdx), %zmm0, %zmm0;

	call __camellia_gfni_avx512_enc_blk64;

	vmovdqu64 %zmm7, (0) * 64(%rsi);
	vmovdqu64 %zmm6, (1) * 64(%rsi);
	vmovdqu64 %zmm5, (2) * 64(%rsi);
	vmovdqu64 %zmm4, (3) * 64(%rsi);
	vmovdqu64 %zmm3, (4) * 64(%rsi);
	vmovdqu64 %zmm2, (5) * 64(%rsi);
	vmovdqu64 %zmm1, (6) * 64(%rsi);
	vmovdqu64 %zmm0, (7) * 64(%rsi);
	vmovdqu64 %zmm15, (8) * 64(%rsi);
	vmovdqu64 %zmm14, (9) * 64(%rsi);
	vmovdqu64 %zmm13, (10) * 64(%rsi);
	vmovdqu64 %zmm12, (11) * 64(%rsi);
	vmovdqu64 %zmm11, (12) * 64(%rsi);
	vmovdqu64 %zmm10, (13) * 64(%rsi);
	vmovdqu64 %zmm9, (14) * 64(%rsi);
	vmovdqu64 %zmm8, (15) * 64(%rsi);

	clear_regs();

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_camellia_gfni_avx512_enc_blk64,.-_gcry_camellia_gfni_avx512_enc_blk64;)

.align 16
.globl _gcry_camellia_gfni_avx512_dec_blk64
ELF(.type   _gcry_camellia_gfni_avx512_dec_blk64,@function;)

_gcry_camellia_gfni_avx512_dec_blk64:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (64 blocks)
	 *	%rdx: src (64 blocks)
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	cmpl $128, key_bitlength(CTX);
	movl $32, %r8d;
	movl $24, %eax;
	cmovel %eax, %r8d; /* max */
	xorl %eax, %eax;

	vpbroadcastq (key_table)(CTX, %r8, 8), %zmm0;
	vpshufb .Lpack_bswap rRIP, %zmm0, %zmm0;

	vpxorq (0) * 64(%rdx), %zmm0, %zmm15;
	vpxorq (1) * 64(%rdx), %zmm0, %zmm14;
	vpxorq (2) * 64(%rdx), %zmm0, %zmm13;
	vpxorq (3) * 64(%rdx), %zmm0, %zmm12;
	vpxorq (4) * 64(%rdx), %zmm0, %zmm11;
	vpxorq (5) * 64(%rdx), %zmm0, %zmm10;
	vpxorq (6) * 64(%rdx), %zmm0, %zmm9;
	vpxorq (7) * 64(%rdx), %zmm0, %zmm8;
	vpxorq (8) * 64(%rdx), %zmm0, %zmm7;
	vpxorq (9) * 64(%rdx), %zmm0, %zmm6;
	vpxorq (10) * 64(%rdx), %zmm0, %zmm5;
	vpxorq (11) * 64(%rdx), %zmm0, %zmm4;
	vpxorq (12) * 64(%rdx), %zmm0, %zmm3;
	vpxorq (13) * 64(%rdx), %zmm0, %zmm2;
	vpxorq (14) * 64(%rdx), %zmm0, %zmm1;
	vpxorq (15) * 64(%rdx), %zmm0, %zmm0;

	call __camellia_gfni_avx512_dec_blk64;

	vmovdqu64 %zmm7, (0) * 64(%rsi);
	vmovdqu64 %zmm6, (1) * 64(%rsi);
	vmovdqu64 %zmm5, (2) * 64(%rsi);
	vmovdqu64 %zmm4, (3) * 64(%rsi);
	vmovdqu64 %zmm3, (4) * 64(%rsi);
	vmovdqu64 %zmm2, (5) * 64(%rsi);
	vmovdqu64 %zmm1, (6) * 64(%rsi);
	vmovdqu64 %zmm0, (7) * 64(%rsi);
	vmovdqu64 %zmm15, (8) * 64(%rsi);
	vmovdqu64 %zmm14, (9) * 64(%rsi);
	vmovdqu64 %zmm13, (10) * 64(%rsi);
	vmovdqu64 %zmm12, (11) * 64(%rsi);
	vmovdqu64 %zmm11, (12) * 64(%rsi);
	vmovdqu64 %zmm10, (13) * 64(%rsi);
	vmovdqu64 %zmm9, (14) * 64(%rsi);
	vmovdqu64 %zmm8, (15) * 64(%rsi);

	clear_regs();

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_camellia_gfni_avx512_dec_blk64,.-_gcry_camellia_gfni_avx512_dec_blk64;)

#endif /* defined(ENABLE_GFNI_SUPPORT) && defined(ENABLE_AVX512_SUPPORT) */
#endif /* __x86_64 */
