/* aria-gfni-avx512-amd64.S  -  GFNI/AVX512 implementation of ARIA cipher
 *
 * Copyright (C) 2022-2023 Taehee Yoo <ap420073@gmail.com>
 * Copyright (C) 2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
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
    defined(ENABLE_AVX512_SUPPORT) && defined(ENABLE_GFNI_SUPPORT)

#include "asm-common-amd64.h"

/* struct ARIA_context: */
#define ARIA_BLOCK_SIZE  16
#define ARIA_MAX_RD_KEYS 17
#define ARIA_CTX_enc_key 0
#define ARIA_CTX_dec_key (ARIA_CTX_enc_key + (ARIA_BLOCK_SIZE * ARIA_MAX_RD_KEYS))
#define ARIA_CTX_rounds (ARIA_CTX_dec_key + (ARIA_BLOCK_SIZE * ARIA_MAX_RD_KEYS))

/* register macros */
#define CTX %rdi

/* helper macros */
#define STACK_DEPTH (2 * 8 + 16 * 64 + 63)

#define BV8(a0, a1, a2, a3, a4, a5, a6, a7)		\
	( (((a0) & 1) << 0) |				\
	  (((a1) & 1) << 1) |				\
	  (((a2) & 1) << 2) |				\
	  (((a3) & 1) << 3) |				\
	  (((a4) & 1) << 4) |				\
	  (((a5) & 1) << 5) |				\
	  (((a6) & 1) << 6) |				\
	  (((a7) & 1) << 7) )

#define BM8X8(l0, l1, l2, l3, l4, l5, l6, l7)		\
	( ((l7) << (0 * 8)) |				\
	  ((l6) << (1 * 8)) |				\
	  ((l5) << (2 * 8)) |				\
	  ((l4) << (3 * 8)) |				\
	  ((l3) << (4 * 8)) |				\
	  ((l2) << (5 * 8)) |				\
	  ((l1) << (6 * 8)) |				\
	  ((l0) << (7 * 8)) )

/* asm macros */
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
	kxorq %k1, %k1, %k1; \
	vzeroall; \
	clear_zmm16_zmm31()

#define add_le128(out, in, lo_counter, hi_counter1)	\
	vpaddq lo_counter, in, out;			\
	vpcmpuq $1, lo_counter, out, %k1;		\
	kaddb %k1, %k1, %k1;				\
	vpaddq hi_counter1, out, out{%k1};

#define filter_8bit(x, lo_t, hi_t, mask4bit, tmp0)	\
	vpandq x, mask4bit, tmp0;			\
	vpandqn x, mask4bit, x;				\
	vpsrld $4, x, x;				\
							\
	vpshufb tmp0, lo_t, tmp0;			\
	vpshufb x, hi_t, x;				\
	vpxorq tmp0, x, x;

#define transpose_4x4(x0, x1, x2, x3, t1, t2)		\
	vpunpckhdq x1, x0, t2;				\
	vpunpckldq x1, x0, x0;				\
							\
	vpunpckldq x3, x2, t1;				\
	vpunpckhdq x3, x2, x2;				\
							\
	vpunpckhqdq t1, x0, x1;				\
	vpunpcklqdq t1, x0, x0;				\
							\
	vpunpckhqdq x2, t2, x3;				\
	vpunpcklqdq x2, t2, x2;

#define byteslice_16x16b(a0, b0, c0, d0,		\
			 a1, b1, c1, d1,		\
			 a2, b2, c2, d2,		\
			 a3, b3, c3, d3,		\
			 st0, st1)			\
	vmovdqu64 d2, st0;				\
	vmovdqu64 d3, st1;				\
	transpose_4x4(a0, a1, a2, a3, d2, d3);		\
	transpose_4x4(b0, b1, b2, b3, d2, d3);		\
	vmovdqu64 st0, d2;				\
	vmovdqu64 st1, d3;				\
							\
	vmovdqu64 a0, st0;				\
	vmovdqu64 a1, st1;				\
	transpose_4x4(c0, c1, c2, c3, a0, a1);		\
	transpose_4x4(d0, d1, d2, d3, a0, a1);		\
							\
	vbroadcasti64x2 .Lshufb_16x16b rRIP, a0;	\
	vmovdqu64 st1, a1;				\
	vpshufb a0, a2, a2;				\
	vpshufb a0, a3, a3;				\
	vpshufb a0, b0, b0;				\
	vpshufb a0, b1, b1;				\
	vpshufb a0, b2, b2;				\
	vpshufb a0, b3, b3;				\
	vpshufb a0, a1, a1;				\
	vpshufb a0, c0, c0;				\
	vpshufb a0, c1, c1;				\
	vpshufb a0, c2, c2;				\
	vpshufb a0, c3, c3;				\
	vpshufb a0, d0, d0;				\
	vpshufb a0, d1, d1;				\
	vpshufb a0, d2, d2;				\
	vpshufb a0, d3, d3;				\
	vmovdqu64 d3, st1;				\
	vmovdqu64 st0, d3;				\
	vpshufb a0, d3, a0;				\
	vmovdqu64 d2, st0;				\
							\
	transpose_4x4(a0, b0, c0, d0, d2, d3);		\
	transpose_4x4(a1, b1, c1, d1, d2, d3);		\
	vmovdqu64 st0, d2;				\
	vmovdqu64 st1, d3;				\
							\
	vmovdqu64 b0, st0;				\
	vmovdqu64 b1, st1;				\
	transpose_4x4(a2, b2, c2, d2, b0, b1);		\
	transpose_4x4(a3, b3, c3, d3, b0, b1);		\
	vmovdqu64 st0, b0;				\
	vmovdqu64 st1, b1;				\
	/* does not adjust output bytes inside vectors */

