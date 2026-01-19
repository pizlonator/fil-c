/* sm4-gfni-avx512-amd64.S  -  GFNI/AVX512 implementation of SM4 cipher
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
  4-way && 8-way SM4 with GFNI and AVX512 (128-bit vectors)
 **********************************************************************/

/* vector registers */
#define RX0          %ymm0
#define RX1          %ymm1
#define RX0x         %xmm0
#define RX1x         %xmm1
#define RX0z         %zmm0
#define RX1z         %zmm1

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
#define RTMP0z       %zmm2
#define RTMP1z       %zmm3
#define RTMP2z       %zmm4
#define RTMP3z       %zmm5
#define RTMP4z       %zmm6

#define RNOT         %ymm7
#define RNOTx        %xmm7
#define RNOTz        %zmm7

#define RA0          %ymm8
#define RA1          %ymm9
#define RA2          %ymm10
#define RA3          %ymm11
#define RA0x         %xmm8
#define RA1x         %xmm9
#define RA2x         %xmm10
#define RA3x         %xmm11
#define RA0z         %zmm8
#define RA1z         %zmm9
#define RA2z         %zmm10
#define RA3z         %zmm11

#define RB0          %ymm12
#define RB1          %ymm13
#define RB2          %ymm14
#define RB3          %ymm15
#define RB0x         %xmm12
#define RB1x         %xmm13
#define RB2x         %xmm14
#define RB3x         %xmm15
#define RB0z         %zmm12
#define RB1z         %zmm13
#define RB2z         %zmm14
#define RB3z         %zmm15

SECTION_RODATA
.align 32

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

/* For CTR-mode IV byteswap */
.Lbswap128_mask:
	.byte 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

/* For input word byte-swap */
.Lbswap32_mask:
	.byte 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12

.Lcounter2222_lo:
	.quad 2, 0
.Lcounter4444_lo:
	.quad 4, 0
.Lcounter8888_lo:
	.quad 8, 0
.Lcounter16161616_lo:
	.quad 16, 0
.Lcounter1111_hi:
	.quad 0, 1

.align 64
.Lcounter0123_lo:
	.quad 0, 0
	.quad 1, 0
	.quad 2, 0
	.quad 3, 0

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
.globl _gcry_sm4_gfni_avx512_expand_key
ELF(.type   _gcry_sm4_gfni_avx512_expand_key,@function;)
_gcry_sm4_gfni_avx512_expand_key:
	/* input:
	 *	%rdi: 128-bit key
	 *	%rsi: rkey_enc
	 *	%rdx: rkey_dec
	 *	%rcx: fk array
	 *	%r8: ck array
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

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
	vpxord (4*(round))(%r8) {1to4}, s1, RX0x; \
	vpternlogd $0x96, s2, s3, RX0x; /* s1 ^ s2 ^ s3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vgf2p8affineqb $0x65, .Lpre_affine_s rRIP, RX0x, RX0x; \
	vgf2p8affineinvqb $0xd3, .Lpost_affine_s rRIP, RX0x, RX0x; \
	\
	/* linear part */ \
	vpxor RX0x, s0, s0; /* s0 ^ x */ \
	vprold $13, RX0x, RTMP1x; \
	vprold $23, RX0x, RTMP3x; \
	vpternlogd $0x96, RTMP1x, RTMP3x, s0; /* s0 ^ x ^ rol(x,13) ^ rol(x,23) */

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
ELF(.size _gcry_sm4_gfni_avx512_expand_key,.-_gcry_sm4_gfni_avx512_expand_key;)

.align 16
ELF(.type   sm4_gfni_avx512_crypt_blk1_4,@function;)
sm4_gfni_avx512_crypt_blk1_4:
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

	transpose_4x4(RA0x, RA1x, RA2x, RA3x, RTMP0x, RTMP1x);

#define ROUND(round, s0, s1, s2, s3) \
	vpxord (4*(round))(%rdi) {1to4}, s1, RX0x; \
	vpternlogd $0x96, s2, s3, RX0x; /* s1 ^ s2 ^ s3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vgf2p8affineqb $0x65, .Lpre_affine_s rRIP, RX0x, RX0x; \
	vgf2p8affineinvqb $0xd3, .Lpost_affine_s rRIP, RX0x, RX0x; \
	\
	/* linear part */ \
	vprold $2, RX0x, RTMP0x; \
	vprold $10, RX0x, RTMP1x; \
	vprold $18, RX0x, RTMP2x; \
	vpternlogd $0x96, RTMP0x, RX0x, s0; /* s0 ^ x ^ rol(x,2) */ \
	vprold $24, RX0x, RX0x; \
	vpternlogd $0x96, RTMP1x, RTMP2x, RX0x; /* rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	vpxor RX0x, s0, s0; /* s0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */

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
ELF(.size sm4_gfni_avx512_crypt_blk1_4,.-sm4_gfni_avx512_crypt_blk1_4;)

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
	vpbroadcastd (4*(round))(%rdi), RX1x; \
	vmovdqa .Lpre_affine_s rRIP, RTMP2x; \
	vmovdqa .Lpost_affine_s rRIP, RTMP3x; \
	vpxor s1, RX1x, RX0x; \
	vpternlogd $0x96, s2, s3, RX0x; /* s1 ^ s2 ^ s3 ^ rk */ \
	    vpxor r1, RX1x, RX1x; \
	    vpternlogd $0x96, r2, r3, RX1x; /* r1 ^ r2 ^ r3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vgf2p8affineqb $0x65, RTMP2x, RX0x, RX0x; \
	vgf2p8affineinvqb $0xd3, RTMP3x, RX0x, RX0x; \
	    vgf2p8affineqb $0x65, RTMP2x, RX1x, RX1x; \
	    vgf2p8affineinvqb $0xd3, RTMP3x, RX1x, RX1x; \
	\
	/* linear part */ \
	vprold $2, RX0x, RTMP0x; \
	vprold $10, RX0x, RTMP1x; \
	vprold $18, RX0x, RTMP2x; \
	vpternlogd $0x96, RTMP0x, RX0x, s0; /* s0 ^ x ^ rol(x,2) */ \
	vprold $24, RX0x, RX0x; \
	    vprold $2, RX1x, RTMP3x; \
	    vprold $10, RX1x, RTMP4x; \
	    vprold $18, RX1x, RTMP0x; \
	    vpternlogd $0x96, RTMP3x, RX1x, r0; /* r0 ^ x ^ rol(x,2) */ \
	    vprold $24, RX1x, RX1x; \
	vpternlogd $0x96, RTMP1x, RTMP2x, RX0x; /* rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	    vpternlogd $0x96, RTMP4x, RTMP0x, RX1x; /* rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	vpxor RX0x, s0, s0; /* s0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	    vpxor RX1x, r0, r0; /* r0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */

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
ELF(.type   _gcry_sm4_gfni_avx512_crypt_blk1_8,@function;)
_gcry_sm4_gfni_avx512_crypt_blk1_8:
	/* input:
	 *	%rdi: round key array, CTX
	 *	%rsi: dst (1..8 blocks)
	 *	%rdx: src (1..8 blocks)
	 *	%rcx: num blocks (1..8)
	 */
	CFI_STARTPROC();

	cmpq $5, %rcx;
	jb sm4_gfni_avx512_crypt_blk1_4;
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
ELF(.size _gcry_sm4_gfni_avx512_crypt_blk1_8,.-_gcry_sm4_gfni_avx512_crypt_blk1_8;)

