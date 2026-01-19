/* twofish-avx2-amd64.S  -  AMD64/AVX2 assembly implementation of Twofish cipher
 *
 * Copyright (C) 2013-2017 Jussi Kivilinna <jussi.kivilinna@iki.fi>
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

#ifdef __x86_64
#include <config.h>
#if (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
    defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS)) && defined(USE_TWOFISH) && \
    defined(ENABLE_AVX2_SUPPORT)

#include "asm-common-amd64.h"

.text

/* structure of TWOFISH_context: */
#define s0	0
#define s1	((s0) + 4 * 256)
#define s2	((s1) + 4 * 256)
#define s3	((s2) + 4 * 256)
#define w	((s3) + 4 * 256)
#define k	((w) + 4 * 8)

/* register macros */
#define CTX	%rdi

#define RROUND  %r13
#define RROUNDd %r13d
#define RS0	CTX
#define RS1	%r8
#define RS2	%r9
#define RS3	%r10
#define RK	%r11
#define RW	%r12
#define RIDX0	%rax
#define RIDX0d	%eax
#define RIDX1	%rbx
#define RIDX1d	%ebx
#define RIDX2	%r14
#define RIDX3	%r15

#define RA0	%ymm8
#define RB0	%ymm9
#define RC0	%ymm10
#define RD0	%ymm11
#define RA1	%ymm12
#define RB1	%ymm13
#define RC1	%ymm14
#define RD1	%ymm15

/* temp regs */
#define RX0	%ymm0
#define RY0	%ymm1
#define RX1	%ymm2
#define RY1	%ymm3
#define RT0	%ymm4
#define RT1	%ymm5

#define RX0x	%xmm0
#define RY0x	%xmm1
#define RX1x	%xmm2
#define RY1x	%xmm3
#define RT0x	%xmm4
#define RT1x	%xmm5

#define RTMP0   RX0
#define RTMP0x  RX0x
#define RTMP1   RX1
#define RTMP1x  RX1x
#define RTMP2   RY0
#define RTMP2x  RY0x
#define RTMP3   RY1
#define RTMP3x  RY1x
#define RTMP4   RT1
#define RTMP4x  RT1x

/* vpgatherdd mask and '-1' */
#define RNOT	%ymm6
#define RNOTx	%xmm6

/* byte mask, (-1 >> 24) */
#define RBYTE	%ymm7

/**********************************************************************
  16-way AVX2 twofish
 **********************************************************************/
#define init_round_constants() \
	vpcmpeqd RNOT, RNOT, RNOT; \
	leaq k(CTX), RK; \
	leaq w(CTX), RW; \
	vpsrld $24, RNOT, RBYTE; \
	leaq s1(CTX), RS1; \
	leaq s2(CTX), RS2; \
	leaq s3(CTX), RS3; \

#define do_gather(stoffs, byteoffs, rs, out) \
	movzbl (stoffs + 0*4 + byteoffs)(%rsp), RIDX0d; \
	movzbl (stoffs + 1*4 + byteoffs)(%rsp), RIDX1d; \
	movzbq (stoffs + 2*4 + byteoffs)(%rsp), RIDX2; \
	movzbq (stoffs + 3*4 + byteoffs)(%rsp), RIDX3; \
	vmovd (rs, RIDX0, 4), RT1x; \
	movzbl (stoffs + 4*4 + byteoffs)(%rsp), RIDX0d; \
	vmovd (rs, RIDX0, 4), RT0x; \
	vpinsrd $1, (rs, RIDX1, 4), RT1x, RT1x; \
	movzbl (stoffs + 5*4 + byteoffs)(%rsp), RIDX1d; \
	vpinsrd $1, (rs, RIDX1, 4), RT0x, RT0x; \
	vpinsrd $2, (rs, RIDX2, 4), RT1x, RT1x; \
	movzbq (stoffs + 6*4 + byteoffs)(%rsp), RIDX2; \
	vpinsrd $2, (rs, RIDX2, 4), RT0x, RT0x; \
	vpinsrd $3, (rs, RIDX3, 4), RT1x, RT1x; \
	movzbq (stoffs + 7*4 + byteoffs)(%rsp), RIDX3; \
	vpinsrd $3, (rs, RIDX3, 4), RT0x, RT0x; \
	vinserti128 $1, RT0x, RT1, out;

