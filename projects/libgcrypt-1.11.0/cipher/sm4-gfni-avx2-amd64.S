/* sm4-gfni-avx2-amd64.S  -  GFNI/AVX2 implementation of SM4 cipher
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
    defined(ENABLE_GFNI_SUPPORT) && defined(ENABLE_AVX2_SUPPORT)

#include "asm-common-amd64.h"

/**********************************************************************
  helper macros
 **********************************************************************/

/* Transpose four 32-bit words between 128-bit vectors. */
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

/**********************************************************************
  4-way && 8-way SM4 with GFNI and AVX2
 **********************************************************************/

/* vector registers */
#define RX0          %ymm0
#define RX1          %ymm1
#define RX0x         %xmm0
#define RX1x         %xmm1

#define RTMP0        %ymm2
#define RTMP1        %ymm3
#define RTMP2        %ymm4
#define RTMP3        %ymm5
#define RTMP4        %ymm6
#define RTMP0x       %xmm2
#define RTMP1x       %xmm3
#define RTMP2x       %xmm4
#define RTMP3x       %xmm5
#define RTMP4x       %xmm6

#define RNOT         %ymm7
#define RNOTx        %xmm7

#define RA0          %ymm8
#define RA1          %ymm9
#define RA2          %ymm10
#define RA3          %ymm11
#define RA0x         %xmm8
#define RA1x         %xmm9
#define RA2x         %xmm10
#define RA3x         %xmm11

#define RB0          %ymm12
#define RB1          %ymm13
#define RB2          %ymm14
#define RB3          %ymm15
#define RB0x         %xmm12
#define RB1x         %xmm13
#define RB2x         %xmm14
#define RB3x         %xmm15

SECTION_RODATA
.align 32

ELF(.type _sm4_gfni_avx2_consts,@object)
_sm4_gfni_avx2_consts:

/* Affine transform, SM4 field to AES field */
.Lpre_affine_s:
	.byte 0x52, 0xbc, 0x2d, 0x02, 0x9e, 0x25, 0xac, 0x34
	.byte 0x52, 0xbc, 0x2d, 0x02, 0x9e, 0x25, 0xac, 0x34
	.byte 0x52, 0xbc, 0x2d, 0x02, 0x9e, 0x25, 0xac, 0x34
	.byte 0x52, 0xbc, 0x2d, 0x02, 0x9e, 0x25, 0xac, 0x34

/* Affine transform, AES field to SM4 field */
.Lpost_affine_s:
	.byte 0x19, 0x8b, 0x6c, 0x1e, 0x51, 0x8e, 0x2d, 0xd7
	.byte 0x19, 0x8b, 0x6c, 0x1e, 0x51, 0x8e, 0x2d, 0xd7
	.byte 0x19, 0x8b, 0x6c, 0x1e, 0x51, 0x8e, 0x2d, 0xd7
	.byte 0x19, 0x8b, 0x6c, 0x1e, 0x51, 0x8e, 0x2d, 0xd7

/* Rotate left by 8 bits on 32-bit words with vpshufb */
.Lrol_8:
	.byte 0x03, 0x00, 0x01, 0x02, 0x07, 0x04, 0x05, 0x06
	.byte 0x0b, 0x08, 0x09, 0x0a, 0x0f, 0x0c, 0x0d, 0x0e
	.byte 0x03, 0x00, 0x01, 0x02, 0x07, 0x04, 0x05, 0x06
	.byte 0x0b, 0x08, 0x09, 0x0a, 0x0f, 0x0c, 0x0d, 0x0e

/* Rotate left by 16 bits on 32-bit words with vpshufb */
.Lrol_16:
	.byte 0x02, 0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05
	.byte 0x0a, 0x0b, 0x08, 0x09, 0x0e, 0x0f, 0x0c, 0x0d
	.byte 0x02, 0x03, 0x00, 0x01, 0x06, 0x07, 0x04, 0x05
	.byte 0x0a, 0x0b, 0x08, 0x09, 0x0e, 0x0f, 0x0c, 0x0d

/* Rotate left by 24 bits on 32-bit words with vpshufb */
.Lrol_24:
	.byte 0x01, 0x02, 0x03, 0x00, 0x05, 0x06, 0x07, 0x04
	.byte 0x09, 0x0a, 0x0b, 0x08, 0x0d, 0x0e, 0x0f, 0x0c
	.byte 0x01, 0x02, 0x03, 0x00, 0x05, 0x06, 0x07, 0x04
	.byte 0x09, 0x0a, 0x0b, 0x08, 0x0d, 0x0e, 0x0f, 0x0c

/* For CTR-mode IV byteswap */
.Lbswap128_mask:
	.byte 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

/* For input word byte-swap */
.Lbswap32_mask:
	.byte 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12

/* CTR byte addition constants */
.align 32
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

.text

.align 16
.globl _gcry_sm4_gfni_avx2_expand_key
ELF(.type   _gcry_sm4_gfni_avx2_expand_key,@function;)
_gcry_sm4_gfni_avx2_expand_key:
	/* input:
	 *	%rdi: 128-bit key
	 *	%rsi: rkey_enc
	 *	%rdx: rkey_dec
	 *	%rcx: fk array
	 *	%r8: ck array
	 */
	CFI_STARTPROC();

	vmovd 0*4(%rdi), RA0x;
	vmovd 1*4(%rdi), RA1x;
	vmovd 2*4(%rdi), RA2x;
	vmovd 3*4(%rdi), RA3x;

	vmovdqa .Lbswap32_mask rRIP, RTMP2x;
	vpshufb RTMP2x, RA0x, RA0x;
	vpshufb RTMP2x, RA1x, RA1x;
	vpshufb RTMP2x, RA2x, RA2x;
	vpshufb RTMP2x, RA3x, RA3x;

	vmovd 0*4(%rcx), RB0x;
	vmovd 1*4(%rcx), RB1x;
	vmovd 2*4(%rcx), RB2x;
	vmovd 3*4(%rcx), RB3x;
	vpxor RB0x, RA0x, RA0x;
	vpxor RB1x, RA1x, RA1x;
	vpxor RB2x, RA2x, RA2x;
	vpxor RB3x, RA3x, RA3x;