/**********************************************************************
  16-way SM4 with GFNI and AVX512 (256-bit vectors)
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
	vpbroadcastd (4*(round))(%rdi), RX1; \
	vbroadcasti128 .Lpre_affine_s rRIP, RTMP2; \
	vbroadcasti128 .Lpost_affine_s rRIP, RTMP3; \
	vpxor s1, RX1, RX0; \
	vpternlogd $0x96, s2, s3, RX0; /* s1 ^ s2 ^ s3 ^ rk */ \
	    vpxor r1, RX1, RX1; \
	    vpternlogd $0x96, r2, r3, RX1; /* r1 ^ r2 ^ r3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vgf2p8affineqb $0x65, RTMP2, RX0, RX0; \
	vgf2p8affineinvqb $0xd3, RTMP3, RX0, RX0; \
	    vgf2p8affineqb $0x65, RTMP2, RX1, RX1; \
	    vgf2p8affineinvqb $0xd3, RTMP3, RX1, RX1; \
	\
	/* linear part */ \
	vprold $2, RX0, RTMP0; \
	vprold $10, RX0, RTMP1; \
	vprold $18, RX0, RTMP2; \
	vpternlogd $0x96, RTMP0, RX0, s0; /* s0 ^ x ^ rol(x,2) */ \
	vprold $24, RX0, RX0; \
	    vprold $2, RX1, RTMP3; \
	    vprold $10, RX1, RTMP4; \
	    vprold $18, RX1, RTMP0; \
	    vpternlogd $0x96, RTMP3, RX1, r0; /* r0 ^ x ^ rol(x,2) */ \
	    vprold $24, RX1, RX1; \
	vpternlogd $0x96, RTMP1, RTMP2, RX0; /* rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	    vpternlogd $0x96, RTMP4, RTMP0, RX1; /* rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	vpxor RX0, s0, s0; /* s0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	    vpxor RX1, r0, r0; /* r0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */

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
.globl _gcry_sm4_gfni_avx512_crypt_blk1_16
ELF(.type   _gcry_sm4_gfni_avx512_crypt_blk1_16,@function;)
_gcry_sm4_gfni_avx512_crypt_blk1_16:
	/* input:
	 *	%rdi: round key array, CTX
	 *	%rsi: dst (1..16 blocks)
	 *	%rdx: src (1..16 blocks)
	 *	%rcx: num blocks (1..16)
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

#define LOAD_INPUT(offset, yreg) \
	cmpq $(1 + 2 * (offset)), %rcx; \
	jb .Lblk16_load_input_done; \
	ja 1f; \
	  vmovdqu (offset) * 32(%rdx), yreg##x; \
	  jmp .Lblk16_load_input_done; \
	1: \
	  vmovdqu (offset) * 32(%rdx), yreg;

	cmpq $8, %rcx;
	jbe _gcry_sm4_gfni_avx512_crypt_blk1_8;
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
ELF(.size _gcry_sm4_gfni_avx512_crypt_blk1_16,.-_gcry_sm4_gfni_avx512_crypt_blk1_16;)

#define add_le128(out, in, lo_counter, hi_counter1) \
	vpaddq lo_counter, in, out; \
	vpcmpuq $1, lo_counter, out, %k1; \
	kaddb %k1, %k1, %k1; \
	vpaddq hi_counter1, out, out{%k1};

.align 16
.globl _gcry_sm4_gfni_avx512_ctr_enc
ELF(.type   _gcry_sm4_gfni_avx512_ctr_enc,@function;)
_gcry_sm4_gfni_avx512_ctr_enc:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv (big endian, 128bit)
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	cmpb $(0x100 - 16), 15(%rcx);
	jbe .Lctr_byteadd16;

	vbroadcasti128 .Lbswap128_mask rRIP, RTMP0;
	vmovdqa .Lcounter0123_lo rRIP, RTMP1;
	vbroadcasti128 .Lcounter2222_lo rRIP, RTMP2;
	vbroadcasti128 .Lcounter4444_lo rRIP, RTMP3;
	vbroadcasti128 .Lcounter8888_lo rRIP, RTMP4;

	/* load IV and byteswap */
	movq 8(%rcx), %r11;
	bswapq %r11;
	vbroadcasti128 (%rcx), RB3;
	vpshufb RTMP0, RB3, RB3;

	/* check need for handling 64-bit overflow and carry */
	cmpq $(0xffffffffffffffff - 16), %r11;
	ja .Lhandle_ctr_carry_blk16;

	/* construct IVs */
	vpaddq RTMP1, RB3, RA0; /* +0:+1 */
	vpaddq RTMP2, RA0, RA1; /* +2:+3 */
	vpaddq RTMP3, RA0, RA2; /* +4:+5 */
	vpaddq RTMP3, RA1, RA3; /* +6:+7 */
	vpaddq RTMP4, RA0, RB0; /* +8... */
	vpaddq RTMP4, RA1, RB1; /* +10... */
	vpaddq RTMP4, RA2, RB2; /* +12... */
	vpaddq RTMP4, RA3, RB3; /* +14... */

	/* Update counter */
	leaq 16(%r11), %r11;
	bswapq %r11;
	movq %r11, 8(%rcx);

	jmp .Lctr_carry_done_blk16;