#define debyteslice_16x16b(a0, b0, c0, d0,		\
			   a1, b1, c1, d1,		\
			   a2, b2, c2, d2,		\
			   a3, b3, c3, d3,		\
			   st0, st1)			\
	vmovdqu64 d2, st0;				\
	vmovdqu64 d3, st1;				\
	transpose_4x4(a0, a1, a2, a3, d2, d3);		\
	transpose_4x4(b0, b1, b2, b3, d2, d3);		\
	vmovdqu64 st0, d2;				\
	vmovdqu64 st1, d3;				\
							\
	vmovdqu64 a0, st0;				\
	vmovdqu64 a1, st1;				\
	transpose_4x4(c0, c1, c2, c3, a0, a1);		\
	transpose_4x4(d0, d1, d2, d3, a0, a1);		\
							\
	vbroadcasti64x2 .Lshufb_16x16b rRIP, a0;	\
	vmovdqu64 st1, a1;				\
	vpshufb a0, a2, a2;				\
	vpshufb a0, a3, a3;				\
	vpshufb a0, b0, b0;				\
	vpshufb a0, b1, b1;				\
	vpshufb a0, b2, b2;				\
	vpshufb a0, b3, b3;				\
	vpshufb a0, a1, a1;				\
	vpshufb a0, c0, c0;				\
	vpshufb a0, c1, c1;				\
	vpshufb a0, c2, c2;				\
	vpshufb a0, c3, c3;				\
	vpshufb a0, d0, d0;				\
	vpshufb a0, d1, d1;				\
	vpshufb a0, d2, d2;				\
	vpshufb a0, d3, d3;				\
	vmovdqu64 d3, st1;				\
	vmovdqu64 st0, d3;				\
	vpshufb a0, d3, a0;				\
	vmovdqu64 d2, st0;				\
							\
	transpose_4x4(c0, d0, a0, b0, d2, d3);		\
	transpose_4x4(c1, d1, a1, b1, d2, d3);		\
	vmovdqu64 st0, d2;				\
	vmovdqu64 st1, d3;				\
							\
	vmovdqu64 b0, st0;				\
	vmovdqu64 b1, st1;				\
	transpose_4x4(c2, d2, a2, b2, b0, b1);		\
	transpose_4x4(c3, d3, a3, b3, b0, b1);		\
	vmovdqu64 st0, b0;				\
	vmovdqu64 st1, b1;				\
	/* does not adjust output bytes inside vectors */

/* load blocks to registers and apply pre-whitening */
#define inpack16_pre(x0, x1, x2, x3,			\
		     x4, x5, x6, x7,			\
		     y0, y1, y2, y3,			\
		     y4, y5, y6, y7,			\
		     rio)				\
	vmovdqu64 (0 * 64)(rio), x0;			\
	vmovdqu64 (1 * 64)(rio), x1;			\
	vmovdqu64 (2 * 64)(rio), x2;			\
	vmovdqu64 (3 * 64)(rio), x3;			\
	vmovdqu64 (4 * 64)(rio), x4;			\
	vmovdqu64 (5 * 64)(rio), x5;			\
	vmovdqu64 (6 * 64)(rio), x6;			\
	vmovdqu64 (7 * 64)(rio), x7;			\
	vmovdqu64 (8 * 64)(rio), y0;			\
	vmovdqu64 (9 * 64)(rio), y1;			\
	vmovdqu64 (10 * 64)(rio), y2;			\
	vmovdqu64 (11 * 64)(rio), y3;			\
	vmovdqu64 (12 * 64)(rio), y4;			\
	vmovdqu64 (13 * 64)(rio), y5;			\
	vmovdqu64 (14 * 64)(rio), y6;			\
	vmovdqu64 (15 * 64)(rio), y7;

/* byteslice pre-whitened blocks and store to temporary memory */
#define inpack16_post(x0, x1, x2, x3,			\
		      x4, x5, x6, x7,			\
		      y0, y1, y2, y3,			\
		      y4, y5, y6, y7,			\
		      mem_ab, mem_cd)			\
	byteslice_16x16b(x0, x1, x2, x3,		\
			 x4, x5, x6, x7,		\
			 y0, y1, y2, y3,		\
			 y4, y5, y6, y7,		\
			 (mem_ab), (mem_cd));		\
							\
	vmovdqu64 x0, 0 * 64(mem_ab);			\
	vmovdqu64 x1, 1 * 64(mem_ab);			\
	vmovdqu64 x2, 2 * 64(mem_ab);			\
	vmovdqu64 x3, 3 * 64(mem_ab);			\
	vmovdqu64 x4, 4 * 64(mem_ab);			\
	vmovdqu64 x5, 5 * 64(mem_ab);			\
	vmovdqu64 x6, 6 * 64(mem_ab);			\
	vmovdqu64 x7, 7 * 64(mem_ab);			\
	vmovdqu64 y0, 0 * 64(mem_cd);			\
	vmovdqu64 y1, 1 * 64(mem_cd);			\
	vmovdqu64 y2, 2 * 64(mem_cd);			\
	vmovdqu64 y3, 3 * 64(mem_cd);			\
	vmovdqu64 y4, 4 * 64(mem_cd);			\
	vmovdqu64 y5, 5 * 64(mem_cd);			\
	vmovdqu64 y6, 6 * 64(mem_cd);			\
	vmovdqu64 y7, 7 * 64(mem_cd);

#define write_output(x0, x1, x2, x3,			\
		     x4, x5, x6, x7,			\
		     y0, y1, y2, y3,			\
		     y4, y5, y6, y7,			\
		     mem)				\
	vmovdqu64 x0, 0 * 64(mem);			\
	vmovdqu64 x1, 1 * 64(mem);			\
	vmovdqu64 x2, 2 * 64(mem);			\
	vmovdqu64 x3, 3 * 64(mem);			\
	vmovdqu64 x4, 4 * 64(mem);			\
	vmovdqu64 x5, 5 * 64(mem);			\
	vmovdqu64 x6, 6 * 64(mem);			\
	vmovdqu64 x7, 7 * 64(mem);			\
	vmovdqu64 y0, 8 * 64(mem);			\
	vmovdqu64 y1, 9 * 64(mem);			\
	vmovdqu64 y2, 10 * 64(mem);			\
	vmovdqu64 y3, 11 * 64(mem);			\
	vmovdqu64 y4, 12 * 64(mem);			\
	vmovdqu64 y5, 13 * 64(mem);			\
	vmovdqu64 y6, 14 * 64(mem);			\
	vmovdqu64 y7, 15 * 64(mem);			\