#define ROUND(round, s0, s1, s2, s3) \
	vpbroadcastd (4*(round))(%r8), RX0x; \
	vpxor s1, RX0x, RX0x; \
	vpxor s2, RX0x, RX0x; \
	vpxor s3, RX0x, RX0x; /* s1 ^ s2 ^ s3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vgf2p8affineqb $0x65, .Lpre_affine_s rRIP, RX0x, RX0x; \
	vgf2p8affineinvqb $0xd3, .Lpost_affine_s rRIP, RX0x, RX0x; \
	\
	/* linear part */ \
	vpxor RX0x, s0, s0; /* s0 ^ x */ \
	vpslld $13, RX0x, RTMP0x; \
	vpsrld $19, RX0x, RTMP1x; \
	vpslld $23, RX0x, RTMP2x; \
	vpsrld $9, RX0x, RTMP3x; \
	vpxor RTMP0x, RTMP1x, RTMP1x;  \
	vpxor RTMP2x, RTMP3x, RTMP3x;  \
	vpxor RTMP1x, s0, s0; /* s0 ^ x ^ rol(x,13) */ \
	vpxor RTMP3x, s0, s0; /* s0 ^ x ^ rol(x,13) ^ rol(x,23) */

	leaq (32*4)(%r8), %rax;
	leaq (32*4)(%rdx), %rdx;
.align 16
.Lroundloop_expand_key:
	leaq (-4*4)(%rdx), %rdx;
	ROUND(0, RA0x, RA1x, RA2x, RA3x);
	ROUND(1, RA1x, RA2x, RA3x, RA0x);
	ROUND(2, RA2x, RA3x, RA0x, RA1x);
	ROUND(3, RA3x, RA0x, RA1x, RA2x);
	leaq (4*4)(%r8), %r8;
	vmovd RA0x, (0*4)(%rsi);
	vmovd RA1x, (1*4)(%rsi);
	vmovd RA2x, (2*4)(%rsi);
	vmovd RA3x, (3*4)(%rsi);
	vmovd RA0x, (3*4)(%rdx);
	vmovd RA1x, (2*4)(%rdx);
	vmovd RA2x, (1*4)(%rdx);
	vmovd RA3x, (0*4)(%rdx);
	leaq (4*4)(%rsi), %rsi;
	cmpq %rax, %r8;
	jne .Lroundloop_expand_key;

#undef ROUND

	vzeroall;
	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_expand_key,.-_gcry_sm4_gfni_avx2_expand_key;)

.align 16
ELF(.type   sm4_gfni_avx2_crypt_blk1_4,@function;)
sm4_gfni_avx2_crypt_blk1_4:
	/* input:
	 *	%rdi: round key array, CTX
	 *	%rsi: dst (1..4 blocks)
	 *	%rdx: src (1..4 blocks)
	 *	%rcx: num blocks (1..4)
	 */
	CFI_STARTPROC();

	vmovdqu 0*16(%rdx), RA0x;
	vmovdqa RA0x, RA1x;
	vmovdqa RA0x, RA2x;
	vmovdqa RA0x, RA3x;
	cmpq $2, %rcx;
	jb .Lblk4_load_input_done;
	vmovdqu 1*16(%rdx), RA1x;
	je .Lblk4_load_input_done;
	vmovdqu 2*16(%rdx), RA2x;
	cmpq $3, %rcx;
	je .Lblk4_load_input_done;
	vmovdqu 3*16(%rdx), RA3x;

.Lblk4_load_input_done:

	vmovdqa .Lbswap32_mask rRIP, RTMP2x;
	vpshufb RTMP2x, RA0x, RA0x;
	vpshufb RTMP2x, RA1x, RA1x;
	vpshufb RTMP2x, RA2x, RA2x;
	vpshufb RTMP2x, RA3x, RA3x;

	vmovdqa .Lrol_8 rRIP, RTMP2x;
	vmovdqa .Lrol_16 rRIP, RTMP3x;
	vmovdqa .Lrol_24 rRIP, RB3x;
	transpose_4x4(RA0x, RA1x, RA2x, RA3x, RTMP0x, RTMP1x);

#define ROUND(round, s0, s1, s2, s3) \
	vpbroadcastd (4*(round))(%rdi), RX0x; \
	vpxor s1, RX0x, RX0x; \
	vpxor s2, RX0x, RX0x; \
	vpxor s3, RX0x, RX0x; /* s1 ^ s2 ^ s3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vgf2p8affineqb $0x65, .Lpre_affine_s rRIP, RX0x, RX0x; \
	vgf2p8affineinvqb $0xd3, .Lpost_affine_s rRIP, RX0x, RX0x; \
	\
	/* linear part */ \
	vpxor RX0x, s0, s0; /* s0 ^ x */ \
	vpshufb RTMP2x, RX0x, RTMP1x; \
	vpxor RTMP1x, RX0x, RTMP0x; /* x ^ rol(x,8) */ \
	vpshufb RTMP3x, RX0x, RTMP1x; \
	vpxor RTMP1x, RTMP0x, RTMP0x; /* x ^ rol(x,8) ^ rol(x,16) */ \
	vpshufb RB3x, RX0x, RTMP1x; \
	vpxor RTMP1x, s0, s0; /* s0 ^ x ^ rol(x,24) */ \
	vpslld $2, RTMP0x, RTMP1x; \
	vpsrld $30, RTMP0x, RTMP0x; \
	vpxor RTMP0x, s0, s0;  \
	vpxor RTMP1x, s0, s0; /* s0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */

	leaq (32*4)(%rdi), %rax;
.align 16
.Lroundloop_blk4:
	ROUND(0, RA0x, RA1x, RA2x, RA3x);
	ROUND(1, RA1x, RA2x, RA3x, RA0x);
	ROUND(2, RA2x, RA3x, RA0x, RA1x);
	ROUND(3, RA3x, RA0x, RA1x, RA2x);
	leaq (4*4)(%rdi), %rdi;
	cmpq %rax, %rdi;
	jne .Lroundloop_blk4;

#undef ROUND

	vmovdqa .Lbswap128_mask rRIP, RTMP2x;

	transpose_4x4(RA0x, RA1x, RA2x, RA3x, RTMP0x, RTMP1x);
	vpshufb RTMP2x, RA0x, RA0x;
	vpshufb RTMP2x, RA1x, RA1x;
	vpshufb RTMP2x, RA2x, RA2x;
	vpshufb RTMP2x, RA3x, RA3x;

	vmovdqu RA0x, 0*16(%rsi);
	cmpq $2, %rcx;
	jb .Lblk4_store_output_done;
	vmovdqu RA1x, 1*16(%rsi);
	je .Lblk4_store_output_done;
	vmovdqu RA2x, 2*16(%rsi);
	cmpq $3, %rcx;
	je .Lblk4_store_output_done;
	vmovdqu RA3x, 3*16(%rsi);

.Lblk4_store_output_done:
	vzeroall;
	xorl %eax, %eax;
	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size sm4_gfni_avx2_crypt_blk1_4,.-sm4_gfni_avx2_crypt_blk1_4;)