#define g16(ab, rs0, rs1, rs2, rs3, xy) \
	vmovdqa ab ## 0, 0(%rsp); \
	vmovdqa ab ## 1, 32(%rsp); \
	do_gather(0*32, 0, rs0, xy ## 0); \
		do_gather(1*32, 0, rs0, xy ## 1); \
	do_gather(0*32, 1, rs1, RT1); \
	vpxor RT1, xy ## 0, xy ## 0; \
		do_gather(1*32, 1, rs1, RT1); \
		vpxor RT1, xy ## 1, xy ## 1; \
	do_gather(0*32, 2, rs2, RT1); \
	vpxor RT1, xy ## 0, xy ## 0; \
		do_gather(1*32, 2, rs2, RT1); \
		vpxor RT1, xy ## 1, xy ## 1; \
	do_gather(0*32, 3, rs3, RT1); \
	vpxor RT1, xy ## 0, xy ## 0; \
		do_gather(1*32, 3, rs3, RT1); \
		vpxor RT1, xy ## 1, xy ## 1;

#define g1_16(a, x) \
	g16(a, RS0, RS1, RS2, RS3, x);

#define g2_16(b, y) \
	g16(b, RS1, RS2, RS3, RS0, y);

#define encrypt_round_end16(a, b, c, d, nk, r) \
	vpaddd RY0, RX0, RX0; \
	vpaddd RX0, RY0, RY0; \
	vpbroadcastd ((nk))(RK,r), RT0; \
	vpaddd RT0, RX0, RX0; \
	vpbroadcastd 4+((nk))(RK,r), RT0; \
	vpaddd RT0, RY0, RY0; \
	\
	vpxor RY0, d ## 0, d ## 0; \
	\
	vpxor RX0, c ## 0, c ## 0; \
	vpsrld $1, c ## 0, RT0; \
	vpslld $31, c ## 0, c ## 0; \
	vpor RT0, c ## 0, c ## 0; \
	\
		vpaddd RY1, RX1, RX1; \
		vpaddd RX1, RY1, RY1; \
		vpbroadcastd ((nk))(RK,r), RT0; \
		vpaddd RT0, RX1, RX1; \
		vpbroadcastd 4+((nk))(RK,r), RT0; \
		vpaddd RT0, RY1, RY1; \
		\
		vpxor RY1, d ## 1, d ## 1; \
		\
		vpxor RX1, c ## 1, c ## 1; \
		vpsrld $1, c ## 1, RT0; \
		vpslld $31, c ## 1, c ## 1; \
		vpor RT0, c ## 1, c ## 1; \

#define encrypt_round16(a, b, c, d, nk, r) \
	g2_16(b, RY); \
	\
	vpslld $1, b ## 0, RT0; \
	vpsrld $31, b ## 0, b ## 0; \
	vpor RT0, b ## 0, b ## 0; \
	\
		vpslld $1, b ## 1, RT0; \
		vpsrld $31, b ## 1, b ## 1; \
		vpor RT0, b ## 1, b ## 1; \
	\
	g1_16(a, RX); \
	\
	encrypt_round_end16(a, b, c, d, nk, r);

#define encrypt_round_first16(a, b, c, d, nk, r) \
	vpslld $1, d ## 0, RT0; \
	vpsrld $31, d ## 0, d ## 0; \
	vpor RT0, d ## 0, d ## 0; \
	\
		vpslld $1, d ## 1, RT0; \
		vpsrld $31, d ## 1, d ## 1; \
		vpor RT0, d ## 1, d ## 1; \
	\
	encrypt_round16(a, b, c, d, nk, r);

#define encrypt_round_last16(a, b, c, d, nk, r) \
	g2_16(b, RY); \
	\
	g1_16(a, RX); \
	\
	encrypt_round_end16(a, b, c, d, nk, r);

#define decrypt_round_end16(a, b, c, d, nk, r) \
	vpaddd RY0, RX0, RX0; \
	vpaddd RX0, RY0, RY0; \
	vpbroadcastd ((nk))(RK,r), RT0; \
	vpaddd RT0, RX0, RX0; \
	vpbroadcastd 4+((nk))(RK,r), RT0; \
	vpaddd RT0, RY0, RY0; \
	\
	vpxor RX0, c ## 0, c ## 0; \
	\
	vpxor RY0, d ## 0, d ## 0; \
	vpsrld $1, d ## 0, RT0; \
	vpslld $31, d ## 0, d ## 0; \
	vpor RT0, d ## 0, d ## 0; \
	\
		vpaddd RY1, RX1, RX1; \
		vpaddd RX1, RY1, RY1; \
		vpbroadcastd ((nk))(RK,r), RT0; \
		vpaddd RT0, RX1, RX1; \
		vpbroadcastd 4+((nk))(RK,r), RT0; \
		vpaddd RT0, RY1, RY1; \
		\
		vpxor RX1, c ## 1, c ## 1; \
		\
		vpxor RY1, d ## 1, d ## 1; \
		vpsrld $1, d ## 1, RT0; \
		vpslld $31, d ## 1, d ## 1; \
		vpor RT0, d ## 1, d ## 1;

#define decrypt_round16(a, b, c, d, nk, r) \
	g1_16(a, RX); \
	\
	vpslld $1, a ## 0, RT0; \
	vpsrld $31, a ## 0, a ## 0; \
	vpor RT0, a ## 0, a ## 0; \
	\
		vpslld $1, a ## 1, RT0; \
		vpsrld $31, a ## 1, a ## 1; \
		vpor RT0, a ## 1, a ## 1; \
	\
	g2_16(b, RY); \
	\
	decrypt_round_end16(a, b, c, d, nk, r);

#define decrypt_round_first16(a, b, c, d, nk, r) \
	vpslld $1, c ## 0, RT0; \
	vpsrld $31, c ## 0, c ## 0; \
	vpor RT0, c ## 0, c ## 0; \
	\
		vpslld $1, c ## 1, RT0; \
		vpsrld $31, c ## 1, c ## 1; \
		vpor RT0, c ## 1, c ## 1; \
	\
	decrypt_round16(a, b, c, d, nk, r)

#define decrypt_round_last16(a, b, c, d, nk, r) \
	g1_16(a, RX); \
	\
	g2_16(b, RY); \
	\
	decrypt_round_end16(a, b, c, d, nk, r);

#define transpose_4x4(x0,x1,x2,x3,t1,t2) \
	vpunpckhdq x1, x0, t2; \
	vpunpckldq x1, x0, x0; \
	\
	vpunpckldq x3, x2, t1; \
	vpunpckhdq x3, x2, x2; \
	\
	vpunpckhqdq t1,	x0, x1; \
	vpunpcklqdq t1,	x0, x0; \
	\
	vpunpckhqdq x2, t2, x3; \
	vpunpcklqdq x2,	t2, x2;

#define inpack_enc8(a,b,c,d) \
	vpbroadcastd 4*0(RW), RT0; \
	vpxor RT0, a, a; \
	\
	vpbroadcastd 4*1(RW), RT0; \
	vpxor RT0, b, b; \
	\
	vpbroadcastd 4*2(RW), RT0; \
	vpxor RT0, c, c; \
	\
	vpbroadcastd 4*3(RW), RT0; \
	vpxor RT0, d, d;

#define outunpack_enc8(a,b,c,d) \
	vpbroadcastd 4*4(RW), RX0; \
	vpbroadcastd 4*5(RW), RY0; \
	vpxor RX0, c, RX0; \
	vpxor RY0, d, RY0; \
	\
	vpbroadcastd 4*6(RW), RT0; \
	vpxor RT0, a, c; \
	vpbroadcastd 4*7(RW), RT0; \
	vpxor RT0, b, d; \
	\
	vmovdqa RX0, a; \
	vmovdqa RY0, b;

#define inpack_dec8(a,b,c,d) \
	vpbroadcastd 4*4(RW), RX0; \
	vpbroadcastd 4*5(RW), RY0; \
	vpxor RX0, a, RX0; \
	vpxor RY0, b, RY0; \
	\
	vpbroadcastd 4*6(RW), RT0; \
	vpxor RT0, c, a; \
	vpbroadcastd 4*7(RW), RT0; \
	vpxor RT0, d, b; \
	\
	vmovdqa RX0, c; \
	vmovdqa RY0, d;

#define outunpack_dec8(a,b,c,d) \
	vpbroadcastd 4*0(RW), RT0; \
	vpxor RT0, a, a; \
	\
	vpbroadcastd 4*1(RW), RT0; \
	vpxor RT0, b, b; \
	\
	vpbroadcastd 4*2(RW), RT0; \
	vpxor RT0, c, c; \
	\
	vpbroadcastd 4*3(RW), RT0; \
	vpxor RT0, d, d;

#define transpose4x4_16(a,b,c,d) \
	transpose_4x4(a ## 0, b ## 0, c ## 0, d ## 0, RX0, RY0); \
	transpose_4x4(a ## 1, b ## 1, c ## 1, d ## 1, RX0, RY0);

#define inpack_enc16(a,b,c,d) \
	inpack_enc8(a ## 0, b ## 0, c ## 0, d ## 0); \
	inpack_enc8(a ## 1, b ## 1, c ## 1, d ## 1);

#define outunpack_enc16(a,b,c,d) \
	outunpack_enc8(a ## 0, b ## 0, c ## 0, d ## 0); \
	outunpack_enc8(a ## 1, b ## 1, c ## 1, d ## 1);

#define inpack_dec16(a,b,c,d) \
	inpack_dec8(a ## 0, b ## 0, c ## 0, d ## 0); \
	inpack_dec8(a ## 1, b ## 1, c ## 1, d ## 1);

#define outunpack_dec16(a,b,c,d) \
	outunpack_dec8(a ## 0, b ## 0, c ## 0, d ## 0); \
	outunpack_dec8(a ## 1, b ## 1, c ## 1, d ## 1);

.align 16
ELF(.type __twofish_enc_blk16,@function;)
__twofish_enc_blk16:
	/* input:
	 *	%rdi: ctx, CTX
	 *	RA0, RB0, RC0, RD0, RA1, RB1, RC1, RD1: sixteen parallel
	 *						plaintext blocks
	 * output:
	 *	RA0, RB0, RC0, RD0, RA1, RB1, RC1, RD1: sixteen parallel
	 *						ciphertext blocks
	 */
	CFI_STARTPROC();

	pushq %rbp;
	CFI_PUSH(%rbp);
	movq %rsp, %rbp;
	CFI_DEF_CFA_REGISTER(%rbp);
	subq $(64 + 5 * 8), %rsp;
	andq $-64, %rsp;

	movq %rbx, (64 + 0 * 8)(%rsp);
	movq %r12, (64 + 1 * 8)(%rsp);
	movq %r13, (64 + 2 * 8)(%rsp);
	movq %r14, (64 + 3 * 8)(%rsp);
	movq %r15, (64 + 4 * 8)(%rsp);
	CFI_REG_ON_STACK(rbx, 64 + 0 * 8);
	CFI_REG_ON_STACK(r12, 64 + 1 * 8);
	CFI_REG_ON_STACK(r13, 64 + 2 * 8);
	CFI_REG_ON_STACK(r14, 64 + 3 * 8);
	CFI_REG_ON_STACK(r15, 64 + 4 * 8);

	init_round_constants();

	transpose4x4_16(RA, RB, RC, RD);
	inpack_enc16(RA, RB, RC, RD);

	xorl RROUNDd, RROUNDd;

	encrypt_round_first16(RA, RB, RC, RD, 0, RROUND);

.align 16
.Loop_enc16:
	encrypt_round16(RC, RD, RA, RB, 8, RROUND);
	encrypt_round16(RA, RB, RC, RD, 16, RROUND);
	leal 16(RROUNDd), RROUNDd;
	cmpl $8*14, RROUNDd;
	jb .Loop_enc16;

	encrypt_round_last16(RC, RD, RA, RB, 8, RROUND);

	outunpack_enc16(RA, RB, RC, RD);
	transpose4x4_16(RA, RB, RC, RD);

	movq (64 + 0 * 8)(%rsp), %rbx;
	movq (64 + 1 * 8)(%rsp), %r12;
	movq (64 + 2 * 8)(%rsp), %r13;
	movq (64 + 3 * 8)(%rsp), %r14;
	movq (64 + 4 * 8)(%rsp), %r15;
	CFI_RESTORE(%rbx);
	CFI_RESTORE(%r12);
	CFI_RESTORE(%r13);
	CFI_RESTORE(%r14);
	CFI_RESTORE(%r15);
	vpxor RT0, RT0, RT0;
	vmovdqa RT0, 0(%rsp);
	vmovdqa RT0, 32(%rsp);
	leave;
	CFI_LEAVE();

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __twofish_enc_blk16,.-__twofish_enc_blk16;)

.align 16
ELF(.type __twofish_dec_blk16,@function;)
__twofish_dec_blk16:
	/* input:
	 *	%rdi: ctx, CTX
	 *	RA0, RB0, RC0, RD0, RA1, RB1, RC1, RD1: sixteen parallel
	 *						plaintext blocks
	 * output:
	 *	RA0, RB0, RC0, RD0, RA1, RB1, RC1, RD1: sixteen parallel
	 *						ciphertext blocks
	 */
	CFI_STARTPROC();

	pushq %rbp;
	CFI_PUSH(%rbp);
	movq %rsp, %rbp;
	CFI_DEF_CFA_REGISTER(%rbp);
	subq $(64 + 5 * 8), %rsp;
	andq $-64, %rsp;

	movq %rbx, (64 + 0 * 8)(%rsp);
	movq %r12, (64 + 1 * 8)(%rsp);
	movq %r13, (64 + 2 * 8)(%rsp);
	movq %r14, (64 + 3 * 8)(%rsp);
	movq %r15, (64 + 4 * 8)(%rsp);
	CFI_REG_ON_STACK(rbx, 64 + 0 * 8);
	CFI_REG_ON_STACK(r12, 64 + 1 * 8);
	CFI_REG_ON_STACK(r13, 64 + 2 * 8);
	CFI_REG_ON_STACK(r14, 64 + 3 * 8);
	CFI_REG_ON_STACK(r15, 64 + 4 * 8);

	init_round_constants();

	transpose4x4_16(RA, RB, RC, RD);
	inpack_dec16(RA, RB, RC, RD);

	movl $14*8, RROUNDd;

	decrypt_round_first16(RC, RD, RA, RB, 8, RROUND);

.align 16
.Loop_dec16:
	decrypt_round16(RA, RB, RC, RD, 0, RROUND);
	decrypt_round16(RC, RD, RA, RB, -8, RROUND);
	subl $16, RROUNDd;
	jnz .Loop_dec16;

	decrypt_round_last16(RA, RB, RC, RD, 0, RROUND);

	outunpack_dec16(RA, RB, RC, RD);
	transpose4x4_16(RA, RB, RC, RD);

	movq (64 + 0 * 8)(%rsp), %rbx;
	movq (64 + 1 * 8)(%rsp), %r12;
	movq (64 + 2 * 8)(%rsp), %r13;
	movq (64 + 3 * 8)(%rsp), %r14;
	movq (64 + 4 * 8)(%rsp), %r15;
	CFI_RESTORE(%rbx);
	CFI_RESTORE(%r12);
	CFI_RESTORE(%r13);
	CFI_RESTORE(%r14);
	CFI_RESTORE(%r15);
	vpxor RT0, RT0, RT0;
	vmovdqa RT0, 0(%rsp);
	vmovdqa RT0, 32(%rsp);
	leave;
	CFI_LEAVE();

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size __twofish_dec_blk16,.-__twofish_dec_blk16;)

.align 16
.globl _gcry_twofish_avx2_blk16
ELF(.type   _gcry_twofish_avx2_blk16,@function;)
_gcry_twofish_avx2_blk16:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%ecx: encrypt
	 */
	CFI_STARTPROC();

	vzeroupper;

	vmovdqu (0 * 32)(%rdx), RA0;
	vmovdqu (1 * 32)(%rdx), RB0;
	vmovdqu (2 * 32)(%rdx), RC0;
	vmovdqu (3 * 32)(%rdx), RD0;
	vmovdqu (4 * 32)(%rdx), RA1;
	vmovdqu (5 * 32)(%rdx), RB1;
	vmovdqu (6 * 32)(%rdx), RC1;
	vmovdqu (7 * 32)(%rdx), RD1;

	testl %ecx, %ecx;
	jz .Lblk16_dec;
		call __twofish_enc_blk16;
		jmp .Lblk16_end;
	.Lblk16_dec:
		call __twofish_dec_blk16;

.Lblk16_end:
	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RB0, (1 * 32)(%rsi);
	vmovdqu RC0, (2 * 32)(%rsi);
	vmovdqu RD0, (3 * 32)(%rsi);
	vmovdqu RA1, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RC1, (6 * 32)(%rsi);
	vmovdqu RD1, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_twofish_avx2_blk16,.-_gcry_twofish_avx2_blk16;)

#define inc_le128(x, minus_one, tmp) \
	vpcmpeqq minus_one, x, tmp; \
	vpsubq minus_one, x, x; \
	vpslldq $8, tmp, tmp; \
	vpsubq tmp, x, x;

.align 16
.globl _gcry_twofish_avx2_ctr_enc
ELF(.type   _gcry_twofish_avx2_ctr_enc,@function;)
_gcry_twofish_avx2_ctr_enc:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv (big endian, 128bit)
	 */
	CFI_STARTPROC();

	movq 8(%rcx), %rax;
	bswapq %rax;

	vzeroupper;

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
	vpshufb RTMP3, RTMP0, RB0;
	vpsubq RTMP2, RTMP0, RTMP0; /* +5 ; +4 */
	vpshufb RTMP3, RTMP0, RC0;
	vpsubq RTMP2, RTMP0, RTMP0; /* +7 ; +6 */
	vpshufb RTMP3, RTMP0, RD0;
	vpsubq RTMP2, RTMP0, RTMP0; /* +9 ; +8 */
	vpshufb RTMP3, RTMP0, RA1;
	vpsubq RTMP2, RTMP0, RTMP0; /* +11 ; +10 */
	vpshufb RTMP3, RTMP0, RB1;
	vpsubq RTMP2, RTMP0, RTMP0; /* +13 ; +12 */
	vpshufb RTMP3, RTMP0, RC1;
	vpsubq RTMP2, RTMP0, RTMP0; /* +15 ; +14 */
	vpshufb RTMP3, RTMP0, RD1;
	vpsubq RTMP2, RTMP0, RTMP0; /* +16 */
	vpshufb RTMP3x, RTMP0x, RTMP0x;

	jmp .Lctr_carry_done;

.Lhandle_ctr_carry:
	/* construct IVs */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RB0; /* +3 ; +2 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RC0; /* +5 ; +4 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RD0; /* +7 ; +6 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RA1; /* +9 ; +8 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RB1; /* +11 ; +10 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RC1; /* +13 ; +12 */
	inc_le128(RTMP0, RNOT, RTMP1);
	inc_le128(RTMP0, RNOT, RTMP1);
	vpshufb RTMP3, RTMP0, RD1; /* +15 ; +14 */
	inc_le128(RTMP0, RNOT, RTMP1);
	vextracti128 $1, RTMP0, RTMP0x;
	vpshufb RTMP3x, RTMP0x, RTMP0x; /* +16 */

.align 4
.Lctr_carry_done:
	/* store new IV */
	vmovdqu RTMP0x, (%rcx);

	call __twofish_enc_blk16;

	vpxor (0 * 32)(%rdx), RA0, RA0;
	vpxor (1 * 32)(%rdx), RB0, RB0;
	vpxor (2 * 32)(%rdx), RC0, RC0;
	vpxor (3 * 32)(%rdx), RD0, RD0;
	vpxor (4 * 32)(%rdx), RA1, RA1;
	vpxor (5 * 32)(%rdx), RB1, RB1;
	vpxor (6 * 32)(%rdx), RC1, RC1;
	vpxor (7 * 32)(%rdx), RD1, RD1;

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RB0, (1 * 32)(%rsi);
	vmovdqu RC0, (2 * 32)(%rsi);
	vmovdqu RD0, (3 * 32)(%rsi);
	vmovdqu RA1, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RC1, (6 * 32)(%rsi);
	vmovdqu RD1, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_twofish_avx2_ctr_enc,.-_gcry_twofish_avx2_ctr_enc;)

.align 16
.globl _gcry_twofish_avx2_cbc_dec
ELF(.type   _gcry_twofish_avx2_cbc_dec,@function;)
_gcry_twofish_avx2_cbc_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();

	vzeroupper;

	vmovdqu (0 * 32)(%rdx), RA0;
	vmovdqu (1 * 32)(%rdx), RB0;
	vmovdqu (2 * 32)(%rdx), RC0;
	vmovdqu (3 * 32)(%rdx), RD0;
	vmovdqu (4 * 32)(%rdx), RA1;
	vmovdqu (5 * 32)(%rdx), RB1;
	vmovdqu (6 * 32)(%rdx), RC1;
	vmovdqu (7 * 32)(%rdx), RD1;

	call __twofish_dec_blk16;

	vmovdqu (%rcx), RNOTx;
	vinserti128 $1, (%rdx), RNOT, RNOT;
	vpxor RNOT, RA0, RA0;
	vpxor (0 * 32 + 16)(%rdx), RB0, RB0;
	vpxor (1 * 32 + 16)(%rdx), RC0, RC0;
	vpxor (2 * 32 + 16)(%rdx), RD0, RD0;
	vpxor (3 * 32 + 16)(%rdx), RA1, RA1;
	vpxor (4 * 32 + 16)(%rdx), RB1, RB1;
	vpxor (5 * 32 + 16)(%rdx), RC1, RC1;
	vpxor (6 * 32 + 16)(%rdx), RD1, RD1;
	vmovdqu (7 * 32 + 16)(%rdx), RNOTx;
	vmovdqu RNOTx, (%rcx); /* store new IV */

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RB0, (1 * 32)(%rsi);
	vmovdqu RC0, (2 * 32)(%rsi);
	vmovdqu RD0, (3 * 32)(%rsi);
	vmovdqu RA1, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RC1, (6 * 32)(%rsi);
	vmovdqu RD1, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_twofish_avx2_cbc_dec,.-_gcry_twofish_avx2_cbc_dec;)

.align 16
.globl _gcry_twofish_avx2_cfb_dec
ELF(.type   _gcry_twofish_avx2_cfb_dec,@function;)
_gcry_twofish_avx2_cfb_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: iv
	 */
	CFI_STARTPROC();

	vzeroupper;

	/* Load input */
	vmovdqu (%rcx), RNOTx;
	vinserti128 $1, (%rdx), RNOT, RA0;
	vmovdqu (0 * 32 + 16)(%rdx), RB0;
	vmovdqu (1 * 32 + 16)(%rdx), RC0;
	vmovdqu (2 * 32 + 16)(%rdx), RD0;
	vmovdqu (3 * 32 + 16)(%rdx), RA1;
	vmovdqu (4 * 32 + 16)(%rdx), RB1;
	vmovdqu (5 * 32 + 16)(%rdx), RC1;
	vmovdqu (6 * 32 + 16)(%rdx), RD1;

	/* Update IV */
	vmovdqu (7 * 32 + 16)(%rdx), RNOTx;
	vmovdqu RNOTx, (%rcx);

	call __twofish_enc_blk16;

	vpxor (0 * 32)(%rdx), RA0, RA0;
	vpxor (1 * 32)(%rdx), RB0, RB0;
	vpxor (2 * 32)(%rdx), RC0, RC0;
	vpxor (3 * 32)(%rdx), RD0, RD0;
	vpxor (4 * 32)(%rdx), RA1, RA1;
	vpxor (5 * 32)(%rdx), RB1, RB1;
	vpxor (6 * 32)(%rdx), RC1, RC1;
	vpxor (7 * 32)(%rdx), RD1, RD1;

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RB0, (1 * 32)(%rsi);
	vmovdqu RC0, (2 * 32)(%rsi);
	vmovdqu RD0, (3 * 32)(%rsi);
	vmovdqu RA1, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RC1, (6 * 32)(%rsi);
	vmovdqu RD1, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_twofish_avx2_cfb_dec,.-_gcry_twofish_avx2_cfb_dec;)

.align 16
.globl _gcry_twofish_avx2_ocb_enc
ELF(.type _gcry_twofish_avx2_ocb_enc,@function;)

_gcry_twofish_avx2_ocb_enc:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();

	vzeroupper;

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
	OCB_INPUT(1, %r12, %r13, RB0);
	movq (4 * 8)(%r9), %r10;
	movq (5 * 8)(%r9), %r11;
	movq (6 * 8)(%r9), %r12;
	movq (7 * 8)(%r9), %r13;
	OCB_INPUT(2, %r10, %r11, RC0);
	OCB_INPUT(3, %r12, %r13, RD0);
	movq (8 * 8)(%r9), %r10;
	movq (9 * 8)(%r9), %r11;
	movq (10 * 8)(%r9), %r12;
	movq (11 * 8)(%r9), %r13;
	OCB_INPUT(4, %r10, %r11, RA1);
	OCB_INPUT(5, %r12, %r13, RB1);
	movq (12 * 8)(%r9), %r10;
	movq (13 * 8)(%r9), %r11;
	movq (14 * 8)(%r9), %r12;
	movq (15 * 8)(%r9), %r13;
	OCB_INPUT(6, %r10, %r11, RC1);
	OCB_INPUT(7, %r12, %r13, RD1);
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

	call __twofish_enc_blk16;

	addq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(-4 * 8);

	vpxor (0 * 32)(%rsi), RA0, RA0;
	vpxor (1 * 32)(%rsi), RB0, RB0;
	vpxor (2 * 32)(%rsi), RC0, RC0;
	vpxor (3 * 32)(%rsi), RD0, RD0;
	vpxor (4 * 32)(%rsi), RA1, RA1;
	vpxor (5 * 32)(%rsi), RB1, RB1;
	vpxor (6 * 32)(%rsi), RC1, RC1;
	vpxor (7 * 32)(%rsi), RD1, RD1;

	vmovdqu RA0, (0 * 32)(%rsi);
	vmovdqu RB0, (1 * 32)(%rsi);
	vmovdqu RC0, (2 * 32)(%rsi);
	vmovdqu RD0, (3 * 32)(%rsi);
	vmovdqu RA1, (4 * 32)(%rsi);
	vmovdqu RB1, (5 * 32)(%rsi);
	vmovdqu RC1, (6 * 32)(%rsi);
	vmovdqu RD1, (7 * 32)(%rsi);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_twofish_avx2_ocb_enc,.-_gcry_twofish_avx2_ocb_enc;)

.align 16
.globl _gcry_twofish_avx2_ocb_dec
ELF(.type _gcry_twofish_avx2_ocb_dec,@function;)

_gcry_twofish_avx2_ocb_dec:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: dst (16 blocks)
	 *	%rdx: src (16 blocks)
	 *	%rcx: offset
	 *	%r8 : checksum
	 *	%r9 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();

	vzeroupper;

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
	OCB_INPUT(1, %r12, %r13, RB0);
	movq (4 * 8)(%r9), %r10;
	movq (5 * 8)(%r9), %r11;
	movq (6 * 8)(%r9), %r12;
	movq (7 * 8)(%r9), %r13;
	OCB_INPUT(2, %r10, %r11, RC0);
	OCB_INPUT(3, %r12, %r13, RD0);
	movq (8 * 8)(%r9), %r10;
	movq (9 * 8)(%r9), %r11;
	movq (10 * 8)(%r9), %r12;
	movq (11 * 8)(%r9), %r13;
	OCB_INPUT(4, %r10, %r11, RA1);
	OCB_INPUT(5, %r12, %r13, RB1);
	movq (12 * 8)(%r9), %r10;
	movq (13 * 8)(%r9), %r11;
	movq (14 * 8)(%r9), %r12;
	movq (15 * 8)(%r9), %r13;
	OCB_INPUT(6, %r10, %r11, RC1);
	OCB_INPUT(7, %r12, %r13, RD1);
#undef OCB_INPUT

	vmovdqu RTMP0x, (%rcx);
	mov %r8, %rcx

	movq (0 * 8)(%rsp), %r10;
	movq (1 * 8)(%rsp), %r11;
	movq (2 * 8)(%rsp), %r12;
	movq (3 * 8)(%rsp), %r13;
	CFI_RESTORE(%r10);
	CFI_RESTORE(%r11);
	CFI_RESTORE(%r12);
	CFI_RESTORE(%r13);

	call __twofish_dec_blk16;

	vmovdqu (%rcx), RTMP1x;

	vpxor (0 * 32)(%rsi), RA0, RA0;
	vpxor (1 * 32)(%rsi), RB0, RB0;
	vpxor (2 * 32)(%rsi), RC0, RC0;
	vpxor (3 * 32)(%rsi), RD0, RD0;
	vpxor (4 * 32)(%rsi), RA1, RA1;
	vpxor (5 * 32)(%rsi), RB1, RB1;
	vpxor (6 * 32)(%rsi), RC1, RC1;
	vpxor (7 * 32)(%rsi), RD1, RD1;

	addq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(-4 * 8);

	/* Checksum_i = Checksum_{i-1} xor P_i  */

	vmovdqu RA0, (0 * 32)(%rsi);
	vpxor RA0, RTMP1, RTMP1;
	vmovdqu RB0, (1 * 32)(%rsi);
	vpxor RB0, RTMP1, RTMP1;
	vmovdqu RC0, (2 * 32)(%rsi);
	vpxor RC0, RTMP1, RTMP1;
	vmovdqu RD0, (3 * 32)(%rsi);
	vpxor RD0, RTMP1, RTMP1;
	vmovdqu RA1, (4 * 32)(%rsi);
	vpxor RA1, RTMP1, RTMP1;
	vmovdqu RB1, (5 * 32)(%rsi);
	vpxor RB1, RTMP1, RTMP1;
	vmovdqu RC1, (6 * 32)(%rsi);
	vpxor RC1, RTMP1, RTMP1;
	vmovdqu RD1, (7 * 32)(%rsi);
	vpxor RD1, RTMP1, RTMP1;

	vextracti128 $1, RTMP1, RNOTx;
	vpxor RNOTx, RTMP1x, RTMP1x;
	vmovdqu RTMP1x, (%rcx);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_twofish_avx2_ocb_dec,.-_gcry_twofish_avx2_ocb_dec;)

.align 16
.globl _gcry_twofish_avx2_ocb_auth
ELF(.type _gcry_twofish_avx2_ocb_auth,@function;)

_gcry_twofish_avx2_ocb_auth:
	/* input:
	 *	%rdi: ctx, CTX
	 *	%rsi: abuf (16 blocks)
	 *	%rdx: offset
	 *	%rcx: checksum
	 *	%r8 : L pointers (void *L[16])
	 */
	CFI_STARTPROC();

	vzeroupper;

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
	OCB_INPUT(1, %r12, %r13, RB0);
	movq (4 * 8)(%r8), %r10;
	movq (5 * 8)(%r8), %r11;
	movq (6 * 8)(%r8), %r12;
	movq (7 * 8)(%r8), %r13;
	OCB_INPUT(2, %r10, %r11, RC0);
	OCB_INPUT(3, %r12, %r13, RD0);
	movq (8 * 8)(%r8), %r10;
	movq (9 * 8)(%r8), %r11;
	movq (10 * 8)(%r8), %r12;
	movq (11 * 8)(%r8), %r13;
	OCB_INPUT(4, %r10, %r11, RA1);
	OCB_INPUT(5, %r12, %r13, RB1);
	movq (12 * 8)(%r8), %r10;
	movq (13 * 8)(%r8), %r11;
	movq (14 * 8)(%r8), %r12;
	movq (15 * 8)(%r8), %r13;
	OCB_INPUT(6, %r10, %r11, RC1);
	OCB_INPUT(7, %r12, %r13, RD1);
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

	call __twofish_enc_blk16;

	vpxor RA0, RB0, RA0;
	vpxor RC0, RD0, RC0;
	vpxor RA1, RB1, RA1;
	vpxor RC1, RD1, RC1;

	vpxor RA0, RC0, RA0;
	vpxor RA1, RC1, RA1;

	addq $(4 * 8), %rsp;
	CFI_ADJUST_CFA_OFFSET(-4 * 8);

	vpxor RA1, RA0, RTMP1;

	vextracti128 $1, RTMP1, RNOTx;
	vpxor (%rcx), RTMP1x, RTMP1x;
	vpxor RNOTx, RTMP1x, RTMP1x;
	vmovdqu RTMP1x, (%rcx);

	vzeroall;

	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_twofish_avx2_ocb_auth,.-_gcry_twofish_avx2_ocb_auth;)

SECTION_RODATA

.align 16

/* For CTR-mode IV byteswap */
ELF(.type _gcry_twofish_bswap128_mask,@object)
_gcry_twofish_bswap128_mask:
.Lbswap128_mask:
	.byte 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
ELF(.size _gcry_twofish_bswap128_mask,.-_gcry_twofish_bswap128_mask;)

#endif /*defined(USE_TWOFISH) && defined(ENABLE_AVX2_SUPPORT)*/
#endif /*__x86_64*/