#define aria_store_state_8way(x0, x1, x2, x3,		\
			      x4, x5, x6, x7,		\
			      mem_tmp, idx)		\
	vmovdqu64 x0, ((idx + 0) * 64)(mem_tmp);	\
	vmovdqu64 x1, ((idx + 1) * 64)(mem_tmp);	\
	vmovdqu64 x2, ((idx + 2) * 64)(mem_tmp);	\
	vmovdqu64 x3, ((idx + 3) * 64)(mem_tmp);	\
	vmovdqu64 x4, ((idx + 4) * 64)(mem_tmp);	\
	vmovdqu64 x5, ((idx + 5) * 64)(mem_tmp);	\
	vmovdqu64 x6, ((idx + 6) * 64)(mem_tmp);	\
	vmovdqu64 x7, ((idx + 7) * 64)(mem_tmp);

#define aria_load_state_8way(x0, x1, x2, x3,		\
			     x4, x5, x6, x7,		\
			     mem_tmp, idx)		\
	vmovdqu64 ((idx + 0) * 64)(mem_tmp), x0;	\
	vmovdqu64 ((idx + 1) * 64)(mem_tmp), x1;	\
	vmovdqu64 ((idx + 2) * 64)(mem_tmp), x2;	\
	vmovdqu64 ((idx + 3) * 64)(mem_tmp), x3;	\
	vmovdqu64 ((idx + 4) * 64)(mem_tmp), x4;	\
	vmovdqu64 ((idx + 5) * 64)(mem_tmp), x5;	\
	vmovdqu64 ((idx + 6) * 64)(mem_tmp), x6;	\
	vmovdqu64 ((idx + 7) * 64)(mem_tmp), x7;

#define aria_ark_16way(x0, x1, x2, x3,			\
		       x4, x5, x6, x7,			\
		       y0, y1, y2, y3,			\
		       y4, y5, y6, y7,			\
		       t0, rk, round)			\
	/* AddRoundKey */                               \
	vpbroadcastb ((round * 16) + 3)(rk), t0;	\
	vpxorq t0, x0, x0;				\
	vpbroadcastb ((round * 16) + 2)(rk), t0;	\
	vpxorq t0, x1, x1;				\
	vpbroadcastb ((round * 16) + 1)(rk), t0;	\
	vpxorq t0, x2, x2;				\
	vpbroadcastb ((round * 16) + 0)(rk), t0;	\
	vpxorq t0, x3, x3;				\
	vpbroadcastb ((round * 16) + 7)(rk), t0;	\
	vpxorq t0, x4, x4;				\
	vpbroadcastb ((round * 16) + 6)(rk), t0;	\
	vpxorq t0, x5, x5;				\
	vpbroadcastb ((round * 16) + 5)(rk), t0;	\
	vpxorq t0, x6, x6;				\
	vpbroadcastb ((round * 16) + 4)(rk), t0;	\
	vpxorq t0, x7, x7;				\
	vpbroadcastb ((round * 16) + 11)(rk), t0;	\
	vpxorq t0, y0, y0;				\
	vpbroadcastb ((round * 16) + 10)(rk), t0;	\
	vpxorq t0, y1, y1;				\
	vpbroadcastb ((round * 16) + 9)(rk), t0;	\
	vpxorq t0, y2, y2;				\
	vpbroadcastb ((round * 16) + 8)(rk), t0;	\
	vpxorq t0, y3, y3;				\
	vpbroadcastb ((round * 16) + 15)(rk), t0;	\
	vpxorq t0, y4, y4;				\
	vpbroadcastb ((round * 16) + 14)(rk), t0;	\
	vpxorq t0, y5, y5;				\
	vpbroadcastb ((round * 16) + 13)(rk), t0;	\
	vpxorq t0, y6, y6;				\
	vpbroadcastb ((round * 16) + 12)(rk), t0;	\
	vpxorq t0, y7, y7;

#define aria_sbox_8way_gfni(x0, x1, x2, x3,		\
			    x4, x5, x6, x7,		\
			    t0, t1, t2, t3,		\
			    t4, t5, t6, t7)		\
	vpbroadcastq .Ltf_s2_bitmatrix rRIP, t0;	\
	vpbroadcastq .Ltf_inv_bitmatrix rRIP, t1;	\
	vpbroadcastq .Ltf_id_bitmatrix rRIP, t2;	\
	vpbroadcastq .Ltf_aff_bitmatrix rRIP, t3;	\
	vpbroadcastq .Ltf_x2_bitmatrix rRIP, t4;	\
	vgf2p8affineinvqb $(tf_s2_const), t0, x1, x1;	\
	vgf2p8affineinvqb $(tf_s2_const), t0, x5, x5;	\
	vgf2p8affineqb $(tf_inv_const), t1, x2, x2;	\
	vgf2p8affineqb $(tf_inv_const), t1, x6, x6;	\
	vgf2p8affineinvqb $0, t2, x2, x2;		\
	vgf2p8affineinvqb $0, t2, x6, x6;		\
	vgf2p8affineinvqb $(tf_aff_const), t3, x0, x0;	\
	vgf2p8affineinvqb $(tf_aff_const), t3, x4, x4;	\
	vgf2p8affineqb $(tf_x2_const), t4, x3, x3;	\
	vgf2p8affineqb $(tf_x2_const), t4, x7, x7;	\
	vgf2p8affineinvqb $0, t2, x3, x3;		\
	vgf2p8affineinvqb $0, t2, x7, x7;

