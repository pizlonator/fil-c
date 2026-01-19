/* sha512-avx512-amd64.c - amd64/AVX512 implementation of SHA-512 transform
 * Copyright (C) 2022 Jussi Kivilinna <jussi.kivilinna@iki.fi>
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
/*
 * Based on implementation from file "sha512-avx2-bmi2-amd64.S":
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; Copyright (c) 2012, Intel Corporation
;
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are
; met:
;
; * Redistributions of source code must retain the above copyright
;   notice, this list of conditions and the following disclaimer.
;
; * Redistributions in binary form must reproduce the above copyright
;   notice, this list of conditions and the following disclaimer in the
;   documentation and/or other materials provided with the
;   distribution.
;
; * Neither the name of the Intel Corporation nor the names of its
;   contributors may be used to endorse or promote products derived from
;   this software without specific prior written permission.
;
;
; THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY
; EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
; IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
; PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL CORPORATION OR
; CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
; EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
; PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
; PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
; LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
; NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; This code schedules 1 blocks at a time, with 4 lanes per block
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
*/

#ifdef __x86_64
#include <config.h>
#if (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
     defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS)) && \
    defined(HAVE_INTEL_SYNTAX_PLATFORM_AS) && \
    defined(HAVE_GCC_INLINE_ASM_AVX512) && \
    defined(USE_SHA512)

#include "asm-common-amd64.h"

.intel_syntax noprefix

.text

/* Virtual Registers */
#define Y_0 ymm0
#define Y_1 ymm1
#define Y_2 ymm2
#define Y_3 ymm3

#define YTMP0 ymm4
#define YTMP1 ymm5
#define YTMP2 ymm6
#define YTMP3 ymm7
#define YTMP4 ymm8
#define XFER YTMP0

#define BYTE_FLIP_MASK ymm9
#define PERM_VPALIGNR_8 ymm10

#define MASK_DC_00 k1

#define INP rdi /* 1st arg */
#define CTX rsi /* 2nd arg */
#define NUM_BLKS rdx /* 3rd arg */
#define SRND r8d
#define RSP_SAVE r9

#define TBL rcx

#define a xmm11
#define b xmm12
#define c xmm13
#define d xmm14
#define e xmm15
#define f xmm16
#define g xmm17
#define h xmm18

#define y0 xmm19
#define y1 xmm20
#define y2 xmm21
#define y3 xmm22

/* Local variables (stack frame) */
#define frame_XFER         0
#define frame_XFER_size    (4*4*8)
#define frame_size         (frame_XFER + frame_XFER_size)

#define clear_reg(x) vpxorq x,x,x

/* addm [mem], reg */
/* Add reg to mem using reg-mem add and store */
#define addm(p1, p2) \
	vmovq	y0, p1; \
	vpaddq	p2, p2, y0; \
	vmovq	p1, p2;

/* COPY_YMM_AND_BSWAP ymm, [mem], byte_flip_mask */
/* Load ymm with mem and byte swap each dword */
#define COPY_YMM_AND_BSWAP(p1, p2, p3) \
	vmovdqu p1, p2; \
	vpshufb p1, p1, p3

/* %macro MY_VPALIGNR	YDST, YSRC1, YSRC2, RVAL */
/* YDST = {YSRC1, YSRC2} >> RVAL*8 */
#define MY_VPALIGNR(YDST_SRC1, YSRC2, RVAL) \
	vpermt2q YDST_SRC1, PERM_VPALIGNR_##RVAL, YSRC2;

#define ONE_ROUND_PART1(XFERIN, a, b, c, d, e, f, g, h) \
	/* h += Sum1 (e) + Ch (e, f, g) + (k[t] + w[0]); \
	 * d += h; \
	 * h += Sum0 (a) + Maj (a, b, c); \
	 * \
	 * Ch(x, y, z) => ((x & y) + (~x & z)) \
	 * Maj(x, y, z) => ((x & y) + (z & (x ^ y))) \
	 */ \
	\
	vmovq y3, [XFERIN]; \
	vmovdqa64 y2, e; \
	vpaddq h, h, y3; \
	vprorq y0, e, 41; \
	vpternlogq y2, f, g, 0xca; /* Ch (e, f, g) */ \
	vprorq y1, e, 18; \
	vprorq y3, e, 14; \
	vpaddq h, h, y2; \
	vpternlogq y0, y1, y3, 0x96; /* Sum1 (e) */ \
	vpaddq h, h, y0; /* h += Sum1 (e) + Ch (e, f, g) + (k[t] + w[0]) */ \
	vpaddq d, d, h; /* d += h */