.Lhandle_ctr_carry_blk16:
	vbroadcasti128 .Lcounter1111_hi rRIP, RNOT;

	/* construct IVs */
	add_le128(RA0, RB3, RTMP1, RNOT); /* +0:+1 */
	add_le128(RA1, RA0, RTMP2, RNOT); /* +2:+3 */
	add_le128(RA2, RA0, RTMP3, RNOT); /* +4:+5 */
	add_le128(RA3, RA1, RTMP3, RNOT); /* +6:+7 */
	add_le128(RB0, RA0, RTMP4, RNOT); /* +8... */
	add_le128(RB1, RA1, RTMP4, RNOT); /* +10... */
	add_le128(RB2, RA2, RTMP4, RNOT); /* +12... */
	add_le128(RB3, RA3, RTMP4, RNOT); /* +14... */

	/* Update counter */
	addq $16, %r11;
	movq (%rcx), %r10;
	bswapq %r10;
	adcq $0, %r10;
	bswapq %r11;
	bswapq %r10;
	movq %r11, 8(%rcx);
	movq %r10, (%rcx);

.align 16
.Lctr_carry_done_blk16:
	/* Byte-swap IVs. */
	vpshufb RTMP0, RA0, RA0;
	vpshufb RTMP0, RA1, RA1;
	vpshufb RTMP0, RA2, RA2;
	vpshufb RTMP0, RA3, RA3;
	vpshufb RTMP0, RB0, RB0;
	vpshufb RTMP0, RB1, RB1;
	vpshufb RTMP0, RB2, RB2;
	vpshufb RTMP0, RB3, RB3;

.align 16
.Lload_ctr_done16:
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
	kxorq %k1, %k1, %k1;

	ret_spec_stop;

.align 16
.Lctr_byteadd_full_ctr_carry16:
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
	jmp .Lctr_byteadd_ymm16;
.align 16
.Lctr_byteadd16:
	vbroadcasti128 (%rcx), RB3;
	je .Lctr_byteadd_full_ctr_carry16;
	addb $16, 15(%rcx);
.Lctr_byteadd_ymm16:
	vpaddb .Lbige_addb_0_1 rRIP, RB3, RA0;
	vpaddb .Lbige_addb_2_3 rRIP, RB3, RA1;
	vpaddb .Lbige_addb_4_5 rRIP, RB3, RA2;
	vpaddb .Lbige_addb_6_7 rRIP, RB3, RA3;
	vpaddb .Lbige_addb_8_9 rRIP, RB3, RB0;
	vpaddb .Lbige_addb_10_11 rRIP, RB3, RB1;
	vpaddb .Lbige_addb_12_13 rRIP, RB3, RB2;
	vpaddb .Lbige_addb_14_15 rRIP, RB3, RB3;

	jmp .Lload_ctr_done16;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_ctr_enc,.-_gcry_sm4_gfni_avx512_ctr_enc;)

.align 16
.globl _gcry_sm4_gfni_avx512_cbc_dec
ELF(.type   _gcry_sm4_gfni_avx512_cbc_dec,@function;)
_gcry_sm4_gfni_avx512_cbc_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

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
ELF(.size _gcry_sm4_gfni_avx512_cbc_dec,.-_gcry_sm4_gfni_avx512_cbc_dec;)

.align 16
.globl _gcry_sm4_gfni_avx512_cfb_dec
ELF(.type   _gcry_sm4_gfni_avx512_cfb_dec,@function;)
_gcry_sm4_gfni_avx512_cfb_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

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
ELF(.size _gcry_sm4_gfni_avx512_cfb_dec,.-_gcry_sm4_gfni_avx512_cfb_dec;)

.align 16
.globl _gcry_sm4_gfni_avx512_ocb_enc
ELF(.type _gcry_sm4_gfni_avx512_ocb_enc,@function;)

_gcry_sm4_gfni_avx512_ocb_enc:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

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

#define OCB_INPUT(n, l0reg, l1reg, yreg, inreg) \
	  vmovdqu (n * 32)(%rdx), inreg; \
	  vpxor (l0reg), RTMP0x, RNOTx; \
	  vpxor (l1reg), RNOTx, RTMP0x; \
	  vinserti128 $1, RTMP0x, RNOT, RNOT; \
	  vpxor inreg, RNOT, yreg; \
	  vmovdqu RNOT, (n * 32)(%rsi);

	movq (0 * 8)(%r9), %r10;
	movq (1 * 8)(%r9), %r11;
	movq (2 * 8)(%r9), %r12;
	movq (3 * 8)(%r9), %r13;
	OCB_INPUT(0, %r10, %r11, RA0, RTMP2);
	OCB_INPUT(1, %r12, %r13, RA1, RTMP3);
	movq (4 * 8)(%r9), %r10;
	movq (5 * 8)(%r9), %r11;
	movq (6 * 8)(%r9), %r12;
	movq (7 * 8)(%r9), %r13;
	OCB_INPUT(2, %r10, %r11, RA2, RTMP4);
	vpternlogd $0x96, RTMP2, RTMP3, RTMP4;
	OCB_INPUT(3, %r12, %r13, RA3, RX0);
	movq (8 * 8)(%r9), %r10;
	movq (9 * 8)(%r9), %r11;
	movq (10 * 8)(%r9), %r12;
	movq (11 * 8)(%r9), %r13;
	OCB_INPUT(4, %r10, %r11, RB0, RX1);
	OCB_INPUT(5, %r12, %r13, RB1, RTMP2);
	vpternlogd $0x96, RX0, RX1, RTMP2;
	movq (12 * 8)(%r9), %r10;
	movq (13 * 8)(%r9), %r11;
	movq (14 * 8)(%r9), %r12;
	movq (15 * 8)(%r9), %r13;
	OCB_INPUT(6, %r10, %r11, RB2, RTMP3);
	OCB_INPUT(7, %r12, %r13, RB3, RX0);
	vpternlogd $0x96, RTMP3, RX0, RTMP1;
#undef OCB_INPUT

	vpternlogd $0x96, RTMP4, RTMP2, RTMP1;
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
ELF(.size _gcry_sm4_gfni_avx512_ocb_enc,.-_gcry_sm4_gfni_avx512_ocb_enc;)

.align 16
.globl _gcry_sm4_gfni_avx512_ocb_dec
ELF(.type _gcry_sm4_gfni_avx512_ocb_dec,@function;)