#define aria_sbox_16way_gfni(x0, x1, x2, x3,		\
			     x4, x5, x6, x7,		\
			     y0, y1, y2, y3,		\
			     y4, y5, y6, y7,		\
			     t0, t1, t2, t3,		\
			     t4, t5, t6, t7)		\
	vpbroadcastq .Ltf_s2_bitmatrix rRIP, t0;	\
	vpbroadcastq .Ltf_inv_bitmatrix rRIP, t1;	\
	vpbroadcastq .Ltf_id_bitmatrix rRIP, t2;	\
	vpbroadcastq .Ltf_aff_bitmatrix rRIP, t3;	\
	vpbroadcastq .Ltf_x2_bitmatrix rRIP, t4;	\
	vgf2p8affineinvqb $(tf_s2_const), t0, x1, x1;	\
	vgf2p8affineinvqb $(tf_s2_const), t0, x5, x5;	\
	vgf2p8affineqb $(tf_inv_const), t1, x2, x2;	\
	vgf2p8affineqb $(tf_inv_const), t1, x6, x6;	\
	vgf2p8affineinvqb $0, t2, x2, x2;		\
	vgf2p8affineinvqb $0, t2, x6, x6;		\
	vgf2p8affineinvqb $(tf_aff_const), t3, x0, x0;	\
	vgf2p8affineinvqb $(tf_aff_const), t3, x4, x4;	\
	vgf2p8affineqb $(tf_x2_const), t4, x3, x3;	\
	vgf2p8affineqb $(tf_x2_const), t4, x7, x7;	\
	vgf2p8affineinvqb $0, t2, x3, x3;		\
	vgf2p8affineinvqb $0, t2, x7, x7;		\
	vgf2p8affineinvqb $(tf_s2_const), t0, y1, y1;	\
	vgf2p8affineinvqb $(tf_s2_const), t0, y5, y5;	\
	vgf2p8affineqb $(tf_inv_const), t1, y2, y2;	\
	vgf2p8affineqb $(tf_inv_const), t1, y6, y6;	\
	vgf2p8affineinvqb $0, t2, y2, y2;		\
	vgf2p8affineinvqb $0, t2, y6, y6;		\
	vgf2p8affineinvqb $(tf_aff_const), t3, y0, y0;	\
	vgf2p8affineinvqb $(tf_aff_const), t3, y4, y4;	\
	vgf2p8affineqb $(tf_x2_const), t4, y3, y3;	\
	vgf2p8affineqb $(tf_x2_const), t4, y7, y7;	\
	vgf2p8affineinvqb $0, t2, y3, y3;		\
	vgf2p8affineinvqb $0, t2, y7, y7;

#define aria_diff_m(x0, x1, x2, x3,			\
		    t0, t1, t2, t3)			\
	/* T = rotr32(X, 8); */				\
	/* X ^= T */					\
	/* X = T ^ rotr(X, 16); */			\
	vmovdqa64 x0, t0;				\
	vmovdqa64 x3, t3;				\
	vpternlogq $0x96, x2, x1, x0;			\
	vpternlogq $0x96, x2, x1, x3;			\
	vpternlogq $0x96, t0, t3, x2;			\
	vpternlogq $0x96, t0, t3, x1;

#define aria_diff_word(x0, x1, x2, x3,			\
		       x4, x5, x6, x7,			\
		       y0, y1, y2, y3,			\
		       y4, y5, y6, y7)			\
	/* t1 ^= t2; */					\
	vpxorq y0, x4, x4;				\
	vpxorq y1, x5, x5;				\
	vpxorq y2, x6, x6;				\
	vpxorq y3, x7, x7;				\
							\
	/* t2 ^= t3; */					\
	vpxorq y4, y0, y0;				\
	vpxorq y5, y1, y1;				\
	vpxorq y6, y2, y2;				\
	vpxorq y7, y3, y3;				\
							\
	/* t0 ^= t1; */					\
	vpxorq x4, x0, x0;				\
	vpxorq x5, x1, x1;				\
	vpxorq x6, x2, x2;				\
	vpxorq x7, x3, x3;				\
							\
	/* t3 ^= t1; */					\
	vpxorq x4, y4, y4;				\
	vpxorq x5, y5, y5;				\
	vpxorq x6, y6, y6;				\
	vpxorq x7, y7, y7;				\
							\
	/* t2 ^= t0; */					\
	vpxorq x0, y0, y0;				\
	vpxorq x1, y1, y1;				\
	vpxorq x2, y2, y2;				\
	vpxorq x3, y3, y3;				\
							\
	/* t1 ^= t2; */					\
	vpxorq y0, x4, x4;				\
	vpxorq y1, x5, x5;				\
	vpxorq y2, x6, x6;				\
	vpxorq y3, x7, x7;

#define aria_fe_gfni(x0, x1, x2, x3,			\
		     x4, x5, x6, x7,			\
		     y0, y1, y2, y3,			\
		     y4, y5, y6, y7,			\
		     z0, z1, z2, z3,			\
		     z4, z5, z6, z7,			\
		     mem_tmp, rk, round)		\
	aria_ark_16way(x0, x1, x2, x3, x4, x5, x6, x7,	\
		       y0, y1, y2, y3, y4, y5, y6, y7,	\
		       z0, rk, round);			\
							\
	aria_sbox_16way_gfni(x2, x3, x0, x1,		\
			     x6, x7, x4, x5,		\
			     y2, y3, y0, y1,		\
			     y6, y7, y4, y5,		\
			     z0, z1, z2, z3,		\
			     z4, z5, z6, z7);		\
							\
	aria_diff_m(x0, x1, x2, x3, z0, z1, z2, z3);	\
	aria_diff_m(x4, x5, x6, x7, z0, z1, z2, z3);	\
	aria_diff_m(y0, y1, y2, y3, z0, z1, z2, z3);	\
	aria_diff_m(y4, y5, y6, y7, z0, z1, z2, z3);	\
	aria_diff_word(x0, x1, x2, x3,			\
		       x4, x5, x6, x7,			\
		       y0, y1, y2, y3,			\
		       y4, y5, y6, y7);			\
	/* aria_diff_byte()				\
	 * T3 = ABCD -> BADC				\
	 * T3 = y4, y5, y6, y7 -> y5, y4, y7, y6	\
	 * T0 = ABCD -> CDAB				\
	 * T0 = x0, x1, x2, x3 -> x2, x3, x0, x1	\
	 * T1 = ABCD -> DCBA				\
	 * T1 = x4, x5, x6, x7 -> x7, x6, x5, x4	\
	 */						\
	aria_diff_word(x2, x3, x0, x1,			\
		       x7, x6, x5, x4,			\
		       y0, y1, y2, y3,			\
		       y5, y4, y7, y6);			\