#define ONE_ROUND_PART2(a, b, c, d, e, f, g, h) \
	vmovdqa64 y1, a; \
	vprorq y0, a, 39; \
	vpternlogq y1, b, c, 0xe8; /* Maj (a, b, c) */ \
	vprorq y2, a, 34; \
	vprorq y3, a, 28; \
	vpternlogq y0, y2, y3, 0x96; /* Sum0 (a) */ \
	vpaddq h, h, y1; \
	vpaddq h, h, y0; /* h += Sum0 (a) + Maj (a, b, c) */

#define FOUR_ROUNDS_AND_SCHED(X, Y_0, Y_1, Y_2, Y_3, a, b, c, d, e, f, g, h) \
	/*;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; RND N + 0 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; */; \
		vmovdqa		YTMP0, Y_3; \
		vmovdqa		YTMP1, Y_1; \
		/* Extract w[t-7] */; \
		vpermt2q	YTMP0, PERM_VPALIGNR_8, Y_2	/* YTMP0 = W[-7] */; \
		/* Calculate w[t-16] + w[t-7] */; \
		vpaddq		YTMP0, YTMP0, Y_0		/* YTMP0 = W[-7] + W[-16] */; \
		/* Extract w[t-15] */; \
		vpermt2q	YTMP1, PERM_VPALIGNR_8, Y_0	/* YTMP1 = W[-15] */; \
	ONE_ROUND_PART1(rsp+frame_XFER+0*8+X*32, a, b, c, d, e, f, g, h); \
		\
		/* Calculate sigma0 */; \
		\
		/* Calculate w[t-15] ror 1 */; \
		vprorq		YTMP3, YTMP1, 1;		/* YTMP3 = W[-15] ror 1 */; \
		/* Calculate w[t-15] shr 7 */; \
		vpsrlq		YTMP4, YTMP1, 7			/* YTMP4 = W[-15] >> 7 */; \
	\
	ONE_ROUND_PART2(a, b, c, d, e, f, g, h); \
	\
	/*;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; RND N + 1 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; */; \
		/* Calculate w[t-15] ror 8 */; \
		vprorq		YTMP1, YTMP1, 8			/* YTMP1 = W[-15] ror 8 */; \
		/* XOR the three components */; \
		vpternlogq	YTMP1, YTMP3, YTMP4, 0x96	/* YTMP1 = s0 = W[-15] ror 1 ^ W[-15] >> 7 ^ W[-15] ror 8 */; \
		\
		/* Add three components, w[t-16], w[t-7] and sigma0 */; \
		vpaddq		YTMP0, YTMP0, YTMP1		/* YTMP0 = W[-16] + W[-7] + s0 */; \
	ONE_ROUND_PART1(rsp+frame_XFER+1*8+X*32, h, a, b, c, d, e, f, g); \
		/* Move to appropriate lanes for calculating w[16] and w[17] */; \
		vshufi64x2	Y_0, YTMP0, YTMP0, 0x0		/* Y_0 = W[-16] + W[-7] + s0 {BABA} */; \
		\
		/* Calculate w[16] and w[17] in both 128 bit lanes */; \
		\
		/* Calculate sigma1 for w[16] and w[17] on both 128 bit lanes */; \
		vshufi64x2	YTMP2, Y_3, Y_3, 0b11		/* YTMP2 = W[-2] {BABA} */; \
		vpsrlq		YTMP4, YTMP2, 6			/* YTMP4 = W[-2] >> 6 {BABA} */; \
	\
	ONE_ROUND_PART2(h, a, b, c, d, e, f, g); \
	\
	/*;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; RND N + 2 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; */; \
		vprorq		YTMP3, YTMP2, 19		/* YTMP3 = W[-2] ror 19 {BABA} */; \
		vprorq		YTMP1, YTMP2, 61		/* YTMP3 = W[-2] ror 61 {BABA} */; \
		vpternlogq	YTMP4, YTMP3, YTMP1, 0x96	/* YTMP4 = s1 = (W[-2] ror 19) ^ (W[-2] ror 61) ^ (W[-2] >> 6) {BABA} */; \
		\
	ONE_ROUND_PART1(rsp+frame_XFER+2*8+X*32, g, h, a, b, c, d, e, f); \
		/* Add sigma1 to the other compunents to get w[16] and w[17] */; \
		vpaddq		Y_0, Y_0, YTMP4			/* Y_0 = {W[1], W[0], W[1], W[0]} */; \
		\
		/* Calculate sigma1 for w[18] and w[19] for upper 128 bit lane */; \
		vpsrlq		YTMP4, Y_0, 6			/* YTMP4 = W[-2] >> 6 {DC--} */; \
	\
	ONE_ROUND_PART2(g, h, a, b, c, d, e, f); \
	\
	/*;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; RND N + 3 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; */; \
		vprorq		YTMP3, Y_0, 19			/* YTMP3 = W[-2] ror 19 {DC--} */; \
		vprorq		YTMP1, Y_0, 61			/* YTMP1 = W[-2] ror 61 {DC--} */; \
		vpternlogq	YTMP4, YTMP3, YTMP1, 0x96	/* YTMP4 = s1 = (W[-2] ror 19) ^ (W[-2] ror 61) ^ (W[-2] >> 6) {DC--} */; \
		\
	ONE_ROUND_PART1(rsp+frame_XFER+3*8+X*32, f, g, h, a, b, c, d, e); \
		/* Add the sigma0 + w[t-7] + w[t-16] for w[18] and w[19] to newly calculated sigma1 to get w[18] and w[19] */; \
		/* Form w[19, w[18], w17], w[16] */; \
		vpaddq		Y_0{MASK_DC_00}, YTMP0, YTMP4	/* YTMP2 = {W[3], W[2], W[1], W[0]} */; \
		\
		vpaddq		XFER, Y_0, [TBL + (4+X)*32]; \
		vmovdqa		[rsp + frame_XFER + X*32], XFER; \
	ONE_ROUND_PART2(f, g, h, a, b, c, d, e)