.align 16
ELF(.type __sm4_gfni_crypt_blk8,@function;)
__sm4_gfni_crypt_blk8:
	/* input:
	 *	%rdi: round key array, CTX
	 *	RA0, RA1, RA2, RA3, RB0, RB1, RB2, RB3: eight parallel
	 * 						ciphertext blocks
	 * output:
	 *	RA0, RA1, RA2, RA3, RB0, RB1, RB2, RB3: eight parallel plaintext
	 *						blocks
	 */
	CFI_STARTPROC();

	vmovdqa .Lbswap32_mask rRIP, RTMP2x;
	vpshufb RTMP2x, RA0x, RA0x;
	vpshufb RTMP2x, RA1x, RA1x;
	vpshufb RTMP2x, RA2x, RA2x;
	vpshufb RTMP2x, RA3x, RA3x;
	vpshufb RTMP2x, RB0x, RB0x;
	vpshufb RTMP2x, RB1x, RB1x;
	vpshufb RTMP2x, RB2x, RB2x;
	vpshufb RTMP2x, RB3x, RB3x;

	transpose_4x4(RA0x, RA1x, RA2x, RA3x, RTMP0x, RTMP1x);
	transpose_4x4(RB0x, RB1x, RB2x, RB3x, RTMP0x, RTMP1x);

#define ROUND(round, s0, s1, s2, s3, r0, r1, r2, r3) \
	vpbroadcastd (4*(round))(%rdi), RX0x; \
	vmovdqa .Lpre_affine_s rRIP, RTMP2x; \
	vmovdqa .Lpost_affine_s rRIP, RTMP3x; \
	vmovdqa RX0x, RX1x; \
	vpxor s1, RX0x, RX0x; \
	vpxor s2, RX0x, RX0x; \
	vpxor s3, RX0x, RX0x; /* s1 ^ s2 ^ s3 ^ rk */ \
	    vpxor r1, RX1x, RX1x; \
	    vpxor r2, RX1x, RX1x; \
	    vpxor r3, RX1x, RX1x; /* r1 ^ r2 ^ r3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vmovdqa .Lrol_8 rRIP, RTMP4x; \
	vgf2p8affineqb $0x65, RTMP2x, RX0x, RX0x; \
	vgf2p8affineinvqb $0xd3, RTMP3x, RX0x, RX0x; \
	    vgf2p8affineqb $0x65, RTMP2x, RX1x, RX1x; \
	    vgf2p8affineinvqb $0xd3, RTMP3x, RX1x, RX1x; \
	\
	/* linear part */ \
	vpxor RX0x, s0, s0; /* s0 ^ x */ \
	vpshufb RTMP4x, RX0x, RTMP1x; \
	vpxor RTMP1x, RX0x, RTMP0x; /* x ^ rol(x,8) */ \
	    vpxor RX1x, r0, r0; /* r0 ^ x */ \
	    vpshufb RTMP4x, RX1x, RTMP3x; \
	    vmovdqa .Lrol_16 rRIP, RTMP4x; \
	    vpxor RTMP3x, RX1x, RTMP2x; /* x ^ rol(x,8) */ \
	vpshufb RTMP4x, RX0x, RTMP1x; \
	vpxor RTMP1x, RTMP0x, RTMP0x; /* x ^ rol(x,8) ^ rol(x,16) */ \
	    vpshufb RTMP4x, RX1x, RTMP3x; \
	    vmovdqa .Lrol_24 rRIP, RTMP4x; \
	    vpxor RTMP3x, RTMP2x, RTMP2x; /* x ^ rol(x,8) ^ rol(x,16) */ \
	vpshufb RTMP4x, RX0x, RTMP1x; \
	vpxor RTMP1x, s0, s0; /* s0 ^ x ^ rol(x,24) */ \
	vpslld $2, RTMP0x, RTMP1x; \
	vpsrld $30, RTMP0x, RTMP0x; \
	vpxor RTMP0x, s0, s0;  \
	vpxor RTMP1x, s0, s0; /* s0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	    vpshufb RTMP4x, RX1x, RTMP3x; \
	    vpxor RTMP3x, r0, r0; /* r0 ^ x ^ rol(x,24) */ \
	    vpslld $2, RTMP2x, RTMP3x; \
	    vpsrld $30, RTMP2x, RTMP2x; \
	    vpxor RTMP2x, r0, r0;  \
	    vpxor RTMP3x, r0, r0; /* r0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */

	leaq (32*4)(%rdi), %rax;
.align 16
.Lroundloop_blk8:
	ROUND(0, RA0x, RA1x, RA2x, RA3x, RB0x, RB1x, RB2x, RB3x);
	ROUND(1, RA1x, RA2x, RA3x, RA0x, RB1x, RB2x, RB3x, RB0x);
	ROUND(2, RA2x, RA3x, RA0x, RA1x, RB2x, RB3x, RB0x, RB1x);
	ROUND(3, RA3x, RA0x, RA1x, RA2x, RB3x, RB0x, RB1x, RB2x);
	leaq (4*4)(%rdi), %rdi;
	cmpq %rax, %rdi;
	jne .Lroundloop_blk8;

#undef ROUND

	vmovdqa .Lbswap128_mask rRIP, RTMP2x;

	transpose_4x4(RA0x, RA1x, RA2x, RA3x, RTMP0x, RTMP1x);
	transpose_4x4(RB0x, RB1x, RB2x, RB3x, RTMP0x, RTMP1x);
	vpshufb RTMP2x, RA0x, RA0x;
	vpshufb RTMP2x, RA1x, RA1x;
	vpshufb RTMP2x, RA2x, RA2x;
	vpshufb RTMP2x, RA3x, RA3x;
	vpshufb RTMP2x, RB0x, RB0x;
	vpshufb RTMP2x, RB1x, RB1x;
	vpshufb RTMP2x, RB2x, RB2x;
	vpshufb RTMP2x, RB3x, RB3x;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __sm4_gfni_crypt_blk8,.-__sm4_gfni_crypt_blk8;)