#define aria_fo_gfni(x0, x1, x2, x3,			\
		     x4, x5, x6, x7,			\
		     y0, y1, y2, y3,			\
		     y4, y5, y6, y7,			\
		     z0, z1, z2, z3,			\
		     z4, z5, z6, z7,			\
		     mem_tmp, rk, round)		\
	aria_ark_16way(x0, x1, x2, x3, x4, x5, x6, x7,	\
		       y0, y1, y2, y3, y4, y5, y6, y7,	\
		       z0, rk, round);			\
							\
	aria_sbox_16way_gfni(x0, x1, x2, x3,		\
			     x4, x5, x6, x7,		\
			     y0, y1, y2, y3,		\
			     y4, y5, y6, y7,		\
			     z0, z1, z2, z3,		\
			     z4, z5, z6, z7);		\
							\
	aria_diff_m(x0, x1, x2, x3, z0, z1, z2, z3);	\
	aria_diff_m(x4, x5, x6, x7, z0, z1, z2, z3);	\
	aria_diff_m(y0, y1, y2, y3, z0, z1, z2, z3);	\
	aria_diff_m(y4, y5, y6, y7, z0, z1, z2, z3);	\
	aria_diff_word(x0, x1, x2, x3,			\
		       x4, x5, x6, x7,			\
		       y0, y1, y2, y3,			\
		       y4, y5, y6, y7);			\
	/* aria_diff_byte()				\
	 * T1 = ABCD -> BADC				\
	 * T1 = x4, x5, x6, x7 -> x5, x4, x7, x6	\
	 * T2 = ABCD -> CDAB				\
	 * T2 = y0, y1, y2, y3, -> y2, y3, y0, y1	\
	 * T3 = ABCD -> DCBA				\
	 * T3 = y4, y5, y6, y7 -> y7, y6, y5, y4	\
	 */						\
	aria_diff_word(x0, x1, x2, x3,			\
		       x5, x4, x7, x6,			\
		       y2, y3, y0, y1,			\
		       y7, y6, y5, y4);

#define aria_ff_gfni(x0, x1, x2, x3,			\
		     x4, x5, x6, x7,			\
		     y0, y1, y2, y3,			\
		     y4, y5, y6, y7,			\
		     z0, z1, z2, z3,			\
		     z4, z5, z6, z7,			\
		     mem_tmp, rk, round, last_round)	\
	aria_ark_16way(x0, x1, x2, x3,			\
		       x4, x5, x6, x7,			\
		       y0, y1, y2, y3,			\
		       y4, y5, y6, y7,			\
		       z0, rk, round);			\
	aria_sbox_16way_gfni(x2, x3, x0, x1,		\
			     x6, x7, x4, x5,		\
			     y2, y3, y0, y1,		\
			     y6, y7, y4, y5,		\
			     z0, z1, z2, z3,		\
			     z4, z5, z6, z7);		\
	aria_ark_16way(x0, x1, x2, x3,			\
		       x4, x5, x6, x7,			\
		       y0, y1, y2, y3,			\
		       y4, y5, y6, y7,			\
		       z0, rk, last_round);

SECTION_RODATA
.align 64
.Lcounter0123_lo:
	.quad 0, 0
	.quad 1, 0
	.quad 2, 0
	.quad 3, 0

.align 32
#define SHUFB_BYTES(idx) \
	0 + (idx), 4 + (idx), 8 + (idx), 12 + (idx)
.Lshufb_16x16b:
	.byte SHUFB_BYTES(0), SHUFB_BYTES(1), SHUFB_BYTES(2), SHUFB_BYTES(3)
	.byte SHUFB_BYTES(0), SHUFB_BYTES(1), SHUFB_BYTES(2), SHUFB_BYTES(3)

.align 16
.Lcounter4444_lo:
	.quad 4, 0
.Lcounter8888_lo:
	.quad 8, 0
.Lcounter16161616_lo:
	.quad 16, 0
.Lcounter1111_hi:
	.quad 0, 1

/* For CTR-mode IV byteswap */
.Lbswap128_mask:
	.byte 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08
	.byte 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00

.align 8
/* AES affine: */
#define tf_aff_const BV8(1, 1, 0, 0, 0, 1, 1, 0)
.Ltf_aff_bitmatrix:
	.quad BM8X8(BV8(1, 0, 0, 0, 1, 1, 1, 1),
		    BV8(1, 1, 0, 0, 0, 1, 1, 1),
		    BV8(1, 1, 1, 0, 0, 0, 1, 1),
		    BV8(1, 1, 1, 1, 0, 0, 0, 1),
		    BV8(1, 1, 1, 1, 1, 0, 0, 0),
		    BV8(0, 1, 1, 1, 1, 1, 0, 0),
		    BV8(0, 0, 1, 1, 1, 1, 1, 0),
		    BV8(0, 0, 0, 1, 1, 1, 1, 1))