#define ONE_ROUND(XFERIN, a, b, c, d, e, f, g, h) \
	ONE_ROUND_PART1(XFERIN, a, b, c, d, e, f, g, h); \
	ONE_ROUND_PART2(a, b, c, d, e, f, g, h)

#define DO_4ROUNDS(X, a, b, c, d, e, f, g, h) \
	ONE_ROUND(rsp+frame_XFER+0*8+X*32, a, b, c, d, e, f, g, h); \
	ONE_ROUND(rsp+frame_XFER+1*8+X*32, h, a, b, c, d, e, f, g); \
	ONE_ROUND(rsp+frame_XFER+2*8+X*32, g, h, a, b, c, d, e, f); \
	ONE_ROUND(rsp+frame_XFER+3*8+X*32, f, g, h, a, b, c, d, e)

/*
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; void sha512_avx512(const void* M, void* D, uint64_t L);
; Purpose: Updates the SHA512 digest stored at D with the message stored in M.
; The size of the message pointed to by M must be an integer multiple of SHA512
;   message blocks.
; L is the message length in SHA512 blocks
*/
.globl _gcry_sha512_transform_amd64_avx512
ELF(.type _gcry_sha512_transform_amd64_avx512,@function;)
.align 16
_gcry_sha512_transform_amd64_avx512:
	CFI_STARTPROC()
	xor	eax, eax

	cmp	rdx, 0
	je	.Lnowork

	spec_stop_avx512_intel_syntax;

	/* Setup mask register for DC:BA merging. */
	mov	eax, 0b1100
	kmovd	MASK_DC_00, eax

	/* Allocate Stack Space */
	mov	RSP_SAVE, rsp
	CFI_DEF_CFA_REGISTER(RSP_SAVE);
	sub	rsp, frame_size
	and	rsp, ~(0x40 - 1)

	/*; load initial digest */
	vmovq	a,[8*0 + CTX]
	vmovq	b,[8*1 + CTX]
	vmovq	c,[8*2 + CTX]
	vmovq	d,[8*3 + CTX]
	vmovq	e,[8*4 + CTX]
	vmovq	f,[8*5 + CTX]
	vmovq	g,[8*6 + CTX]
	vmovq	h,[8*7 + CTX]

	vmovdqa	BYTE_FLIP_MASK, [.LPSHUFFLE_BYTE_FLIP_MASK ADD_RIP]
	vpmovzxbq PERM_VPALIGNR_8, [.LPERM_VPALIGNR_8 ADD_RIP]

	lea	TBL,[.LK512 ADD_RIP]

	/*; byte swap first 16 dwords */
	COPY_YMM_AND_BSWAP(Y_0, [INP + 0*32], BYTE_FLIP_MASK)
	COPY_YMM_AND_BSWAP(Y_1, [INP + 1*32], BYTE_FLIP_MASK)
	COPY_YMM_AND_BSWAP(Y_2, [INP + 2*32], BYTE_FLIP_MASK)
	COPY_YMM_AND_BSWAP(Y_3, [INP + 3*32], BYTE_FLIP_MASK)

	lea	INP, [INP + 128]

	vpaddq	XFER, Y_0, [TBL + 0*32]
	vmovdqa	[rsp + frame_XFER + 0*32], XFER
	vpaddq	XFER, Y_1, [TBL + 1*32]
	vmovdqa	[rsp + frame_XFER + 1*32], XFER
	vpaddq	XFER, Y_2, [TBL + 2*32]
	vmovdqa	[rsp + frame_XFER + 2*32], XFER
	vpaddq	XFER, Y_3, [TBL + 3*32]
	vmovdqa	[rsp + frame_XFER + 3*32], XFER

	/*; schedule 64 input dwords, by doing 12 rounds of 4 each */
	mov	SRND, 4