.align 16
ELF(.type   _gcry_sm4_gfni_avx2_crypt_blk1_8,@function;)
_gcry_sm4_gfni_avx2_crypt_blk1_8:
	/* input:
	 *	%rdi: round key array, CTX
	 *	%rsi: dst (1..8 blocks)
	 *	%rdx: src (1..8 blocks)
	 *	%rcx: num blocks (1..8)
	 */
	CFI_STARTPROC();

	cmpq $5, %rcx;
	jb sm4_gfni_avx2_crypt_blk1_4;
	vmovdqu (0 * 16)(%rdx), RA0x;
	vmovdqu (1 * 16)(%rdx), RA1x;
	vmovdqu (2 * 16)(%rdx), RA2x;
	vmovdqu (3 * 16)(%rdx), RA3x;
	vmovdqu (4 * 16)(%rdx), RB0x;
	vmovdqa RB0x, RB1x;
	vmovdqa RB0x, RB2x;
	vmovdqa RB0x, RB3x;
	je .Lblk8_load_input_done;
	vmovdqu (5 * 16)(%rdx), RB1x;
	cmpq $7, %rcx;
	jb .Lblk8_load_input_done;
	vmovdqu (6 * 16)(%rdx), RB2x;
	je .Lblk8_load_input_done;
	vmovdqu (7 * 16)(%rdx), RB3x;

.Lblk8_load_input_done:
	call __sm4_gfni_crypt_blk8;

	cmpq $6, %rcx;
	vmovdqu RA0x, (0 * 16)(%rsi);
	vmovdqu RA1x, (1 * 16)(%rsi);
	vmovdqu RA2x, (2 * 16)(%rsi);
	vmovdqu RA3x, (3 * 16)(%rsi);
	vmovdqu RB0x, (4 * 16)(%rsi);
	jb .Lblk8_store_output_done;
	vmovdqu RB1x, (5 * 16)(%rsi);
	je .Lblk8_store_output_done;
	vmovdqu RB2x, (6 * 16)(%rsi);
	cmpq $7, %rcx;
	je .Lblk8_store_output_done;
	vmovdqu RB3x, (7 * 16)(%rsi);

.Lblk8_store_output_done:
	vzeroall;
	xorl %eax, %eax;
	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_crypt_blk1_8,.-_gcry_sm4_gfni_avx2_crypt_blk1_8;)

/**********************************************************************
  16-way SM4 with GFNI and AVX2
 **********************************************************************/

.align 16
ELF(.type   __sm4_gfni_crypt_blk16,@function;)
__sm4_gfni_crypt_blk16:
	/* input:
	 *	%rdi: ctx, CTX
	 *	RA0, RA1, RA2, RA3, RB0, RB1, RB2, RB3: sixteen parallel
	 *						plaintext blocks
	 * output:
	 *	RA0, RA1, RA2, RA3, RB0, RB1, RB2, RB3: sixteen parallel
	 * 						ciphertext blocks
	 */
	CFI_STARTPROC();

	vbroadcasti128 .Lbswap32_mask rRIP, RTMP2;
	vpshufb RTMP2, RA0, RA0;
	vpshufb RTMP2, RA1, RA1;
	vpshufb RTMP2, RA2, RA2;
	vpshufb RTMP2, RA3, RA3;
	vpshufb RTMP2, RB0, RB0;
	vpshufb RTMP2, RB1, RB1;
	vpshufb RTMP2, RB2, RB2;
	vpshufb RTMP2, RB3, RB3;

	transpose_4x4(RA0, RA1, RA2, RA3, RTMP0, RTMP1);
	transpose_4x4(RB0, RB1, RB2, RB3, RTMP0, RTMP1);

#define ROUND(round, s0, s1, s2, s3, r0, r1, r2, r3) \
	vpbroadcastd (4*(round))(%rdi), RX0; \
	vbroadcasti128 .Lpre_affine_s rRIP, RTMP2; \
	vbroadcasti128 .Lpost_affine_s rRIP, RTMP3; \
	vmovdqa RX0, RX1; \
	vpxor s1, RX0, RX0; \
	vpxor s2, RX0, RX0; \
	vpxor s3, RX0, RX0; /* s1 ^ s2 ^ s3 ^ rk */ \
	    vpxor r1, RX1, RX1; \
	    vpxor r2, RX1, RX1; \
	    vpxor r3, RX1, RX1; /* r1 ^ r2 ^ r3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vbroadcasti128 .Lrol_8 rRIP, RTMP4; \
	vgf2p8affineqb $0x65, RTMP2, RX0, RX0; \
	vgf2p8affineinvqb $0xd3, RTMP3, RX0, RX0; \
	    vgf2p8affineqb $0x65, RTMP2, RX1, RX1; \
	    vgf2p8affineinvqb $0xd3, RTMP3, RX1, RX1; \
	\
	/* linear part */ \
	vpxor RX0, s0, s0; /* s0 ^ x */ \
	vpshufb RTMP4, RX0, RTMP1; \
	vpxor RTMP1, RX0, RTMP0; /* x ^ rol(x,8) */ \
	    vpxor RX1, r0, r0; /* r0 ^ x */ \
	    vpshufb RTMP4, RX1, RTMP3; \
	    vbroadcasti128 .Lrol_16 rRIP, RTMP4; \
	    vpxor RTMP3, RX1, RTMP2; /* x ^ rol(x,8) */ \
	vpshufb RTMP4, RX0, RTMP1; \
	vpxor RTMP1, RTMP0, RTMP0; /* x ^ rol(x,8) ^ rol(x,16) */ \
	    vpshufb RTMP4, RX1, RTMP3; \
	    vbroadcasti128 .Lrol_24 rRIP, RTMP4; \
	    vpxor RTMP3, RTMP2, RTMP2; /* x ^ rol(x,8) ^ rol(x,16) */ \
	vpshufb RTMP4, RX0, RTMP1; \
	vpxor RTMP1, s0, s0; /* s0 ^ x ^ rol(x,24) */ \
	vpslld $2, RTMP0, RTMP1; \
	vpsrld $30, RTMP0, RTMP0; \
	vpxor RTMP0, s0, s0;  \
	vpxor RTMP1, s0, s0; /* s0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	    vpshufb RTMP4, RX1, RTMP3; \
	    vpxor RTMP3, r0, r0; /* r0 ^ x ^ rol(x,24) */ \
	    vpslld $2, RTMP2, RTMP3; \
	    vpsrld $30, RTMP2, RTMP2; \
	    vpxor RTMP2, r0, r0;  \
	    vpxor RTMP3, r0, r0; /* r0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */

	leaq (32*4)(%rdi), %rax;
.align 16
.Lroundloop_blk16:
	ROUND(0, RA0, RA1, RA2, RA3, RB0, RB1, RB2, RB3);
	ROUND(1, RA1, RA2, RA3, RA0, RB1, RB2, RB3, RB0);
	ROUND(2, RA2, RA3, RA0, RA1, RB2, RB3, RB0, RB1);
	ROUND(3, RA3, RA0, RA1, RA2, RB3, RB0, RB1, RB2);
	leaq (4*4)(%rdi), %rdi;
	cmpq %rax, %rdi;
	jne .Lroundloop_blk16;