/* AES inverse affine: */
#define tf_inv_const BV8(1, 0, 1, 0, 0, 0, 0, 0)
.Ltf_inv_bitmatrix:
	.quad BM8X8(BV8(0, 0, 1, 0, 0, 1, 0, 1),
		    BV8(1, 0, 0, 1, 0, 0, 1, 0),
		    BV8(0, 1, 0, 0, 1, 0, 0, 1),
		    BV8(1, 0, 1, 0, 0, 1, 0, 0),
		    BV8(0, 1, 0, 1, 0, 0, 1, 0),
		    BV8(0, 0, 1, 0, 1, 0, 0, 1),
		    BV8(1, 0, 0, 1, 0, 1, 0, 0),
		    BV8(0, 1, 0, 0, 1, 0, 1, 0))

/* S2: */
#define tf_s2_const BV8(0, 1, 0, 0, 0, 1, 1, 1)
.Ltf_s2_bitmatrix:
	.quad BM8X8(BV8(0, 1, 0, 1, 0, 1, 1, 1),
		    BV8(0, 0, 1, 1, 1, 1, 1, 1),
		    BV8(1, 1, 1, 0, 1, 1, 0, 1),
		    BV8(1, 1, 0, 0, 0, 0, 1, 1),
		    BV8(0, 1, 0, 0, 0, 0, 1, 1),
		    BV8(1, 1, 0, 0, 1, 1, 1, 0),
		    BV8(0, 1, 1, 0, 0, 0, 1, 1),
		    BV8(1, 1, 1, 1, 0, 1, 1, 0))

/* X2: */
#define tf_x2_const BV8(0, 0, 1, 1, 0, 1, 0, 0)
.Ltf_x2_bitmatrix:
	.quad BM8X8(BV8(0, 0, 0, 1, 1, 0, 0, 0),
		    BV8(0, 0, 1, 0, 0, 1, 1, 0),
		    BV8(0, 0, 0, 0, 1, 0, 1, 0),
		    BV8(1, 1, 1, 0, 0, 0, 1, 1),
		    BV8(1, 1, 1, 0, 1, 1, 0, 0),
		    BV8(0, 1, 1, 0, 1, 0, 1, 1),
		    BV8(1, 0, 1, 1, 1, 1, 0, 1),
		    BV8(1, 0, 0, 1, 0, 0, 1, 1))

/* Identity matrix: */
.Ltf_id_bitmatrix:
	.quad BM8X8(BV8(1, 0, 0, 0, 0, 0, 0, 0),
		    BV8(0, 1, 0, 0, 0, 0, 0, 0),
		    BV8(0, 0, 1, 0, 0, 0, 0, 0),
		    BV8(0, 0, 0, 1, 0, 0, 0, 0),
		    BV8(0, 0, 0, 0, 1, 0, 0, 0),
		    BV8(0, 0, 0, 0, 0, 1, 0, 0),
		    BV8(0, 0, 0, 0, 0, 0, 1, 0),
		    BV8(0, 0, 0, 0, 0, 0, 0, 1))

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

.text

.align 16
ELF(.type __aria_gfni_avx512_crypt_64way,@function;)
__aria_gfni_avx512_crypt_64way:
	/* input:
	 *      %r9: rk
	 *      %rsi: dst
	 *      %rdx: src
	 *      %zmm0..%zmm15: byte-sliced blocks
	 */
	CFI_STARTPROC();

	movq %rsi, %rax;
	leaq 8 * 64(%rax), %r8;

	movl ARIA_CTX_rounds(CTX), %r10d;
	subl $2, %r10d;

	inpack16_post(%zmm0, %zmm1, %zmm2, %zmm3,
		      %zmm4, %zmm5, %zmm6, %zmm7,
		      %zmm8, %zmm9, %zmm10, %zmm11,
		      %zmm12, %zmm13, %zmm14,
		      %zmm15, %rax, %r8);
	aria_fo_gfni(%zmm0, %zmm1, %zmm2, %zmm3,
		     %zmm4, %zmm5, %zmm6, %zmm7,
		     %zmm8, %zmm9, %zmm10, %zmm11,
		     %zmm12, %zmm13, %zmm14, %zmm15,
		     %zmm24, %zmm25, %zmm26, %zmm27,
		     %zmm28, %zmm29, %zmm30, %zmm31,
		     %rax, %r9, 0);
	leaq 1*16(%r9), %r9;

.align 16
.Loop_gfni:
	aria_fe_gfni(%zmm3, %zmm2, %zmm1, %zmm0,
		     %zmm6, %zmm7, %zmm4, %zmm5,
		     %zmm9, %zmm8, %zmm11, %zmm10,
		     %zmm12, %zmm13, %zmm14, %zmm15,
		     %zmm24, %zmm25, %zmm26, %zmm27,
		     %zmm28, %zmm29, %zmm30, %zmm31,
		     %rax, %r9, 0);
	aria_fo_gfni(%zmm0, %zmm1, %zmm2, %zmm3,
		     %zmm4, %zmm5, %zmm6, %zmm7,
		     %zmm8, %zmm9, %zmm10, %zmm11,
		     %zmm12, %zmm13, %zmm14, %zmm15,
		     %zmm24, %zmm25, %zmm26, %zmm27,
		     %zmm28, %zmm29, %zmm30, %zmm31,
		     %rax, %r9, 1);
	leaq 2*16(%r9), %r9;
	subl $2, %r10d;
	jnz .Loop_gfni;

	aria_ff_gfni(%zmm3, %zmm2, %zmm1, %zmm0,
		     %zmm6, %zmm7, %zmm4, %zmm5,
		     %zmm9, %zmm8, %zmm11, %zmm10,
		     %zmm12, %zmm13, %zmm14, %zmm15,
		     %zmm24, %zmm25, %zmm26, %zmm27,
		     %zmm28, %zmm29, %zmm30, %zmm31,
		     %rax, %r9, 0, 1);

	debyteslice_16x16b(%zmm9, %zmm12, %zmm3, %zmm6,
			   %zmm8, %zmm13, %zmm2, %zmm7,
			   %zmm11, %zmm14, %zmm1, %zmm4,
			   %zmm10, %zmm15, %zmm0, %zmm5,
			   (%rax), (%r8));

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __aria_gfni_avx512_crypt_64way,.-__aria_gfni_avx512_crypt_64way;)