.align 16
.Loop0:
	FOUR_ROUNDS_AND_SCHED(0, Y_0, Y_1, Y_2, Y_3, a, b, c, d, e, f, g, h)
	FOUR_ROUNDS_AND_SCHED(1, Y_1, Y_2, Y_3, Y_0, e, f, g, h, a, b, c, d)
	FOUR_ROUNDS_AND_SCHED(2, Y_2, Y_3, Y_0, Y_1, a, b, c, d, e, f, g, h)
	FOUR_ROUNDS_AND_SCHED(3, Y_3, Y_0, Y_1, Y_2, e, f, g, h, a, b, c, d)
	lea	TBL, [TBL + 4*32]

	sub	SRND, 1
	jne	.Loop0

	sub	NUM_BLKS, 1
	je	.Ldone_hash

	lea	TBL, [.LK512 ADD_RIP]

	/* load next block and byte swap */
	COPY_YMM_AND_BSWAP(Y_0, [INP + 0*32], BYTE_FLIP_MASK)
	COPY_YMM_AND_BSWAP(Y_1, [INP + 1*32], BYTE_FLIP_MASK)
	COPY_YMM_AND_BSWAP(Y_2, [INP + 2*32], BYTE_FLIP_MASK)
	COPY_YMM_AND_BSWAP(Y_3, [INP + 3*32], BYTE_FLIP_MASK)

	lea	INP, [INP + 128]

	DO_4ROUNDS(0, a, b, c, d, e, f, g, h)
	vpaddq	XFER, Y_0, [TBL + 0*32]
	vmovdqa	[rsp + frame_XFER + 0*32], XFER
	DO_4ROUNDS(1, e, f, g, h, a, b, c, d)
	vpaddq	XFER, Y_1, [TBL + 1*32]
	vmovdqa	[rsp + frame_XFER + 1*32], XFER
	DO_4ROUNDS(2, a, b, c, d, e, f, g, h)
	vpaddq	XFER, Y_2, [TBL + 2*32]
	vmovdqa	[rsp + frame_XFER + 2*32], XFER
	DO_4ROUNDS(3, e, f, g, h, a, b, c, d)
	vpaddq	XFER, Y_3, [TBL + 3*32]
	vmovdqa	[rsp + frame_XFER + 3*32], XFER

	addm([8*0 + CTX],a)
	addm([8*1 + CTX],b)
	addm([8*2 + CTX],c)
	addm([8*3 + CTX],d)
	addm([8*4 + CTX],e)
	addm([8*5 + CTX],f)
	addm([8*6 + CTX],g)
	addm([8*7 + CTX],h)

	/*; schedule 64 input dwords, by doing 12 rounds of 4 each */
	mov	SRND, 4

	jmp	.Loop0

.Ldone_hash:
	DO_4ROUNDS(0, a, b, c, d, e, f, g, h)
	DO_4ROUNDS(1, e, f, g, h, a, b, c, d)
	DO_4ROUNDS(2, a, b, c, d, e, f, g, h)
	DO_4ROUNDS(3, e, f, g, h, a, b, c, d)

	addm([8*0 + CTX],a)
	xor	eax, eax /* burn stack */
	addm([8*1 + CTX],b)
	addm([8*2 + CTX],c)
	addm([8*3 + CTX],d)
	addm([8*4 + CTX],e)
	addm([8*5 + CTX],f)
	addm([8*6 + CTX],g)
	addm([8*7 + CTX],h)
	kxord MASK_DC_00, MASK_DC_00, MASK_DC_00

	vzeroall
	vmovdqa	[rsp + frame_XFER + 0*32], ymm0 /* burn stack */
	vmovdqa	[rsp + frame_XFER + 1*32], ymm0 /* burn stack */
	vmovdqa	[rsp + frame_XFER + 2*32], ymm0 /* burn stack */
	vmovdqa	[rsp + frame_XFER + 3*32], ymm0 /* burn stack */
	clear_reg(ymm16);
	clear_reg(ymm17);
	clear_reg(ymm18);
	clear_reg(ymm19);
	clear_reg(ymm20);
	clear_reg(ymm21);
	clear_reg(ymm22);

	/* Restore Stack Pointer */
	mov	rsp, RSP_SAVE
	CFI_DEF_CFA_REGISTER(rsp)