#undef ROUND

	vbroadcasti128 .Lbswap128_mask rRIP, RTMP2;

	transpose_4x4(RA0, RA1, RA2, RA3, RTMP0, RTMP1);
	transpose_4x4(RB0, RB1, RB2, RB3, RTMP0, RTMP1);
	vpshufb RTMP2, RA0, RA0;
	vpshufb RTMP2, RA1, RA1;
	vpshufb RTMP2, RA2, RA2;
	vpshufb RTMP2, RA3, RA3;
	vpshufb RTMP2, RB0, RB0;
	vpshufb RTMP2, RB1, RB1;
	vpshufb RTMP2, RB2, RB2;
	vpshufb RTMP2, RB3, RB3;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __sm4_gfni_crypt_blk16,.-__sm4_gfni_crypt_blk16;)

.align 16
.globl _gcry_sm4_gfni_avx2_crypt_blk1_16
ELF(.type   _gcry_sm4_gfni_avx2_crypt_blk1_16,@function;)
_gcry_sm4_gfni_avx2_crypt_blk1_16:
	/* input:
	 *	%rdi: round key array, CTX
	 *	%rsi: dst (1..16 blocks)
	 *	%rdx: src (1..16 blocks)
	 *	%rcx: num blocks (1..16)
	 */
	CFI_STARTPROC();

#define LOAD_INPUT(offset, yreg) \
	cmpq $(1 + 2 * (offset)), %rcx; \
	jb .Lblk16_load_input_done; \
	ja 1f; \
	  vmovdqu (offset) * 32(%rdx), yreg##x; \
	  jmp .Lblk16_load_input_done; \
	1: \
	  vmovdqu (offset) * 32(%rdx), yreg;

	cmpq $8, %rcx;
	jbe _gcry_sm4_gfni_avx2_crypt_blk1_8;
	vmovdqu (0 * 32)(%rdx), RA0;
	vmovdqu (1 * 32)(%rdx), RA1;
	vmovdqu (2 * 32)(%rdx), RA2;
	vmovdqu (3 * 32)(%rdx), RA3;
	LOAD_INPUT(4, RB0);
	LOAD_INPUT(5, RB1);
	LOAD_INPUT(6, RB2);
	LOAD_INPUT(7, RB3);
#undef LOAD_INPUT

.Lblk16_load_input_done:
	call __sm4_gfni_crypt_blk16;

#define STORE_OUTPUT(yreg, offset) \
	cmpq $(1 + 2 * (offset)), %rcx; \
	jb .Lblk16_store_output_done; \
	ja 1f; \
	  vmovdqu yreg##x, (offset) * 32(%rsi); \
	  jmp .Lblk16_store_output_done; \
	1: \
	  vmovdqu yreg, (offset) * 32(%rsi);

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RA1, (1 * 32)(%rsi);
	vmovdqu RA2, (2 * 32)(%rsi);
	vmovdqu RA3, (3 * 32)(%rsi);
	STORE_OUTPUT(RB0, 4);
	STORE_OUTPUT(RB1, 5);
	STORE_OUTPUT(RB2, 6);
	STORE_OUTPUT(RB3, 7);
#undef STORE_OUTPUT

.Lblk16_store_output_done:
	vzeroall;
	xorl %eax, %eax;
	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_crypt_blk1_16,.-_gcry_sm4_gfni_avx2_crypt_blk1_16;)

#define inc_le128(x, minus_one, tmp) \
	vpcmpeqq minus_one, x, tmp; \
	vpsubq minus_one, x, x; \
	vpslldq $8, tmp, tmp; \
	vpsubq tmp, x, x;

.align 16
.globl _gcry_sm4_gfni_avx2_ctr_enc
ELF(.type   _gcry_sm4_gfni_avx2_ctr_enc,@function;)
_gcry_sm4_gfni_avx2_ctr_enc:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv (big endian, 128bit)
	 */
	CFI_STARTPROC();

	cmpb $(0x100 - 16), 15(%rcx);
	jbe .Lctr_byteadd;

	movq 8(%rcx), %rax;
	bswapq %rax;

	vbroadcasti128 .Lbswap128_mask rRIP, RTMP3;
	vpcmpeqd RNOT, RNOT, RNOT;
	vpsrldq $8, RNOT, RNOT;   /* ab: -1:0 ; cd: -1:0 */
	vpaddq RNOT, RNOT, RTMP2; /* ab: -2:0 ; cd: -2:0 */

	/* load IV and byteswap */
	vmovdqu (%rcx), RTMP4x;
	vpshufb RTMP3x, RTMP4x, RTMP4x;
	vmovdqa RTMP4x, RTMP0x;
	inc_le128(RTMP4x, RNOTx, RTMP1x);
	vinserti128 $1, RTMP4x, RTMP0, RTMP0;
	vpshufb RTMP3, RTMP0, RA0; /* +1 ; +0 */

	/* check need for handling 64-bit overflow and carry */
	cmpq $(0xffffffffffffffff - 16), %rax;
	ja .Lhandle_ctr_carry;

	/* construct IVs */
	vpsubq RTMP2, RTMP0, RTMP0; /* +3 ; +2 */
	vpshufb RTMP3, RTMP0, RA1;
	vpsubq RTMP2, RTMP0, RTMP0; /* +5 ; +4 */
	vpshufb RTMP3, RTMP0, RA2;
	vpsubq RTMP2, RTMP0, RTMP0; /* +7 ; +6 */
	vpshufb RTMP3, RTMP0, RA3;
	vpsubq RTMP2, RTMP0, RTMP0; /* +9 ; +8 */
	vpshufb RTMP3, RTMP0, RB0;
	vpsubq RTMP2, RTMP0, RTMP0; /* +11 ; +10 */
	vpshufb RTMP3, RTMP0, RB1;
	vpsubq RTMP2, RTMP0, RTMP0; /* +13 ; +12 */
	vpshufb RTMP3, RTMP0, RB2;
	vpsubq RTMP2, RTMP0, RTMP0; /* +15 ; +14 */
	vpshufb RTMP3, RTMP0, RB3;
	vpsubq RTMP2, RTMP0, RTMP0; /* +16 */
	vpshufb RTMP3x, RTMP0x, RTMP0x;

	jmp .Lctr_carry_done;

.Lhandle_ctr_carry:
	/* construct IVs */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RA1; /* +3 ; +2 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RA2; /* +5 ; +4 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RA3; /* +7 ; +6 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RB0; /* +9 ; +8 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RB1; /* +11 ; +10 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RB2; /* +13 ; +12 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RB3; /* +15 ; +14 */
	inc_le128(RTMP0, RNOT, RTMP1);
	vextracti128 $1, RTMP0, RTMP0x;
	vpshufb RTMP3x, RTMP0x, RTMP0x; /* +16 */