_gcry_sm4_gfni_avx512_ocb_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

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
	vmovdqu RA1, (1 * 32)(%rsi);
	vmovdqu RA2, (2 * 32)(%rsi);
	vmovdqu RA3, (3 * 32)(%rsi);
	vmovdqu RB0, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RB2, (6 * 32)(%rsi);
	vmovdqu RB3, (7 * 32)(%rsi);

	vpternlogd $0x96, RA0, RA1, RA2;
	vpternlogd $0x96, RA3, RB0, RB1;
	vpternlogd $0x96, RB2, RB3, RA2;
	vpxord RA2, RB1, RTMP1;

	vextracti128 $1, RTMP1, RNOTx;
	vpternlogd $0x96, (%r8), RNOTx, RTMP1x;
	vmovdqu RTMP1x, (%r8);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_ocb_dec,.-_gcry_sm4_gfni_avx512_ocb_dec;)

.align 16
.globl _gcry_sm4_gfni_avx512_ocb_auth
ELF(.type _gcry_sm4_gfni_avx512_ocb_auth,@function;)

_gcry_sm4_gfni_avx512_ocb_auth:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: abuf (16 blocks)
	 *	%rdx: offset
	 *	%rcx: checksum
	 *	%r8 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

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

	vpternlogd $0x96, RA0, RA1, RA2;
	vpternlogd $0x96, RA3, RB0, RB1;
	vpternlogd $0x96, RB2, RB3, RA2;
	vpxor RA2, RB1, RTMP1;

	vextracti128 $1, RTMP1, RNOTx;
	vpternlogd $0x96, (%rcx), RNOTx, RTMP1x;
	vmovdqu RTMP1x, (%rcx);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_ocb_auth,.-_gcry_sm4_gfni_avx512_ocb_auth;)

/**********************************************************************
  32-way SM4 with GFNI and AVX512 (512-bit vectors)
 **********************************************************************/

.align 16
ELF(.type   __sm4_gfni_crypt_blk32,@function;)
__sm4_gfni_crypt_blk32:
	/* input:
	 *	%rdi: ctx, CTX
	 *	RA0z, RA1z, RA2z, RA3z, RB0z, RB1z, RB2z, RB3z: 32 parallel plaintext blocks
	 * output:
	 *	RA0z, RA1z, RA2z, RA3z, RB0z, RB1z, RB2z, RB3z: 32 parallel ciphertext blocks
	 */
	CFI_STARTPROC();

	vbroadcasti32x4 .Lbswap32_mask rRIP, RTMP2z;
	vpshufb RTMP2z, RA0z, RA0z;
	vpshufb RTMP2z, RA1z, RA1z;
	vpshufb RTMP2z, RA2z, RA2z;
	vpshufb RTMP2z, RA3z, RA3z;
	vpshufb RTMP2z, RB0z, RB0z;
	vpshufb RTMP2z, RB1z, RB1z;
	vpshufb RTMP2z, RB2z, RB2z;
	vpshufb RTMP2z, RB3z, RB3z;

	vbroadcasti32x4 .Lpre_affine_s rRIP, %zmm16;
	vbroadcasti32x4 .Lpost_affine_s rRIP, %zmm17;

	transpose_4x4(RA0z, RA1z, RA2z, RA3z, RTMP0z, RTMP1z);
	transpose_4x4(RB0z, RB1z, RB2z, RB3z, RTMP0z, RTMP1z);

#define ROUND(round, s0, s1, s2, s3, r0, r1, r2, r3) \
	vpbroadcastd (4*(round))(%rdi), RX1z; \
	vpxord s1, RX1z, RX0z; \
	vpternlogd $0x96, s2, s3, RX0z; /* s1 ^ s2 ^ s3 ^ rk */ \
	    vpxord r1, RX1z, RX1z; \
	    vpternlogd $0x96, r2, r3, RX1z; /* r1 ^ r2 ^ r3 ^ rk */ \
	\
	/* sbox, non-linear part */ \
	vgf2p8affineqb $0x65, %zmm16, RX0z, RX0z; \
	vgf2p8affineinvqb $0xd3, %zmm17, RX0z, RX0z; \
	    vgf2p8affineqb $0x65, %zmm16, RX1z, RX1z; \
	    vgf2p8affineinvqb $0xd3, %zmm17, RX1z, RX1z; \
	\
	/* linear part */ \
	vprold $2, RX0z, RTMP0z; \
	vprold $10, RX0z, RTMP1z; \
	vprold $18, RX0z, RTMP2z; \
	vpternlogd $0x96, RTMP0z, RX0z, s0; /* s0 ^ x ^ rol(x,2) */ \
	vprold $24, RX0z, RX0z; \
	    vprold $2, RX1z, RTMP3z; \
	    vprold $10, RX1z, RTMP4z; \
	    vprold $18, RX1z, RTMP0z; \
	    vpternlogd $0x96, RTMP3z, RX1z, r0; /* r0 ^ x ^ rol(x,2) */ \
	    vprold $24, RX1z, RX1z; \
	vpternlogd $0x96, RTMP1z, RTMP2z, RX0z; /* rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	    vpternlogd $0x96, RTMP4z, RTMP0z, RX1z; /* rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	vpxord RX0z, s0, s0; /* s0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */ \
	    vpxord RX1z, r0, r0; /* r0 ^ x ^ rol(x,2) ^ rol(x,10) ^ rol(x,18) ^ rol(x,24) */

	leaq (32*4)(%rdi), %rax;
.align 16
.Lroundloop_blk32:
	ROUND(0, RA0z, RA1z, RA2z, RA3z, RB0z, RB1z, RB2z, RB3z);
	ROUND(1, RA1z, RA2z, RA3z, RA0z, RB1z, RB2z, RB3z, RB0z);
	ROUND(2, RA2z, RA3z, RA0z, RA1z, RB2z, RB3z, RB0z, RB1z);
	ROUND(3, RA3z, RA0z, RA1z, RA2z, RB3z, RB0z, RB1z, RB2z);
	leaq (4*4)(%rdi), %rdi;
	cmpq %rax, %rdi;
	jne .Lroundloop_blk32;