.Lnowork:
	ret_spec_stop
	CFI_ENDPROC()
ELF(.size _gcry_sha512_transform_amd64_avx512,.-_gcry_sha512_transform_amd64_avx512)

/*;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; */
/*;; Binary Data */

SECTION_RODATA

ELF(.type _gcry_sha512_avx512_consts,@object)
_gcry_sha512_avx512_consts:
.align 64
/* K[t] used in SHA512 hashing */
.LK512:
	.quad	0x428a2f98d728ae22,0x7137449123ef65cd
	.quad	0xb5c0fbcfec4d3b2f,0xe9b5dba58189dbbc
	.quad	0x3956c25bf348b538,0x59f111f1b605d019
	.quad	0x923f82a4af194f9b,0xab1c5ed5da6d8118
	.quad	0xd807aa98a3030242,0x12835b0145706fbe
	.quad	0x243185be4ee4b28c,0x550c7dc3d5ffb4e2
	.quad	0x72be5d74f27b896f,0x80deb1fe3b1696b1
	.quad	0x9bdc06a725c71235,0xc19bf174cf692694
	.quad	0xe49b69c19ef14ad2,0xefbe4786384f25e3
	.quad	0x0fc19dc68b8cd5b5,0x240ca1cc77ac9c65
	.quad	0x2de92c6f592b0275,0x4a7484aa6ea6e483
	.quad	0x5cb0a9dcbd41fbd4,0x76f988da831153b5
	.quad	0x983e5152ee66dfab,0xa831c66d2db43210
	.quad	0xb00327c898fb213f,0xbf597fc7beef0ee4
	.quad	0xc6e00bf33da88fc2,0xd5a79147930aa725
	.quad	0x06ca6351e003826f,0x142929670a0e6e70
	.quad	0x27b70a8546d22ffc,0x2e1b21385c26c926
	.quad	0x4d2c6dfc5ac42aed,0x53380d139d95b3df
	.quad	0x650a73548baf63de,0x766a0abb3c77b2a8
	.quad	0x81c2c92e47edaee6,0x92722c851482353b
	.quad	0xa2bfe8a14cf10364,0xa81a664bbc423001
	.quad	0xc24b8b70d0f89791,0xc76c51a30654be30
	.quad	0xd192e819d6ef5218,0xd69906245565a910
	.quad	0xf40e35855771202a,0x106aa07032bbd1b8
	.quad	0x19a4c116b8d2d0c8,0x1e376c085141ab53
	.quad	0x2748774cdf8eeb99,0x34b0bcb5e19b48a8
	.quad	0x391c0cb3c5c95a63,0x4ed8aa4ae3418acb
	.quad	0x5b9cca4f7763e373,0x682e6ff3d6b2b8a3
	.quad	0x748f82ee5defb2fc,0x78a5636f43172f60
	.quad	0x84c87814a1f0ab72,0x8cc702081a6439ec
	.quad	0x90befffa23631e28,0xa4506cebde82bde9
	.quad	0xbef9a3f7b2c67915,0xc67178f2e372532b
	.quad	0xca273eceea26619c,0xd186b8c721c0c207
	.quad	0xeada7dd6cde0eb1e,0xf57d4f7fee6ed178
	.quad	0x06f067aa72176fba,0x0a637dc5a2c898a6
	.quad	0x113f9804bef90dae,0x1b710b35131c471b
	.quad	0x28db77f523047d84,0x32caab7b40c72493
	.quad	0x3c9ebe0a15c9bebc,0x431d67c49c100d4c
	.quad	0x4cc5d4becb3e42b6,0x597f299cfc657e2a
	.quad	0x5fcb6fab3ad6faec,0x6c44198c4a475817

/* Mask for byte-swapping a couple of qwords in an XMM register using (v)pshufb. */
.align 32
.LPSHUFFLE_BYTE_FLIP_MASK:	.octa 0x08090a0b0c0d0e0f0001020304050607
				.octa 0x18191a1b1c1d1e1f1011121314151617

.align 4
.LPERM_VPALIGNR_8:		.byte 5, 6, 7, 0
ELF(.size _gcry_sha512_avx512_consts,.-_gcry_sha512_avx512_consts)

#endif
#endif