.Lctr_carry_done:
	/* store new IV */
	vmovdqu RTMP0x, (%rcx);

.align 8
.Lload_ctr_done:
	call __sm4_gfni_crypt_blk16;

	vpxor (0 * 32)(%rdx), RA0, RA0;
	vpxor (1 * 32)(%rdx), RA1, RA1;
	vpxor (2 * 32)(%rdx), RA2, RA2;
	vpxor (3 * 32)(%rdx), RA3, RA3;
	vpxor (4 * 32)(%rdx), RB0, RB0;
	vpxor (5 * 32)(%rdx), RB1, RB1;
	vpxor (6 * 32)(%rdx), RB2, RB2;
	vpxor (7 * 32)(%rdx), RB3, RB3;

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RA1, (1 * 32)(%rsi);
	vmovdqu RA2, (2 * 32)(%rsi);
	vmovdqu RA3, (3 * 32)(%rsi);
	vmovdqu RB0, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RB2, (6 * 32)(%rsi);
	vmovdqu RB3, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;

.align 8
.Lctr_byteadd_full_ctr_carry:
	movq 8(%rcx), %r11;
	movq (%rcx), %r10;
	bswapq %r11;
	bswapq %r10;
	addq $16, %r11;
	adcq $0, %r10;
	bswapq %r11;
	bswapq %r10;
	movq %r11, 8(%rcx);
	movq %r10, (%rcx);
	jmp .Lctr_byteadd_ymm;
.align 8
.Lctr_byteadd:
	vbroadcasti128 (%rcx), RB3;
	je .Lctr_byteadd_full_ctr_carry;
	addb $16, 15(%rcx);
.Lctr_byteadd_ymm:
	vpaddb .Lbige_addb_0_1 rRIP, RB3, RA0;
	vpaddb .Lbige_addb_2_3 rRIP, RB3, RA1;
	vpaddb .Lbige_addb_4_5 rRIP, RB3, RA2;
	vpaddb .Lbige_addb_6_7 rRIP, RB3, RA3;
	vpaddb .Lbige_addb_8_9 rRIP, RB3, RB0;
	vpaddb .Lbige_addb_10_11 rRIP, RB3, RB1;
	vpaddb .Lbige_addb_12_13 rRIP, RB3, RB2;
	vpaddb .Lbige_addb_14_15 rRIP, RB3, RB3;

	jmp .Lload_ctr_done;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_ctr_enc,.-_gcry_sm4_gfni_avx2_ctr_enc;)

.align 16
.globl _gcry_sm4_gfni_avx2_cbc_dec
ELF(.type   _gcry_sm4_gfni_avx2_cbc_dec,@function;)
_gcry_sm4_gfni_avx2_cbc_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();

	vmovdqu (0 * 32)(%rdx), RA0;
	vmovdqu (1 * 32)(%rdx), RA1;
	vmovdqu (2 * 32)(%rdx), RA2;
	vmovdqu (3 * 32)(%rdx), RA3;
	vmovdqu (4 * 32)(%rdx), RB0;
	vmovdqu (5 * 32)(%rdx), RB1;
	vmovdqu (6 * 32)(%rdx), RB2;
	vmovdqu (7 * 32)(%rdx), RB3;

	call __sm4_gfni_crypt_blk16;

	vmovdqu (%rcx), RNOTx;
	vinserti128 $1, (%rdx), RNOT, RNOT;
	vpxor RNOT, RA0, RA0;
	vpxor (0 * 32 + 16)(%rdx), RA1, RA1;
	vpxor (1 * 32 + 16)(%rdx), RA2, RA2;
	vpxor (2 * 32 + 16)(%rdx), RA3, RA3;
	vpxor (3 * 32 + 16)(%rdx), RB0, RB0;
	vpxor (4 * 32 + 16)(%rdx), RB1, RB1;
	vpxor (5 * 32 + 16)(%rdx), RB2, RB2;
	vpxor (6 * 32 + 16)(%rdx), RB3, RB3;
	vmovdqu (7 * 32 + 16)(%rdx), RNOTx;
	vmovdqu RNOTx, (%rcx); /* store new IV */

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RA1, (1 * 32)(%rsi);
	vmovdqu RA2, (2 * 32)(%rsi);
	vmovdqu RA3, (3 * 32)(%rsi);
	vmovdqu RB0, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RB2, (6 * 32)(%rsi);
	vmovdqu RB3, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_cbc_dec,.-_gcry_sm4_gfni_avx2_cbc_dec;)

.align 16
.globl _gcry_sm4_gfni_avx2_cfb_dec
ELF(.type   _gcry_sm4_gfni_avx2_cfb_dec,@function;)
_gcry_sm4_gfni_avx2_cfb_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();

	/* Load input */
	vmovdqu (%rcx), RNOTx;
	vinserti128 $1, (%rdx), RNOT, RA0;
	vmovdqu (0 * 32 + 16)(%rdx), RA1;
	vmovdqu (1 * 32 + 16)(%rdx), RA2;
	vmovdqu (2 * 32 + 16)(%rdx), RA3;
	vmovdqu (3 * 32 + 16)(%rdx), RB0;
	vmovdqu (4 * 32 + 16)(%rdx), RB1;
	vmovdqu (5 * 32 + 16)(%rdx), RB2;
	vmovdqu (6 * 32 + 16)(%rdx), RB3;

	/* Update IV */
	vmovdqu (7 * 32 + 16)(%rdx), RNOTx;
	vmovdqu RNOTx, (%rcx);

	call __sm4_gfni_crypt_blk16;

	vpxor (0 * 32)(%rdx), RA0, RA0;
	vpxor (1 * 32)(%rdx), RA1, RA1;
	vpxor (2 * 32)(%rdx), RA2, RA2;
	vpxor (3 * 32)(%rdx), RA3, RA3;
	vpxor (4 * 32)(%rdx), RB0, RB0;
	vpxor (5 * 32)(%rdx), RB1, RB1;
	vpxor (6 * 32)(%rdx), RB2, RB2;
	vpxor (7 * 32)(%rdx), RB3, RB3;

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RA1, (1 * 32)(%rsi);
	vmovdqu RA2, (2 * 32)(%rsi);
	vmovdqu RA3, (3 * 32)(%rsi);
	vmovdqu RB0, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RB2, (6 * 32)(%rsi);
	vmovdqu RB3, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_cfb_dec,.-_gcry_sm4_gfni_avx2_cfb_dec;)

.align 16
.globl _gcry_sm4_gfni_avx2_ocb_enc
ELF(.type _gcry_sm4_gfni_avx2_ocb_enc,@function;)