#undef ROUND

	vbroadcasti32x4 .Lbswap128_mask rRIP, RTMP2z;

	transpose_4x4(RA0z, RA1z, RA2z, RA3z, RTMP0z, RTMP1z);
	transpose_4x4(RB0z, RB1z, RB2z, RB3z, RTMP0z, RTMP1z);
	vpshufb RTMP2z, RA0z, RA0z;
	vpshufb RTMP2z, RA1z, RA1z;
	vpshufb RTMP2z, RA2z, RA2z;
	vpshufb RTMP2z, RA3z, RA3z;
	vpshufb RTMP2z, RB0z, RB0z;
	vpshufb RTMP2z, RB1z, RB1z;
	vpshufb RTMP2z, RB2z, RB2z;
	vpshufb RTMP2z, RB3z, RB3z;

	vpxord %zmm16, %zmm16, %zmm16;
	vpxord %zmm17, %zmm17, %zmm17;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __sm4_gfni_crypt_blk32,.-__sm4_gfni_crypt_blk32;)

.align 16
.globl _gcry_sm4_gfni_avx512_crypt_blk32
ELF(.type   _gcry_sm4_gfni_avx512_crypt_blk32,@function;)
_gcry_sm4_gfni_avx512_crypt_blk32:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (32 blocks)
	 *	%rdx: src (32 blocks)
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	/* Load input */
	vmovdqu32 (0 * 64)(%rdx), RA0z;
	vmovdqu32 (1 * 64)(%rdx), RA1z;
	vmovdqu32 (2 * 64)(%rdx), RA2z;
	vmovdqu32 (3 * 64)(%rdx), RA3z;
	vmovdqu32 (4 * 64)(%rdx), RB0z;
	vmovdqu32 (5 * 64)(%rdx), RB1z;
	vmovdqu32 (6 * 64)(%rdx), RB2z;
	vmovdqu32 (7 * 64)(%rdx), RB3z;

	call __sm4_gfni_crypt_blk32;

	vmovdqu32 RA0z, (0 * 64)(%rsi);
	vmovdqu32 RA1z, (1 * 64)(%rsi);
	vmovdqu32 RA2z, (2 * 64)(%rsi);
	vmovdqu32 RA3z, (3 * 64)(%rsi);
	vmovdqu32 RB0z, (4 * 64)(%rsi);
	vmovdqu32 RB1z, (5 * 64)(%rsi);
	vmovdqu32 RB2z, (6 * 64)(%rsi);
	vmovdqu32 RB3z, (7 * 64)(%rsi);

	xorl %eax, %eax;
	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_crypt_blk32,.-_gcry_sm4_gfni_avx512_crypt_blk32;)

.align 16
.globl _gcry_sm4_gfni_avx512_ctr_enc_blk32
ELF(.type   _gcry_sm4_gfni_avx512_ctr_enc_blk32,@function;)
_gcry_sm4_gfni_avx512_ctr_enc_blk32:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (32 blocks)
	 *	%rdx: src (32 blocks)
	 *	%rcx: iv (big endian, 128bit)
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	cmpb $(0x100 - 32), 15(%rcx);
	jbe .Lctr_byteadd32;

	vbroadcasti64x2 .Lbswap128_mask rRIP, RTMP0z;
	vmovdqa32 .Lcounter0123_lo rRIP, RTMP1z;
	vbroadcasti64x2 .Lcounter4444_lo rRIP, RTMP2z;
	vbroadcasti64x2 .Lcounter8888_lo rRIP, RTMP3z;
	vbroadcasti64x2 .Lcounter16161616_lo rRIP, RTMP4z;

	/* load IV and byteswap */
	movq 8(%rcx), %r11;
	bswapq %r11;
	vbroadcasti64x2 (%rcx), RB3z;
	vpshufb RTMP0z, RB3z, RB3z;

	/* check need for handling 64-bit overflow and carry */
	cmpq $(0xffffffffffffffff - 32), %r11;
	ja .Lhandle_ctr_carry_blk32;

	/* construct IVs */
	vpaddq RTMP1z, RB3z, RA0z; /* +0:+1:+2:+3 */
	vpaddq RTMP2z, RA0z, RA1z; /* +4:+5:+6:+7 */
	vpaddq RTMP3z, RA0z, RA2z; /* +8:+9:+10:+11 */
	vpaddq RTMP3z, RA1z, RA3z; /* +12:+13:+14:+15 */
	vpaddq RTMP4z, RA0z, RB0z; /* +16... */
	vpaddq RTMP4z, RA1z, RB1z; /* +20... */
	vpaddq RTMP4z, RA2z, RB2z; /* +24... */
	vpaddq RTMP4z, RA3z, RB3z; /* +28... */

	/* Update counter */
	leaq 32(%r11), %r11;
	bswapq %r11;
	movq %r11, 8(%rcx);

	jmp .Lctr_carry_done_blk32;

.Lhandle_ctr_carry_blk32:
	vbroadcasti64x2 .Lcounter1111_hi rRIP, RNOTz;

	/* construct IVs */
	add_le128(RA0z, RB3z, RTMP1z, RNOTz); /* +0:+1:+2:+3 */
	add_le128(RA1z, RA0z, RTMP2z, RNOTz); /* +4:+5:+6:+7 */
	add_le128(RA2z, RA0z, RTMP3z, RNOTz); /* +8:+9:+10:+11 */
	add_le128(RA3z, RA1z, RTMP3z, RNOTz); /* +12:+13:+14:+15 */
	add_le128(RB0z, RA0z, RTMP4z, RNOTz); /* +16... */
	add_le128(RB1z, RA1z, RTMP4z, RNOTz); /* +20... */
	add_le128(RB2z, RA2z, RTMP4z, RNOTz); /* +24... */
	add_le128(RB3z, RA3z, RTMP4z, RNOTz); /* +28... */

	/* Update counter */
	addq $32, %r11;
	movq (%rcx), %r10;
	bswapq %r10;
	adcq $0, %r10;
	bswapq %r11;
	bswapq %r10;
	movq %r11, 8(%rcx);
	movq %r10, (%rcx);

.align 16
.Lctr_carry_done_blk32:
	/* Byte-swap IVs. */
	vpshufb RTMP0z, RA0z, RA0z;
	vpshufb RTMP0z, RA1z, RA1z;
	vpshufb RTMP0z, RA2z, RA2z;
	vpshufb RTMP0z, RA3z, RA3z;
	vpshufb RTMP0z, RB0z, RB0z;
	vpshufb RTMP0z, RB1z, RB1z;
	vpshufb RTMP0z, RB2z, RB2z;
	vpshufb RTMP0z, RB3z, RB3z;