.align 16
.globl _gcry_aria_gfni_avx512_ecb_crypt_blk64
ELF(.type _gcry_aria_gfni_avx512_ecb_crypt_blk64,@function;)
_gcry_aria_gfni_avx512_ecb_crypt_blk64:
	/* input:
	 *      %rdi: ctx, CTX
	 *      %rsi: dst
	 *      %rdx: src
	 *      %rcx: round keys
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	pushq %rbp;
	CFI_PUSH(%rbp);
	movq %rsp, %rbp;
	CFI_DEF_CFA_REGISTER(%rbp);

	subq $(16 * 64), %rsp;
	andq $~63, %rsp;

	movq %rcx, %r9;
	movq %rsi, %r11;
	movq %rsp, %rsi; /* use stack for temporary store */

	inpack16_pre(%zmm0, %zmm1, %zmm2, %zmm3, %zmm4, %zmm5, %zmm6, %zmm7,
		     %zmm8, %zmm9, %zmm10, %zmm11, %zmm12, %zmm13, %zmm14,
		     %zmm15, %rdx);

	call __aria_gfni_avx512_crypt_64way;

	write_output(%zmm3, %zmm2, %zmm1, %zmm0, %zmm6, %zmm7, %zmm4, %zmm5,
		     %zmm9, %zmm8, %zmm11, %zmm10, %zmm12, %zmm13, %zmm14,
		     %zmm15, %r11);

	movl $STACK_DEPTH, %eax;
	leave;
	CFI_LEAVE();
	clear_regs();
	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_aria_gfni_avx512_ecb_crypt_blk64,
	  .-_gcry_aria_gfni_avx512_ecb_crypt_blk64;)

.align 16
ELF(.type __aria_gfni_avx512_ctr_gen_keystream_64way,@function;)
__aria_gfni_avx512_ctr_gen_keystream_64way:
	/* input:
	 *      %rdi: ctx
	 *      %rsi: dst
	 *      %rdx: src
	 *      %rcx: keystream
	 *      %r8: iv (big endian, 128bit)
	 */
	CFI_STARTPROC();

	cmpb $(0x100 - 64), 15(%r8);
	jbe .Lctr_byteadd;

	vbroadcasti64x2 .Lbswap128_mask rRIP, %zmm19;
	vmovdqa64 .Lcounter0123_lo rRIP, %zmm21;
	vbroadcasti64x2 .Lcounter4444_lo rRIP, %zmm22;
	vbroadcasti64x2 .Lcounter8888_lo rRIP, %zmm23;
	vbroadcasti64x2 .Lcounter16161616_lo rRIP, %zmm24;
	vbroadcasti64x2 .Lcounter1111_hi rRIP, %zmm25;

	/* load IV and byteswap */
	movq 8(%r8), %r11;
	movq (%r8), %r10;
	bswapq %r11;
	bswapq %r10;
	vbroadcasti64x2 (%r8), %zmm20;
	vpshufb %zmm19, %zmm20, %zmm20;

	/* check need for handling 64-bit overflow and carry */
	cmpq $(0xffffffffffffffff - 64), %r11;
	ja .Lload_ctr_carry;

	/* construct IVs */
	vpaddq %zmm21, %zmm20, %zmm0; /* +0:+1:+2:+3 */
	vpaddq %zmm22, %zmm0, %zmm1; /* +4:+5:+6:+7 */
	vpaddq %zmm23, %zmm0, %zmm2; /* +8:+9:+10:+11 */
	vpaddq %zmm23, %zmm1, %zmm3; /* +12:+13:+14:+15 */
	vpaddq %zmm24, %zmm0, %zmm4; /* +16... */
	vpaddq %zmm24, %zmm1, %zmm5; /* +20... */
	vpaddq %zmm24, %zmm2, %zmm6; /* +24... */
	vpaddq %zmm24, %zmm3, %zmm7; /* +28... */
	vpaddq %zmm24, %zmm4, %zmm8; /* +32... */
	vpaddq %zmm24, %zmm5, %zmm9; /* +36... */
	vpaddq %zmm24, %zmm6, %zmm10; /* +40... */
	vpaddq %zmm24, %zmm7, %zmm11; /* +44... */
	vpaddq %zmm24, %zmm8, %zmm12; /* +48... */
	vpaddq %zmm24, %zmm9, %zmm13; /* +52... */
	vpaddq %zmm24, %zmm10, %zmm14; /* +56... */
	vpaddq %zmm24, %zmm11, %zmm15; /* +60... */
	jmp .Lload_ctr_done;

.Lload_ctr_carry:
	/* construct IVs */
	add_le128(%zmm0, %zmm20, %zmm21, %zmm25); /* +0:+1:+2:+3 */
	add_le128(%zmm1, %zmm0, %zmm22, %zmm25); /* +4:+5:+6:+7 */
	add_le128(%zmm2, %zmm0, %zmm23, %zmm25); /* +8:+9:+10:+11 */
	add_le128(%zmm3, %zmm1, %zmm23, %zmm25); /* +12:+13:+14:+15 */
	add_le128(%zmm4, %zmm0, %zmm24, %zmm25); /* +16... */
	add_le128(%zmm5, %zmm1, %zmm24, %zmm25); /* +20... */
	add_le128(%zmm6, %zmm2, %zmm24, %zmm25); /* +24... */
	add_le128(%zmm7, %zmm3, %zmm24, %zmm25); /* +28... */
	add_le128(%zmm8, %zmm4, %zmm24, %zmm25); /* +32... */
	add_le128(%zmm9, %zmm5, %zmm24, %zmm25); /* +36... */
	add_le128(%zmm10, %zmm6, %zmm24, %zmm25); /* +40... */
	add_le128(%zmm11, %zmm7, %zmm24, %zmm25); /* +44... */
	add_le128(%zmm12, %zmm8, %zmm24, %zmm25); /* +48... */
	add_le128(%zmm13, %zmm9, %zmm24, %zmm25); /* +52... */
	add_le128(%zmm14, %zmm10, %zmm24, %zmm25); /* +56... */
	add_le128(%zmm15, %zmm11, %zmm24, %zmm25); /* +60... */