_gcry_sm4_gfni_avx2_ocb_enc:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();

	subq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(4 * 8);

	movq %r10, (0 * 8)(%rsp);
	movq %r11, (1 * 8)(%rsp);
	movq %r12, (2 * 8)(%rsp);
	movq %r13, (3 * 8)(%rsp);
	CFI_REL_OFFSET(%r10, 0 * 8);
	CFI_REL_OFFSET(%r11, 1 * 8);
	CFI_REL_OFFSET(%r12, 2 * 8);
	CFI_REL_OFFSET(%r13, 3 * 8);

	vmovdqu (%rcx), RTMP0x;
	vmovdqu (%r8), RTMP1x;

	/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
	/* Checksum_i = Checksum_{i-1} xor P_i  */
	/* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */

#define OCB_INPUT(n, l0reg, l1reg, yreg) \
	  vmovdqu (n * 32)(%rdx), yreg; \
	  vpxor (l0reg), RTMP0x, RNOTx; \
	  vpxor (l1reg), RNOTx, RTMP0x; \
	  vinserti128 $1, RTMP0x, RNOT, RNOT; \
	  vpxor yreg, RTMP1, RTMP1; \
	  vpxor yreg, RNOT, yreg; \
	  vmovdqu RNOT, (n * 32)(%rsi);

	movq (0 * 8)(%r9), %r10;
	movq (1 * 8)(%r9), %r11;
	movq (2 * 8)(%r9), %r12;
	movq (3 * 8)(%r9), %r13;
	OCB_INPUT(0, %r10, %r11, RA0);
	OCB_INPUT(1, %r12, %r13, RA1);
	movq (4 * 8)(%r9), %r10;
	movq (5 * 8)(%r9), %r11;
	movq (6 * 8)(%r9), %r12;
	movq (7 * 8)(%r9), %r13;
	OCB_INPUT(2, %r10, %r11, RA2);
	OCB_INPUT(3, %r12, %r13, RA3);
	movq (8 * 8)(%r9), %r10;
	movq (9 * 8)(%r9), %r11;
	movq (10 * 8)(%r9), %r12;
	movq (11 * 8)(%r9), %r13;
	OCB_INPUT(4, %r10, %r11, RB0);
	OCB_INPUT(5, %r12, %r13, RB1);
	movq (12 * 8)(%r9), %r10;
	movq (13 * 8)(%r9), %r11;
	movq (14 * 8)(%r9), %r12;
	movq (15 * 8)(%r9), %r13;
	OCB_INPUT(6, %r10, %r11, RB2);
	OCB_INPUT(7, %r12, %r13, RB3);
#undef OCB_INPUT

	vextracti128 $1, RTMP1, RNOTx;
	vmovdqu RTMP0x, (%rcx);
	vpxor RNOTx, RTMP1x, RTMP1x;
	vmovdqu RTMP1x, (%r8);

	movq (0 * 8)(%rsp), %r10;
	movq (1 * 8)(%rsp), %r11;
	movq (2 * 8)(%rsp), %r12;
	movq (3 * 8)(%rsp), %r13;
	CFI_RESTORE(%r10);
	CFI_RESTORE(%r11);
	CFI_RESTORE(%r12);
	CFI_RESTORE(%r13);

	call __sm4_gfni_crypt_blk16;

	addq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(-4 * 8);

	vpxor (0 * 32)(%rsi), RA0, RA0;
	vpxor (1 * 32)(%rsi), RA1, RA1;
	vpxor (2 * 32)(%rsi), RA2, RA2;
	vpxor (3 * 32)(%rsi), RA3, RA3;
	vpxor (4 * 32)(%rsi), RB0, RB0;
	vpxor (5 * 32)(%rsi), RB1, RB1;
	vpxor (6 * 32)(%rsi), RB2, RB2;
	vpxor (7 * 32)(%rsi), RB3, RB3;

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RA1, (1 * 32)(%rsi);
	vmovdqu RA2, (2 * 32)(%rsi);
	vmovdqu RA3, (3 * 32)(%rsi);
	vmovdqu RB0, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RB2, (6 * 32)(%rsi);
	vmovdqu RB3, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_ocb_enc,.-_gcry_sm4_gfni_avx2_ocb_enc;)

.align 16
.globl _gcry_sm4_gfni_avx2_ocb_dec
ELF(.type _gcry_sm4_gfni_avx2_ocb_dec,@function;)

_gcry_sm4_gfni_avx2_ocb_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();

	subq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(4 * 8);

	movq %r10, (0 * 8)(%rsp);
	movq %r11, (1 * 8)(%rsp);
	movq %r12, (2 * 8)(%rsp);
	movq %r13, (3 * 8)(%rsp);
	CFI_REL_OFFSET(%r10, 0 * 8);
	CFI_REL_OFFSET(%r11, 1 * 8);
	CFI_REL_OFFSET(%r12, 2 * 8);
	CFI_REL_OFFSET(%r13, 3 * 8);

	vmovdqu (%rcx), RTMP0x;

	/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
	/* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */

#define OCB_INPUT(n, l0reg, l1reg, yreg) \
	  vmovdqu (n * 32)(%rdx), yreg; \
	  vpxor (l0reg), RTMP0x, RNOTx; \
	  vpxor (l1reg), RNOTx, RTMP0x; \
	  vinserti128 $1, RTMP0x, RNOT, RNOT; \
	  vpxor yreg, RNOT, yreg; \
	  vmovdqu RNOT, (n * 32)(%rsi);

	movq (0 * 8)(%r9), %r10;
	movq (1 * 8)(%r9), %r11;
	movq (2 * 8)(%r9), %r12;
	movq (3 * 8)(%r9), %r13;
	OCB_INPUT(0, %r10, %r11, RA0);
	OCB_INPUT(1, %r12, %r13, RA1);
	movq (4 * 8)(%r9), %r10;
	movq (5 * 8)(%r9), %r11;
	movq (6 * 8)(%r9), %r12;
	movq (7 * 8)(%r9), %r13;
	OCB_INPUT(2, %r10, %r11, RA2);
	OCB_INPUT(3, %r12, %r13, RA3);
	movq (8 * 8)(%r9), %r10;
	movq (9 * 8)(%r9), %r11;
	movq (10 * 8)(%r9), %r12;
	movq (11 * 8)(%r9), %r13;
	OCB_INPUT(4, %r10, %r11, RB0);
	OCB_INPUT(5, %r12, %r13, RB1);
	movq (12 * 8)(%r9), %r10;
	movq (13 * 8)(%r9), %r11;
	movq (14 * 8)(%r9), %r12;
	movq (15 * 8)(%r9), %r13;
	OCB_INPUT(6, %r10, %r11, RB2);
	OCB_INPUT(7, %r12, %r13, RB3);