.align 16
.Lload_ctr_done32:
	call __sm4_gfni_crypt_blk32;

	vpxord (0 * 64)(%rdx), RA0z, RA0z;
	vpxord (1 * 64)(%rdx), RA1z, RA1z;
	vpxord (2 * 64)(%rdx), RA2z, RA2z;
	vpxord (3 * 64)(%rdx), RA3z, RA3z;
	vpxord (4 * 64)(%rdx), RB0z, RB0z;
	vpxord (5 * 64)(%rdx), RB1z, RB1z;
	vpxord (6 * 64)(%rdx), RB2z, RB2z;
	vpxord (7 * 64)(%rdx), RB3z, RB3z;

	vmovdqu32 RA0z, (0 * 64)(%rsi);
	vmovdqu32 RA1z, (1 * 64)(%rsi);
	vmovdqu32 RA2z, (2 * 64)(%rsi);
	vmovdqu32 RA3z, (3 * 64)(%rsi);
	vmovdqu32 RB0z, (4 * 64)(%rsi);
	vmovdqu32 RB1z, (5 * 64)(%rsi);
	vmovdqu32 RB2z, (6 * 64)(%rsi);
	vmovdqu32 RB3z, (7 * 64)(%rsi);

	vzeroall;
	kxorq %k1, %k1, %k1;

	ret_spec_stop;

.align 16
.Lctr_byteadd_full_ctr_carry32:
	movq 8(%rcx), %r11;
	movq (%rcx), %r10;
	bswapq %r11;
	bswapq %r10;
	addq $32, %r11;
	adcq $0, %r10;
	bswapq %r11;
	bswapq %r10;
	movq %r11, 8(%rcx);
	movq %r10, (%rcx);
	jmp .Lctr_byteadd_zmm32;
.align 16
.Lctr_byteadd32:
	vbroadcasti64x2 (%rcx), RA3z;
	je .Lctr_byteadd_full_ctr_carry32;
	addb $32, 15(%rcx);
.Lctr_byteadd_zmm32:
	vbroadcasti64x2 .Lbige_addb_16 rRIP, RB3z;
	vpaddb RB3z, RA3z, RB3z;
	vpaddb .Lbige_addb_0_1 rRIP, RA3z, RA0z;
	vpaddb .Lbige_addb_4_5 rRIP, RA3z, RA1z;
	vpaddb .Lbige_addb_8_9 rRIP, RA3z, RA2z;
	vpaddb .Lbige_addb_12_13 rRIP, RA3z, RA3z;
	vpaddb .Lbige_addb_0_1 rRIP, RB3z, RB0z;
	vpaddb .Lbige_addb_4_5 rRIP, RB3z, RB1z;
	vpaddb .Lbige_addb_8_9 rRIP, RB3z, RB2z;
	vpaddb .Lbige_addb_12_13 rRIP, RB3z, RB3z;

	jmp .Lload_ctr_done32;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_ctr_enc_blk32,.-_gcry_sm4_gfni_avx512_ctr_enc_blk32;)

.align 16
.globl _gcry_sm4_gfni_avx512_cbc_dec_blk32
ELF(.type   _gcry_sm4_gfni_avx512_cbc_dec_blk32,@function;)
_gcry_sm4_gfni_avx512_cbc_dec_blk32:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (32 blocks)
	 *	%rdx: src (32 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	vmovdqu32 (0 * 64)(%rdx), RA0z;
	vmovdqu32 (1 * 64)(%rdx), RA1z;
	vmovdqu32 (2 * 64)(%rdx), RA2z;
	vmovdqu32 (3 * 64)(%rdx), RA3z;
	vmovdqu32 (4 * 64)(%rdx), RB0z;
	vmovdqu32 (5 * 64)(%rdx), RB1z;
	vmovdqu32 (6 * 64)(%rdx), RB2z;
	vmovdqu32 (7 * 64)(%rdx), RB3z;

	call __sm4_gfni_crypt_blk32;

	vmovdqu (%rcx), RNOTx;
	vinserti64x2 $1, (0 * 16)(%rdx), RNOT, RNOT;
	vinserti64x4 $1, (1 * 16)(%rdx), RNOTz, RNOTz;
	vpxord RNOTz, RA0z, RA0z;
	vpxord (0 * 64 + 48)(%rdx), RA1z, RA1z;
	vpxord (1 * 64 + 48)(%rdx), RA2z, RA2z;
	vpxord (2 * 64 + 48)(%rdx), RA3z, RA3z;
	vpxord (3 * 64 + 48)(%rdx), RB0z, RB0z;
	vpxord (4 * 64 + 48)(%rdx), RB1z, RB1z;
	vpxord (5 * 64 + 48)(%rdx), RB2z, RB2z;
	vpxord (6 * 64 + 48)(%rdx), RB3z, RB3z;
	vmovdqu (7 * 64 + 48)(%rdx), RNOTx;
	vmovdqu RNOTx, (%rcx); /* store new IV */

	vmovdqu32 RA0z, (0 * 64)(%rsi);
	vmovdqu32 RA1z, (1 * 64)(%rsi);
	vmovdqu32 RA2z, (2 * 64)(%rsi);
	vmovdqu32 RA3z, (3 * 64)(%rsi);
	vmovdqu32 RB0z, (4 * 64)(%rsi);
	vmovdqu32 RB1z, (5 * 64)(%rsi);
	vmovdqu32 RB2z, (6 * 64)(%rsi);
	vmovdqu32 RB3z, (7 * 64)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_cbc_dec_blk32,.-_gcry_sm4_gfni_avx512_cbc_dec_blk32;)