.Lload_ctr_done:
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
	movq %r11, 8(%r8);
	movq %r10, (%r8);

	ret_spec_stop;

.align 16
.Lctr_byteadd_full_ctr_carry:
	movq 8(%r8), %r11;
	movq (%r8), %r10;
	bswapq %r11;
	bswapq %r10;
	addq $64, %r11;
	adcq $0, %r10;
	bswapq %r11;
	bswapq %r10;
	movq %r11, 8(%r8);
	movq %r10, (%r8);
	jmp .Lctr_byteadd_zmm;
.align 16
.Lctr_byteadd:
	vbroadcasti64x2 (%r8), %zmm3;
	je .Lctr_byteadd_full_ctr_carry;
	addb $64, 15(%r8);
.Lctr_byteadd_zmm:
	vbroadcasti64x2 .Lbige_addb_16 rRIP, %zmm16;
	vmovdqa64 .Lbige_addb_0_1 rRIP, %zmm17;
	vmovdqa64 .Lbige_addb_4_5 rRIP, %zmm18;
	vmovdqa64 .Lbige_addb_8_9 rRIP, %zmm19;
	vmovdqa64 .Lbige_addb_12_13 rRIP, %zmm20;
	vpaddb %zmm16, %zmm3, %zmm7;
	vpaddb %zmm17, %zmm3, %zmm0;
	vpaddb %zmm18, %zmm3, %zmm1;
	vpaddb %zmm19, %zmm3, %zmm2;
	vpaddb %zmm20, %zmm3, %zmm3;
	vpaddb %zmm16, %zmm7, %zmm11;
	vpaddb %zmm17, %zmm7, %zmm4;
	vpaddb %zmm18, %zmm7, %zmm5;
	vpaddb %zmm19, %zmm7, %zmm6;
	vpaddb %zmm20, %zmm7, %zmm7;
	vpaddb %zmm16, %zmm11, %zmm15;
	vpaddb %zmm17, %zmm11, %zmm8;
	vpaddb %zmm18, %zmm11, %zmm9;
	vpaddb %zmm19, %zmm11, %zmm10;
	vpaddb %zmm20, %zmm11, %zmm11;
	vpaddb %zmm17, %zmm15, %zmm12;
	vpaddb %zmm18, %zmm15, %zmm13;
	vpaddb %zmm19, %zmm15, %zmm14;
	vpaddb %zmm20, %zmm15, %zmm15;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __aria_gfni_avx512_ctr_gen_keystream_64way,
	  .-__aria_gfni_avx512_ctr_gen_keystream_64way;)

.align 16
.globl _gcry_aria_gfni_avx512_ctr_crypt_blk64
ELF(.type _gcry_aria_gfni_avx512_ctr_crypt_blk64,@function;)
_gcry_aria_gfni_avx512_ctr_crypt_blk64:
	/* input:
	 *      %rdi: ctx
	 *      %rsi: dst
	 *      %rdx: src
	 *      %rcx: iv (big endian, 128bit)
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	pushq %rbp;
	CFI_PUSH(%rbp);
	movq %rsp, %rbp;
	CFI_DEF_CFA_REGISTER(%rbp);

	subq $(16 * 64), %rsp;
	andq $~63, %rsp;

	movq %rcx, %r8;  /* %r8: iv */
	movq %rsp, %rcx; /* %rcx: keystream */
	call __aria_gfni_avx512_ctr_gen_keystream_64way

	pushq %rsi;
	movq %rdx, %r11;
	movq %rcx, %rsi;
	movq %rcx, %rdx;
	leaq ARIA_CTX_enc_key(CTX), %r9;

	call __aria_gfni_avx512_crypt_64way;

	popq %rsi;
	vpxorq (0 * 64)(%r11), %zmm3, %zmm3;
	vpxorq (1 * 64)(%r11), %zmm2, %zmm2;
	vpxorq (2 * 64)(%r11), %zmm1, %zmm1;
	vpxorq (3 * 64)(%r11), %zmm0, %zmm0;
	vpxorq (4 * 64)(%r11), %zmm6, %zmm6;
	vpxorq (5 * 64)(%r11), %zmm7, %zmm7;
	vpxorq (6 * 64)(%r11), %zmm4, %zmm4;
	vpxorq (7 * 64)(%r11), %zmm5, %zmm5;
	vpxorq (8 * 64)(%r11), %zmm9, %zmm9;
	vpxorq (9 * 64)(%r11), %zmm8, %zmm8;
	vpxorq (10 * 64)(%r11), %zmm11, %zmm11;
	vpxorq (11 * 64)(%r11), %zmm10, %zmm10;
	vpxorq (12 * 64)(%r11), %zmm12, %zmm12;
	vpxorq (13 * 64)(%r11), %zmm13, %zmm13;
	vpxorq (14 * 64)(%r11), %zmm14, %zmm14;
	vpxorq (15 * 64)(%r11), %zmm15, %zmm15;
	write_output(%zmm3, %zmm2, %zmm1, %zmm0, %zmm6, %zmm7, %zmm4, %zmm5,
		     %zmm9, %zmm8, %zmm11, %zmm10, %zmm12, %zmm13, %zmm14,
		     %zmm15, %rsi);

	movl $STACK_DEPTH, %eax;
	leave;
	CFI_LEAVE();
	clear_regs();
	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_aria_gfni_avx512_ctr_crypt_blk64,
	  .-_gcry_aria_gfni_avx512_ctr_crypt_blk64;)

#endif /* ENABLE_AVX512_SUPPORT && ENABLE_GFNI_SUPPORT */
#endif /* __x86_64 */