#undef OCB_INPUT

	vmovdqu RTMP0x, (%rcx);

	movq (0 * 8)(%rsp), %r10;
	movq (1 * 8)(%rsp), %r11;
	movq (2 * 8)(%rsp), %r12;
	movq (3 * 8)(%rsp), %r13;
	CFI_RESTORE(%r10);
	CFI_RESTORE(%r11);
	CFI_RESTORE(%r12);
	CFI_RESTORE(%r13);

	call __sm4_gfni_crypt_blk16;

	addq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(-4 * 8);

	vmovdqu (%r8), RTMP1x;

	vpxor (0 * 32)(%rsi), RA0, RA0;
	vpxor (1 * 32)(%rsi), RA1, RA1;
	vpxor (2 * 32)(%rsi), RA2, RA2;
	vpxor (3 * 32)(%rsi), RA3, RA3;
	vpxor (4 * 32)(%rsi), RB0, RB0;
	vpxor (5 * 32)(%rsi), RB1, RB1;
	vpxor (6 * 32)(%rsi), RB2, RB2;
	vpxor (7 * 32)(%rsi), RB3, RB3;

	/* Checksum_i = Checksum_{i-1} xor P_i  */

	vmovdqu RA0, (0 * 32)(%rsi);
	vpxor RA0, RTMP1, RTMP1;
	vmovdqu RA1, (1 * 32)(%rsi);
	vpxor RA1, RTMP1, RTMP1;
	vmovdqu RA2, (2 * 32)(%rsi);
	vpxor RA2, RTMP1, RTMP1;
	vmovdqu RA3, (3 * 32)(%rsi);
	vpxor RA3, RTMP1, RTMP1;
	vmovdqu RB0, (4 * 32)(%rsi);
	vpxor RB0, RTMP1, RTMP1;
	vmovdqu RB1, (5 * 32)(%rsi);
	vpxor RB1, RTMP1, RTMP1;
	vmovdqu RB2, (6 * 32)(%rsi);
	vpxor RB2, RTMP1, RTMP1;
	vmovdqu RB3, (7 * 32)(%rsi);
	vpxor RB3, RTMP1, RTMP1;

	vextracti128 $1, RTMP1, RNOTx;
	vpxor RNOTx, RTMP1x, RTMP1x;
	vmovdqu RTMP1x, (%r8);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_ocb_dec,.-_gcry_sm4_gfni_avx2_ocb_dec;)

.align 16
.globl _gcry_sm4_gfni_avx2_ocb_auth
ELF(.type _gcry_sm4_gfni_avx2_ocb_auth,@function;)

_gcry_sm4_gfni_avx2_ocb_auth:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: abuf (16 blocks)
	 *	%rdx: offset
	 *	%rcx: checksum
	 *	%r8 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();

	subq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(4 * 8);

	movq %r10, (0 * 8)(%rsp);
	movq %r11, (1 * 8)(%rsp);
	movq %r12, (2 * 8)(%rsp);
	movq %r13, (3 * 8)(%rsp);
	CFI_REL_OFFSET(%r10, 0 * 8);
	CFI_REL_OFFSET(%r11, 1 * 8);
	CFI_REL_OFFSET(%r12, 2 * 8);
	CFI_REL_OFFSET(%r13, 3 * 8);

	vmovdqu (%rdx), RTMP0x;

	/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
	/* Sum_i = Sum_{i-1} xor ENCIPHER(K, A_i xor Offset_i)  */

#define OCB_INPUT(n, l0reg, l1reg, yreg) \
	  vmovdqu (n * 32)(%rsi), yreg; \
	  vpxor (l0reg), RTMP0x, RNOTx; \
	  vpxor (l1reg), RNOTx, RTMP0x; \
	  vinserti128 $1, RTMP0x, RNOT, RNOT; \
	  vpxor yreg, RNOT, yreg;

	movq (0 * 8)(%r8), %r10;
	movq (1 * 8)(%r8), %r11;
	movq (2 * 8)(%r8), %r12;
	movq (3 * 8)(%r8), %r13;
	OCB_INPUT(0, %r10, %r11, RA0);
	OCB_INPUT(1, %r12, %r13, RA1);
	movq (4 * 8)(%r8), %r10;
	movq (5 * 8)(%r8), %r11;
	movq (6 * 8)(%r8), %r12;
	movq (7 * 8)(%r8), %r13;
	OCB_INPUT(2, %r10, %r11, RA2);
	OCB_INPUT(3, %r12, %r13, RA3);
	movq (8 * 8)(%r8), %r10;
	movq (9 * 8)(%r8), %r11;
	movq (10 * 8)(%r8), %r12;
	movq (11 * 8)(%r8), %r13;
	OCB_INPUT(4, %r10, %r11, RB0);
	OCB_INPUT(5, %r12, %r13, RB1);
	movq (12 * 8)(%r8), %r10;
	movq (13 * 8)(%r8), %r11;
	movq (14 * 8)(%r8), %r12;
	movq (15 * 8)(%r8), %r13;
	OCB_INPUT(6, %r10, %r11, RB2);
	OCB_INPUT(7, %r12, %r13, RB3);
#undef OCB_INPUT

	vmovdqu RTMP0x, (%rdx);

	movq (0 * 8)(%rsp), %r10;
	movq (1 * 8)(%rsp), %r11;
	movq (2 * 8)(%rsp), %r12;
	movq (3 * 8)(%rsp), %r13;
	CFI_RESTORE(%r10);
	CFI_RESTORE(%r11);
	CFI_RESTORE(%r12);
	CFI_RESTORE(%r13);

	call __sm4_gfni_crypt_blk16;

	addq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(-4 * 8);

	vpxor RA0, RB0, RA0;
	vpxor RA1, RB1, RA1;
	vpxor RA2, RB2, RA2;
	vpxor RA3, RB3, RA3;

	vpxor RA1, RA0, RA0;
	vpxor RA3, RA2, RA2;

	vpxor RA2, RA0, RTMP1;

	vextracti128 $1, RTMP1, RNOTx;
	vpxor (%rcx), RTMP1x, RTMP1x;
	vpxor RNOTx, RTMP1x, RTMP1x;
	vmovdqu RTMP1x, (%rcx);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx2_ocb_auth,.-_gcry_sm4_gfni_avx2_ocb_auth;)

#endif /*defined(ENABLE_GFNI_SUPPORT) && defined(ENABLE_AVX2_SUPPORT)*/
#endif /*__x86_64*/