.align 16
.globl _gcry_sm4_gfni_avx512_cfb_dec_blk32
ELF(.type   _gcry_sm4_gfni_avx512_cfb_dec_blk32,@function;)
_gcry_sm4_gfni_avx512_cfb_dec_blk32:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (32 blocks)
	 *	%rdx: src (32 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	/* Load input */
	vmovdqu (%rcx), RA0x;
	vinserti64x2 $1, (%rdx), RA0, RA0;
	vinserti64x4 $1, 16(%rdx), RA0z, RA0z;
	vmovdqu32 (0 * 64 + 48)(%rdx), RA1z;
	vmovdqu32 (1 * 64 + 48)(%rdx), RA2z;
	vmovdqu32 (2 * 64 + 48)(%rdx), RA3z;
	vmovdqu32 (3 * 64 + 48)(%rdx), RB0z;
	vmovdqu32 (4 * 64 + 48)(%rdx), RB1z;
	vmovdqu32 (5 * 64 + 48)(%rdx), RB2z;
	vmovdqu32 (6 * 64 + 48)(%rdx), RB3z;

	/* Update IV */
	vmovdqu (7 * 64 + 48)(%rdx), RNOTx;
	vmovdqu RNOTx, (%rcx);

	call __sm4_gfni_crypt_blk32;

	vpxord (0 * 64)(%rdx), RA0z, RA0z;
	vpxord (1 * 64)(%rdx), RA1z, RA1z;
	vpxord (2 * 64)(%rdx), RA2z, RA2z;
	vpxord (3 * 64)(%rdx), RA3z, RA3z;
	vpxord (4 * 64)(%rdx), RB0z, RB0z;
	vpxord (5 * 64)(%rdx), RB1z, RB1z;
	vpxord (6 * 64)(%rdx), RB2z, RB2z;
	vpxord (7 * 64)(%rdx), RB3z, RB3z;

	vmovdqu32 RA0z, (0 * 64)(%rsi);
	vmovdqu32 RA1z, (1 * 64)(%rsi);
	vmovdqu32 RA2z, (2 * 64)(%rsi);
	vmovdqu32 RA3z, (3 * 64)(%rsi);
	vmovdqu32 RB0z, (4 * 64)(%rsi);
	vmovdqu32 RB1z, (5 * 64)(%rsi);
	vmovdqu32 RB2z, (6 * 64)(%rsi);
	vmovdqu32 RB3z, (7 * 64)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_cfb_dec_blk32,.-_gcry_sm4_gfni_avx512_cfb_dec_blk32;)

.align 16
.globl _gcry_sm4_gfni_avx512_ocb_enc_blk32
ELF(.type _gcry_sm4_gfni_avx512_ocb_enc_blk32,@function;)
_gcry_sm4_gfni_avx512_ocb_enc_blk32:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (32 blocks)
	 *	%rdx: src (32 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[32])
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	subq $(5 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(5 * 8);

	movq %r12, (0 * 8)(%rsp);
	movq %r13, (1 * 8)(%rsp);
	movq %r14, (2 * 8)(%rsp);
	movq %r15, (3 * 8)(%rsp);
	movq %rbx, (4 * 8)(%rsp);
	CFI_REL_OFFSET(%r12, 0 * 8);
	CFI_REL_OFFSET(%r13, 1 * 8);
	CFI_REL_OFFSET(%r14, 2 * 8);
	CFI_REL_OFFSET(%r15, 3 * 8);
	CFI_REL_OFFSET(%rbx, 4 * 8);

	vmovdqu (%rcx), RTMP0x;

	/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
	/* Checksum_i = Checksum_{i-1} xor P_i  */
	/* C_i = Offset_i xor ENCIPHER(K, P_i xor Offset_i)  */

#define OCB_INPUT(n, l0reg, l1reg, l2reg, l3reg, zreg, zplain) \
	  vmovdqu32 (n * 64)(%rdx), zplain; \
	  vpxor (l0reg), RTMP0x, RNOTx; \
	  vpxor (l1reg), RNOTx, RTMP0x; \
	  vinserti64x2 $1, RTMP0x, RNOT, RNOT; \
	  vpxor (l2reg), RTMP0x, RTMP0x; \
	  vinserti64x2 $2, RTMP0x, RNOTz, RNOTz; \
	  vpxor (l3reg), RTMP0x, RTMP0x; \
	  vinserti64x2 $3, RTMP0x, RNOTz, RNOTz; \
	  vpxord zplain, RNOTz, zreg; \
	  vmovdqu32 RNOTz, (n * 64)(%rsi);

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
	OCB_INPUT(0, %r10, %r11, %r12, %r13, RA0z, RTMP1z);
	OCB_INPUT(1, %r14, %r15, %rax, %rbx, RA1z, RTMP2z);
	OCB_LOAD_PTRS(2);
	OCB_INPUT(2, %r10, %r11, %r12, %r13, RA2z, RTMP3z);
	vpternlogd $0x96, RTMP1z, RTMP2z, RTMP3z;
	OCB_INPUT(3, %r14, %r15, %rax, %rbx, RA3z, RTMP4z);
	OCB_LOAD_PTRS(4);
	OCB_INPUT(4, %r10, %r11, %r12, %r13, RB0z, RX0z);
	OCB_INPUT(5, %r14, %r15, %rax, %rbx, RB1z, RX1z);
	vpternlogd $0x96, RTMP4z, RX0z, RX1z;
	OCB_LOAD_PTRS(6);
	OCB_INPUT(6, %r10, %r11, %r12, %r13, RB2z, RTMP4z);
	OCB_INPUT(7, %r14, %r15, %rax, %rbx, RB3z, RX0z);
#undef OCB_LOAD_PTRS
#undef OCB_INPUT

	vpternlogd $0x96, RTMP3z, RTMP4z, RX0z;
	vpxord RX1z, RX0z, RNOTz;
	vextracti64x4 $1, RNOTz, RTMP1;
	vpxor RTMP1, RNOT, RNOT;
	vextracti128 $1, RNOT, RTMP1x;
	vpternlogd $0x96, (%r8), RTMP1x, RNOTx;

	movq (0 * 8)(%rsp), %r12;
	movq (1 * 8)(%rsp), %r13;
	movq (2 * 8)(%rsp), %r14;
	movq (3 * 8)(%rsp), %r15;
	movq (4 * 8)(%rsp), %rbx;
	CFI_RESTORE(%r12);
	CFI_RESTORE(%r13);
	CFI_RESTORE(%r14);
	CFI_RESTORE(%r15);
	CFI_RESTORE(%rbx);

	vmovdqu RTMP0x, (%rcx);
	vmovdqu RNOTx, (%r8);

	call __sm4_gfni_crypt_blk32;

	addq $(5 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(-5 * 8);

	vpxord (0 * 64)(%rsi), RA0z, RA0z;
	vpxord (1 * 64)(%rsi), RA1z, RA1z;
	vpxord (2 * 64)(%rsi), RA2z, RA2z;
	vpxord (3 * 64)(%rsi), RA3z, RA3z;
	vpxord (4 * 64)(%rsi), RB0z, RB0z;
	vpxord (5 * 64)(%rsi), RB1z, RB1z;
	vpxord (6 * 64)(%rsi), RB2z, RB2z;
	vpxord (7 * 64)(%rsi), RB3z, RB3z;

	vmovdqu32 RA0z, (0 * 64)(%rsi);
	vmovdqu32 RA1z, (1 * 64)(%rsi);
	vmovdqu32 RA2z, (2 * 64)(%rsi);
	vmovdqu32 RA3z, (3 * 64)(%rsi);
	vmovdqu32 RB0z, (4 * 64)(%rsi);
	vmovdqu32 RB1z, (5 * 64)(%rsi);
	vmovdqu32 RB2z, (6 * 64)(%rsi);
	vmovdqu32 RB3z, (7 * 64)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_ocb_enc_blk32,.-_gcry_sm4_gfni_avx512_ocb_enc_blk32;)

.align 16
.globl _gcry_sm4_gfni_avx512_ocb_dec_blk32
ELF(.type _gcry_sm4_gfni_avx512_ocb_dec_blk32,@function;)
_gcry_sm4_gfni_avx512_ocb_dec_blk32:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (32 blocks)
	 *	%rdx: src (32 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[32])
	 */
	CFI_STARTPROC();
	spec_stop_avx512;

	subq $(5 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(5 * 8);

	movq %r12, (0 * 8)(%rsp);
	movq %r13, (1 * 8)(%rsp);
	movq %r14, (2 * 8)(%rsp);
	movq %r15, (3 * 8)(%rsp);
	movq %rbx, (4 * 8)(%rsp);
	CFI_REL_OFFSET(%r12, 0 * 8);
	CFI_REL_OFFSET(%r13, 1 * 8);
	CFI_REL_OFFSET(%r14, 2 * 8);
	CFI_REL_OFFSET(%r15, 3 * 8);
	CFI_REL_OFFSET(%rbx, 4 * 8);

	vmovdqu (%rcx), RTMP0x;

	/* Offset_i = Offset_{i-1} xor L_{ntz(i)} */
	/* C_i = Offset_i xor DECIPHER(K, P_i xor Offset_i)  */

#define OCB_INPUT(n, l0reg, l1reg, l2reg, l3reg, zreg) \
	  vmovdqu32 (n * 64)(%rdx), RTMP1z; \
	  vpxor (l0reg), RTMP0x, RNOTx; \
	  vpxor (l1reg), RNOTx, RTMP0x; \
	  vinserti64x2 $1, RTMP0x, RNOT, RNOT; \
	  vpxor (l2reg), RTMP0x, RTMP0x; \
	  vinserti64x2 $2, RTMP0x, RNOTz, RNOTz; \
	  vpxor (l3reg), RTMP0x, RTMP0x; \
	  vinserti64x2 $3, RTMP0x, RNOTz, RNOTz; \
	  vpxord RTMP1z, RNOTz, zreg; \
	  vmovdqu32 RNOTz, (n * 64)(%rsi);

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
	OCB_INPUT(0, %r10, %r11, %r12, %r13, RA0z);
	OCB_INPUT(1, %r14, %r15, %rax, %rbx, RA1z);
	OCB_LOAD_PTRS(2);
	OCB_INPUT(2, %r10, %r11, %r12, %r13, RA2z);
	OCB_INPUT(3, %r14, %r15, %rax, %rbx, RA3z);
	OCB_LOAD_PTRS(4);
	OCB_INPUT(4, %r10, %r11, %r12, %r13, RB0z);
	OCB_INPUT(5, %r14, %r15, %rax, %rbx, RB1z);
	OCB_LOAD_PTRS(6);
	OCB_INPUT(6, %r10, %r11, %r12, %r13, RB2z);
	OCB_INPUT(7, %r14, %r15, %rax, %rbx, RB3z);
#undef OCB_LOAD_PTRS
#undef OCB_INPUT

	movq (0 * 8)(%rsp), %r12;
	movq (1 * 8)(%rsp), %r13;
	movq (2 * 8)(%rsp), %r14;
	movq (3 * 8)(%rsp), %r15;
	movq (4 * 8)(%rsp), %rbx;
	CFI_RESTORE(%r12);
	CFI_RESTORE(%r13);
	CFI_RESTORE(%r14);
	CFI_RESTORE(%r15);
	CFI_RESTORE(%rbx);

	vmovdqu RTMP0x, (%rcx);

	call __sm4_gfni_crypt_blk32;

	addq $(5 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(-5 * 8);

	vpxord (0 * 64)(%rsi), RA0z, RA0z;
	vpxord (1 * 64)(%rsi), RA1z, RA1z;
	vpxord (2 * 64)(%rsi), RA2z, RA2z;
	vpxord (3 * 64)(%rsi), RA3z, RA3z;
	vpxord (4 * 64)(%rsi), RB0z, RB0z;
	vpxord (5 * 64)(%rsi), RB1z, RB1z;
	vpxord (6 * 64)(%rsi), RB2z, RB2z;
	vpxord (7 * 64)(%rsi), RB3z, RB3z;

	vmovdqu32 RA0z, (0 * 64)(%rsi);
	vmovdqu32 RA1z, (1 * 64)(%rsi);
	vmovdqu32 RA2z, (2 * 64)(%rsi);
	vmovdqu32 RA3z, (3 * 64)(%rsi);
	vmovdqu32 RB0z, (4 * 64)(%rsi);
	vmovdqu32 RB1z, (5 * 64)(%rsi);
	vmovdqu32 RB2z, (6 * 64)(%rsi);
	vmovdqu32 RB3z, (7 * 64)(%rsi);

	/* Checksum_i = Checksum_{i-1} xor C_i  */
	vpternlogd $0x96, RA0z, RA1z, RA2z;
	vpternlogd $0x96, RA3z, RB0z, RB1z;
	vpternlogd $0x96, RB2z, RB3z, RA2z;
	vpxord RA2z, RB1z, RTMP1z;

	vextracti64x4 $1, RTMP1z, RNOT;
	vpxor RNOT, RTMP1, RTMP1;
	vextracti128 $1, RTMP1, RNOTx;
	vpternlogd $0x96, (%r8), RNOTx, RTMP1x;
	vmovdqu RTMP1x, (%r8);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_sm4_gfni_avx512_ocb_dec_blk32,.-_gcry_sm4_gfni_avx512_ocb_dec_blk32;)

#endif /*defined(ENABLE_GFNI_SUPPORT) && defined(ENABLE_AVX512_SUPPORT)*/
#endif /*__x86_64*/
