/* chacha20-amd64-avx512.S  -  AVX512 implementation of ChaCha20 cipher
 *
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
 * Based on D. J. Bernstein reference implementation at
 * http://cr.yp.to/chacha.html:
 *
 * chacha-regs.c version 20080118
 * D. J. Bernstein
 * Public domain.
 */

#ifdef __x86_64
#include <config.h>
#if defined(HAVE_GCC_INLINE_ASM_AVX512) && \
   (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
    defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))

#include "asm-common-amd64.h"

/* register macros */
#define INPUT %rdi
#define DST   %rsi
#define SRC   %rdx
#define NBLKS %rcx
#define ROUND %eax

/* vector registers */
#define X0 %zmm0
#define X1 %zmm1
#define X2 %zmm2
#define X3 %zmm3
#define X4 %zmm4
#define X5 %zmm5
#define X6 %zmm6
#define X7 %zmm7
#define X8 %zmm8
#define X9 %zmm9
#define X10 %zmm10
#define X11 %zmm11
#define X12 %zmm12
#define X13 %zmm13
#define X14 %zmm14
#define X15 %zmm15
#define X0y %ymm0
#define X1y %ymm1
#define X2y %ymm2
#define X3y %ymm3
#define X4y %ymm4
#define X5y %ymm5
#define X6y %ymm6
#define X7y %ymm7
#define X8y %ymm8
#define X9y %ymm9
#define X10y %ymm10
#define X11y %ymm11
#define X12y %ymm12
#define X13y %ymm13
#define X14y %ymm14
#define X15y %ymm15
#define X0x %xmm0
#define X1x %xmm1
#define X2x %xmm2
#define X3x %xmm3
#define X4x %xmm4
#define X5x %xmm5
#define X6x %xmm6
#define X7x %xmm7
#define X8x %xmm8
#define X9x %xmm9
#define X10x %xmm10
#define X11x %xmm11
#define X12x %xmm12
#define X13x %xmm13
#define X14x %xmm14
#define X15x %xmm15

#define TMP0 %zmm16
#define TMP1 %zmm17
#define TMP0y %ymm16
#define TMP1y %ymm17
#define TMP0x %xmm16
#define TMP1x %xmm17

#define COUNTER_ADD %zmm18
#define COUNTER_ADDy %ymm18
#define COUNTER_ADDx %xmm18

#define X12_SAVE %zmm19
#define X12_SAVEy %ymm19
#define X12_SAVEx %xmm19
#define X13_SAVE %zmm20
#define X13_SAVEy %ymm20
#define X13_SAVEx %xmm20

#define S0 %zmm21
#define S1 %zmm22
#define S2 %zmm23
#define S3 %zmm24
#define S4 %zmm25
#define S5 %zmm26
#define S6 %zmm27
#define S7 %zmm28
#define S8 %zmm29
#define S14 %zmm30
#define S15 %zmm31
#define S0y %ymm21
#define S1y %ymm22
#define S2y %ymm23
#define S3y %ymm24
#define S4y %ymm25
#define S5y %ymm26
#define S6y %ymm27
#define S7y %ymm28
#define S8y %ymm29
#define S14y %ymm30
#define S15y %ymm31
#define S0x %xmm21
#define S1x %xmm22
#define S2x %xmm23
#define S3x %xmm24
#define S4x %xmm25
#define S5x %xmm26
#define S6x %xmm27
#define S7x %xmm28
#define S8x %xmm29
#define S14x %xmm30
#define S15x %xmm31

/**********************************************************************
  helper macros
 **********************************************************************/

/* 4x4 32-bit integer matrix transpose */
#define transpose_4x4(x0,x1,x2,x3,t1,t2) \
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

/* 4x4 128-bit matrix transpose */
#define transpose_16byte_4x4(x0,x1,x2,x3,t1,t2) \
	vshufi32x4 $0xee, x1, x0, t2; \
	vshufi32x4 $0x44, x1, x0, x0; \
	\
	vshufi32x4 $0x44, x3, x2, t1; \
	vshufi32x4 $0xee, x3, x2, x2; \
	\
	vshufi32x4 $0xdd, t1, x0, x1; \
	vshufi32x4 $0x88, t1, x0, x0; \
	\
	vshufi32x4 $0xdd, x2, t2, x3; \
	vshufi32x4 $0x88, x2, t2, x2;

/* 2x2 128-bit matrix transpose */
#define transpose_16byte_2x2(x0,x1,t1) \
	vmovdqa32  x0, t1; \
	vshufi32x4 $0x0, x1, x0, x0; \
	vshufi32x4 $0x3, x1, t1, x1;

#define xor_src_dst_4x4(dst, src, offset, add, x0, x4, x8, x12) \
	vpxord (offset + 0 * (add))(src), x0, x0; \
	vpxord (offset + 1 * (add))(src), x4, x4; \
	vpxord (offset + 2 * (add))(src), x8, x8; \
	vpxord (offset + 3 * (add))(src), x12, x12; \
	vmovdqu32 x0, (offset + 0 * (add))(dst); \
	vmovdqu32 x4, (offset + 1 * (add))(dst); \
	vmovdqu32 x8, (offset + 2 * (add))(dst); \
	vmovdqu32 x12, (offset + 3 * (add))(dst);

#define xor_src_dst(dst, src, offset, xreg) \
	vpxord offset(src), xreg, xreg; \
	vmovdqu32 xreg, offset(dst);

#define clear_vec4(v0,v1,v2,v3) \
	vpxord v0, v0, v0; \
	vpxord v1, v1, v1; \
	vpxord v2, v2, v2; \
	vpxord v3, v3, v3;

#define clear_zmm16_zmm31() \
	clear_vec4(%ymm16, %ymm20, %ymm24, %ymm28); \
	clear_vec4(%ymm17, %ymm21, %ymm25, %ymm29); \
	clear_vec4(%ymm18, %ymm22, %ymm26, %ymm30); \
	clear_vec4(%ymm19, %ymm23, %ymm27, %ymm31);

/**********************************************************************
  16-way (zmm), 8-way (ymm), 4-way (xmm) chacha20
 **********************************************************************/

#define ROTATE2(v1,v2,c)	\
	vprold $(c), v1, v1;	\
	vprold $(c), v2, v2;

#define XOR(ds,s) \
	vpxord s, ds, ds;

#define PLUS(ds,s) \
	vpaddd s, ds, ds;

#define QUARTERROUND2V(a1,b1,c1,d1,a2,b2,c2,d2)			\
	PLUS(a1,b1); PLUS(a2,b2); XOR(d1,a1); XOR(d2,a2);	\
	    ROTATE2(d1, d2, 16);				\
	PLUS(c1,d1); PLUS(c2,d2); XOR(b1,c1); XOR(b2,c2);	\
	    ROTATE2(b1, b2, 12);				\
	PLUS(a1,b1); PLUS(a2,b2); XOR(d1,a1); XOR(d2,a2);	\
	    ROTATE2(d1, d2, 8);					\
	PLUS(c1,d1); PLUS(c2,d2); XOR(b1,c1); XOR(b2,c2);	\
	    ROTATE2(b1, b2, 7);

/**********************************************************************
  1-way/2-way (xmm) chacha20
 **********************************************************************/

#define ROTATE(v1,c)			\
	vprold $(c), v1, v1;		\

#define WORD_SHUF(v1,shuf)		\
	vpshufd $shuf, v1, v1;

#define QUARTERROUND1H(x0,x1,x2,x3,shuf_x1,shuf_x2,shuf_x3) \
	PLUS(x0, x1); XOR(x3, x0); ROTATE(x3, 16); \
	PLUS(x2, x3); XOR(x1, x2); ROTATE(x1, 12); \
	PLUS(x0, x1); XOR(x3, x0); ROTATE(x3, 8); \
	PLUS(x2, x3); \
	  WORD_SHUF(x3, shuf_x3); \
		      XOR(x1, x2); \
	  WORD_SHUF(x2, shuf_x2); \
				   ROTATE(x1, 7); \
	  WORD_SHUF(x1, shuf_x1);

#define QUARTERROUND2H(x0,x1,x2,x3,y0,y1,y2,y3,shuf_x1,shuf_x2,shuf_x3) \
	PLUS(x0, x1); PLUS(y0, y1); XOR(x3, x0); XOR(y3, y0); \
	  ROTATE(x3, 16); ROTATE(y3, 16); \
	PLUS(x2, x3); PLUS(y2, y3); XOR(x1, x2); XOR(y1, y2); \
	  ROTATE(x1, 12); ROTATE(y1, 12); \
	PLUS(x0, x1); PLUS(y0, y1); XOR(x3, x0); XOR(y3, y0); \
	  ROTATE(x3, 8); ROTATE(y3, 8); \
	PLUS(x2, x3); PLUS(y2, y3); \
	  WORD_SHUF(x3, shuf_x3); WORD_SHUF(y3, shuf_x3); \
		      XOR(x1, x2); XOR(y1, y2); \
	  WORD_SHUF(x2, shuf_x2); WORD_SHUF(y2, shuf_x2); \
				   ROTATE(x1, 7); ROTATE(y1, 7); \
	  WORD_SHUF(x1, shuf_x1); WORD_SHUF(y1, shuf_x1);

SECTION_RODATA

.align 64
ELF(.type _gcry_chacha20_amd64_avx512_data,@object;)
_gcry_chacha20_amd64_avx512_data:
.Lcounter_0_1_2_3:
.Lcounter_0_1:
	.long 0,0,0,0
.Lone:
	.long 1,0,0,0
.Lcounter_2_3:
.Ltwo:
	.long 2,0,0,0
.Lthree:
	.long 3,0,0,0
.Linc_counter:
	.byte 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
ELF(.size _gcry_chacha20_amd64_avx512_data,.-_gcry_chacha20_amd64_avx512_data)

.text

.align 16
.globl _gcry_chacha20_amd64_avx512_blocks
ELF(.type _gcry_chacha20_amd64_avx512_blocks,@function;)
_gcry_chacha20_amd64_avx512_blocks:
	/* input:
	 *	%rdi: input
	 *	%rsi: dst
	 *	%rdx: src
	 *	%rcx: nblks
	 */
	CFI_STARTPROC();

	spec_stop_avx512;

	cmpq $4, NBLKS;
	jb .Lskip_vertical_handling;

	/* Load constants */
	vpmovzxbd .Linc_counter rRIP, COUNTER_ADD;
	kxnorq %k1, %k1, %k1;

	cmpq $16, NBLKS;
	jae .Lprocess_16v;

	/* Preload state to YMM registers */
	vpbroadcastd (0 * 4)(INPUT), S0y;
	vpbroadcastd (1 * 4)(INPUT), S1y;
	vpbroadcastd (2 * 4)(INPUT), S2y;
	vpbroadcastd (3 * 4)(INPUT), S3y;
	vpbroadcastd (4 * 4)(INPUT), S4y;
	vpbroadcastd (5 * 4)(INPUT), S5y;
	vpbroadcastd (6 * 4)(INPUT), S6y;
	vpbroadcastd (7 * 4)(INPUT), S7y;
	vpbroadcastd (8 * 4)(INPUT), S8y;
	vpbroadcastd (14 * 4)(INPUT), S14y;
	vpbroadcastd (15 * 4)(INPUT), S15y;
	jmp .Lskip16v;

.align 16
.Lprocess_16v:
	/* Process 16 ChaCha20 blocks */

	/* Preload state to ZMM registers */
	vpbroadcastd (0 * 4)(INPUT), S0;
	vpbroadcastd (1 * 4)(INPUT), S1;
	vpbroadcastd (2 * 4)(INPUT), S2;
	vpbroadcastd (3 * 4)(INPUT), S3;
	vpbroadcastd (4 * 4)(INPUT), S4;
	vpbroadcastd (5 * 4)(INPUT), S5;
	vpbroadcastd (6 * 4)(INPUT), S6;
	vpbroadcastd (7 * 4)(INPUT), S7;
	vpbroadcastd (8 * 4)(INPUT), S8;
	vpbroadcastd (14 * 4)(INPUT), S14;
	vpbroadcastd (15 * 4)(INPUT), S15;

	movl $20, ROUND;
	subq $16, NBLKS;

	/* Construct counter vectors X12 and X13 */
	vpmovm2d %k1, X9;
	vpaddd (12 * 4)(INPUT){1to16}, COUNTER_ADD, X12;
	vpbroadcastd (13 * 4)(INPUT), X13;
	vpcmpud $6, X12, COUNTER_ADD, %k2;
	vpsubd X9, X13, X13{%k2};
	vmovdqa32 X12, X12_SAVE;
	vmovdqa32 X13, X13_SAVE;

	/* Load vectors */
	vmovdqa32 S0, X0;
	vmovdqa32 S4, X4;
	vmovdqa32 S8, X8;
	vmovdqa32 S1, X1;
	vmovdqa32 S5, X5;
	vpbroadcastd (9 * 4)(INPUT), X9;
	QUARTERROUND2V(X0, X4,  X8, X12,   X1, X5,  X9, X13)
	vmovdqa32 S2, X2;
	vmovdqa32 S6, X6;
	vpbroadcastd (10 * 4)(INPUT), X10;
	vmovdqa32 S14, X14;
	vmovdqa32 S3, X3;
	vmovdqa32 S7, X7;
	vpbroadcastd (11 * 4)(INPUT), X11;
	vmovdqa32 S15, X15;

	/* Update counter */
	addq $16, (12 * 4)(INPUT);
	jmp .Lround2_entry_16v;

.align 16
.Loop16v:
	movl $20, ROUND;
	subq $16, NBLKS;

	vmovdqa32 S0, X0;
	vmovdqa32 S4, X4;
	vmovdqa32 S8, X8;
	transpose_16byte_4x4(X1, X5, X9, X13, TMP0, TMP1);
	xor_src_dst_4x4(DST, SRC, (64 * 1), 256, X1, X5, X9, X13);
	vpmovm2d %k1, X9;
	vpaddd (12 * 4)(INPUT){1to16}, COUNTER_ADD, X12;
	vpbroadcastd (13 * 4)(INPUT), X13;
	vpcmpud $6, X12, COUNTER_ADD, %k2;
	vpsubd X9, X13, X13{%k2};
	vmovdqa32 S1, X1;
	vmovdqa32 S5, X5;
	vpbroadcastd (9 * 4)(INPUT), X9;
	vmovdqa32 X12, X12_SAVE;
	vmovdqa32 X13, X13_SAVE;
	QUARTERROUND2V(X0, X4,  X8, X12,   X1, X5,  X9, X13)
	transpose_16byte_4x4(X2, X6, X10, X14, TMP0, TMP1);
	xor_src_dst_4x4(DST, SRC, (64 * 2), 256, X2, X6, X10, X14);
	vmovdqa32 S2, X2;
	vmovdqa32 S6, X6;
	vpbroadcastd (10 * 4)(INPUT), X10;
	vmovdqa32 S14, X14;
	transpose_16byte_4x4(X3, X7, X11, X15, TMP0, TMP1);
	xor_src_dst_4x4(DST, SRC, (64 * 3), 256, X3, X7, X11, X15);
	leaq (16 * 64)(SRC), SRC;
	leaq (16 * 64)(DST), DST;
	vmovdqa32 S3, X3;
	vmovdqa32 S7, X7;
	vpbroadcastd (11 * 4)(INPUT), X11;
	vmovdqa32 S15, X15;

	/* Update counter */
	addq $16, (12 * 4)(INPUT);
	jmp .Lround2_entry_16v;

.align 16
.Lround2_16v:
	QUARTERROUND2V(X2, X7,  X8, X13,   X3, X4,  X9, X14)
	QUARTERROUND2V(X0, X4,  X8, X12,   X1, X5,  X9, X13)
.align 16
.Lround2_entry_16v:
	QUARTERROUND2V(X2, X6, X10, X14,   X3, X7, X11, X15)
	QUARTERROUND2V(X0, X5, X10, X15,   X1, X6, X11, X12)
	subl $2, ROUND;
	jnz .Lround2_16v;

	PLUS(X0, S0);
	PLUS(X1, S1);
	QUARTERROUND2V(X2, X7,  X8, X13,   X3, X4,  X9, X14)
	PLUS(X2, S2);
	PLUS(X3, S3);
	transpose_4x4(X0, X1, X2, X3, TMP0, TMP1);
	PLUS(X4, S4);
	PLUS(X5, S5);
	PLUS(X6, S6);
	PLUS(X7, S7);
	transpose_4x4(X4, X5, X6, X7, TMP0, TMP1);
	PLUS(X8, S8);
	PLUS(X9, (9 * 4)(INPUT){1to16});
	PLUS(X10, (10 * 4)(INPUT){1to16});
	PLUS(X11, (11 * 4)(INPUT){1to16});
	transpose_4x4(X8, X9, X10, X11, TMP0, TMP1);
	PLUS(X12, X12_SAVE);
	PLUS(X13, X13_SAVE);
	PLUS(X14, S14);
	PLUS(X15, S15);
	transpose_4x4(X12, X13, X14, X15, TMP0, TMP1);

	transpose_16byte_4x4(X0, X4, X8, X12, TMP0, TMP1);
	xor_src_dst_4x4(DST, SRC, (64 * 0), 256, X0, X4, X8, X12);

	cmpq $16, NBLKS;
	jae .Loop16v;

	transpose_16byte_4x4(X1, X5, X9, X13, TMP0, TMP1);
	xor_src_dst_4x4(DST, SRC, (64 * 1), 256, X1, X5, X9, X13);
	transpose_16byte_4x4(X2, X6, X10, X14, TMP0, TMP1);
	xor_src_dst_4x4(DST, SRC, (64 * 2), 256, X2, X6, X10, X14);
	transpose_16byte_4x4(X3, X7, X11, X15, TMP0, TMP1);
	xor_src_dst_4x4(DST, SRC, (64 * 3), 256, X3, X7, X11, X15);

	leaq (16 * 64)(SRC), SRC;
	leaq (16 * 64)(DST), DST;

.align 16
.Lskip16v:
	cmpq $8, NBLKS;
	jb .Lskip8v;

	/* Process 8 ChaCha20 blocks */

	/* Construct counter vectors X12 and X13 */
	vpmovm2d %k1, X9y;
	vpaddd (12 * 4)(INPUT){1to8}, COUNTER_ADDy, X12y;
	vpbroadcastd (13 * 4)(INPUT), X13y;
	vpcmpud $6, X12y, COUNTER_ADDy, %k2;
	vpsubd X9y, X13y, X13y{%k2};
	vmovdqa32 X12y, X12_SAVEy;
	vmovdqa32 X13y, X13_SAVEy;

	/* Load vectors */
	vmovdqa32 S0y, X0y;
	vmovdqa32 S4y, X4y;
	vmovdqa32 S8y, X8y;
	vmovdqa32 S1y, X1y;
	vmovdqa32 S5y, X5y;
	vpbroadcastd (9 * 4)(INPUT), X9y;
	vmovdqa32 S2y, X2y;
	vmovdqa32 S6y, X6y;
	vpbroadcastd (10 * 4)(INPUT), X10y;
	vmovdqa32 S14y, X14y;
	vmovdqa32 S3y, X3y;
	vmovdqa32 S7y, X7y;
	vpbroadcastd (11 * 4)(INPUT), X11y;
	vmovdqa32 S15y, X15y;

	/* Update counter */
	addq $8, (12 * 4)(INPUT);

	movl $20, ROUND;
	subq $8, NBLKS;
.align 16
.Lround2_8v:
	QUARTERROUND2V(X0y, X4y,  X8y, X12y,   X1y, X5y,  X9y, X13y)
	QUARTERROUND2V(X2y, X6y, X10y, X14y,   X3y, X7y, X11y, X15y)
	QUARTERROUND2V(X0y, X5y, X10y, X15y,   X1y, X6y, X11y, X12y)
	QUARTERROUND2V(X2y, X7y,  X8y, X13y,   X3y, X4y,  X9y, X14y)
	subl $2, ROUND;
	jnz .Lround2_8v;

	PLUS(X0y, S0y);
	PLUS(X1y, S1y);
	PLUS(X2y, S2y);
	PLUS(X3y, S3y);
	transpose_4x4(X0y, X1y, X2y, X3y, TMP0y, TMP1y);
	PLUS(X4y, S4y);
	PLUS(X5y, S5y);
	PLUS(X6y, S6y);
	PLUS(X7y, S7y);
	transpose_4x4(X4y, X5y, X6y, X7y, TMP0y, TMP1y);
	PLUS(X8y, S8y);
	transpose_16byte_2x2(X0y, X4y, TMP0y);
	PLUS(X9y, (9 * 4)(INPUT){1to8});
	transpose_16byte_2x2(X1y, X5y, TMP0y);
	PLUS(X10y, (10 * 4)(INPUT){1to8});
	transpose_16byte_2x2(X2y, X6y, TMP0y);
	PLUS(X11y, (11 * 4)(INPUT){1to8});
	transpose_16byte_2x2(X3y, X7y, TMP0y);
	xor_src_dst_4x4(DST, SRC, (16 * 0),  64, X0y, X1y, X2y, X3y);
	transpose_4x4(X8y, X9y, X10y, X11y, TMP0y, TMP1y);
	PLUS(X12y, X12_SAVEy);
	PLUS(X13y, X13_SAVEy);
	PLUS(X14y, S14y);
	PLUS(X15y, S15y);
	xor_src_dst_4x4(DST, SRC, (16 * 16), 64, X4y, X5y, X6y, X7y);
	transpose_4x4(X12y, X13y, X14y, X15y, TMP0y, TMP1y);
	transpose_16byte_2x2(X8y, X12y, TMP0y);
	transpose_16byte_2x2(X9y, X13y, TMP0y);
	transpose_16byte_2x2(X10y, X14y, TMP0y);
	transpose_16byte_2x2(X11y, X15y, TMP0y);
	xor_src_dst_4x4(DST, SRC, (16 * 2),  64, X8y, X9y, X10y, X11y);
	xor_src_dst_4x4(DST, SRC, (16 * 18), 64, X12y, X13y, X14y, X15y);

	leaq (8 * 64)(SRC), SRC;
	leaq (8 * 64)(DST), DST;

.align 16
.Lskip8v:
	cmpq $4, NBLKS;
	jb .Lskip4v;

	/* Process 4 ChaCha20 blocks */

	/* Construct counter vectors X12 and X13 */
	vpmovm2d %k1, X9x;
	vpaddd (12 * 4)(INPUT){1to4}, COUNTER_ADDx, X12x;
	vpbroadcastd (13 * 4)(INPUT), X13x;
	vpcmpud $6, X12x, COUNTER_ADDx, %k2;
	vpsubd X9x, X13x, X13x{%k2};
	vmovdqa32 X12x, X12_SAVEx;
	vmovdqa32 X13x, X13_SAVEx;

	/* Load vectors */
	vmovdqa32 S0x, X0x;
	vmovdqa32 S4x, X4x;
	vmovdqa32 S8x, X8x;
	vmovdqa32 S1x, X1x;
	vmovdqa32 S5x, X5x;
	vpbroadcastd (9 * 4)(INPUT), X9x;
	vmovdqa32 S2x, X2x;
	vmovdqa32 S6x, X6x;
	vpbroadcastd (10 * 4)(INPUT), X10x;
	vmovdqa32 S14x, X14x;
	vmovdqa32 S3x, X3x;
	vmovdqa32 S7x, X7x;
	vpbroadcastd (11 * 4)(INPUT), X11x;
	vmovdqa32 S15x, X15x;

	/* Update counter */
	addq $4, (12 * 4)(INPUT);

	movl $20, ROUND;
	subq $4, NBLKS;
.align 16
.Lround2_4v:
	QUARTERROUND2V(X0x, X4x,  X8x, X12x,   X1x, X5x,  X9x, X13x)
	QUARTERROUND2V(X2x, X6x, X10x, X14x,   X3x, X7x, X11x, X15x)
	QUARTERROUND2V(X0x, X5x, X10x, X15x,   X1x, X6x, X11x, X12x)
	QUARTERROUND2V(X2x, X7x,  X8x, X13x,   X3x, X4x,  X9x, X14x)
	subl $2, ROUND;
	jnz .Lround2_4v;

	PLUS(X0x, S0x);
	PLUS(X1x, S1x);
	PLUS(X2x, S2x);
	PLUS(X3x, S3x);
	transpose_4x4(X0x, X1x, X2x, X3x, TMP0x, TMP1x);
	PLUS(X4x, S4x);
	PLUS(X5x, S5x);
	PLUS(X6x, S6x);
	PLUS(X7x, S7x);
	xor_src_dst_4x4(DST, SRC, (16 * 0), 64, X0x, X1x, X2x, X3x);
	transpose_4x4(X4x, X5x, X6x, X7x, TMP0x, TMP1x);
	PLUS(X8x, S8x);
	PLUS(X9x, (9 * 4)(INPUT){1to4});
	PLUS(X10x, (10 * 4)(INPUT){1to4});
	PLUS(X11x, (11 * 4)(INPUT){1to4});
	xor_src_dst_4x4(DST, SRC, (16 * 1), 64, X4x, X5x, X6x, X7x);
	transpose_4x4(X8x, X9x, X10x, X11x, TMP0x, TMP1x);
	PLUS(X12x, X12_SAVEx);
	PLUS(X13x, X13_SAVEx);
	PLUS(X14x, S14x);
	PLUS(X15x, S15x);
	xor_src_dst_4x4(DST, SRC, (16 * 2), 64, X8x, X9x, X10x, X11x);
	transpose_4x4(X12x, X13x, X14x, X15x, TMP0x, TMP1x);
	xor_src_dst_4x4(DST, SRC, (16 * 3), 64, X12x, X13x, X14x, X15x);

	leaq (4 * 64)(SRC), SRC;
	leaq (4 * 64)(DST), DST;

.align 16
.Lskip4v:
	/* clear AVX512 registers */
	kxorq %k2, %k2, %k2;
	vzeroupper;
	clear_zmm16_zmm31();

.align 16
.Lskip_vertical_handling:
	cmpq $0, NBLKS;
	je .Ldone;

	/* Load state */
	vmovdqu (0 * 4)(INPUT), X10x;
	vmovdqu (4 * 4)(INPUT), X11x;
	vmovdqu (8 * 4)(INPUT), X12x;
	vmovdqu (12 * 4)(INPUT), X13x;

	/* Load constant */
	vmovdqa .Lone rRIP, X4x;

	cmpq $1, NBLKS;
	je .Lhandle1;

	/* Process two ChaCha20 blocks (XMM) */
	movl $20, ROUND;
	subq $2, NBLKS;

	vmovdqa X10x, X0x;
	vmovdqa X11x, X1x;
	vmovdqa X12x, X2x;
	vmovdqa X13x, X3x;

	vmovdqa X10x, X8x;
	vmovdqa X11x, X9x;
	vmovdqa X12x, X14x;
	vpaddq X4x, X13x, X15x;
	vmovdqa X15x, X7x;

.align 16
.Lround2_2:
	QUARTERROUND2H(X0x, X1x, X2x,  X3x,  X8x, X9x, X14x, X15x,
		       0x39, 0x4e, 0x93);
	QUARTERROUND2H(X0x, X1x, X2x,  X3x,  X8x, X9x, X14x, X15x,
		       0x93, 0x4e, 0x39);
	subl $2, ROUND;
	jnz .Lround2_2;

	PLUS(X0x, X10x);
	PLUS(X1x, X11x);
	PLUS(X2x, X12x);
	PLUS(X3x, X13x);

	vpaddq .Ltwo rRIP, X13x, X13x; /* Update counter */

	xor_src_dst_4x4(DST, SRC, 0 * 4, 4 * 4, X0x, X1x, X2x, X3x);

	PLUS(X8x, X10x);
	PLUS(X9x, X11x);
	PLUS(X14x, X12x);
	PLUS(X15x, X7x);

	xor_src_dst_4x4(DST, SRC, 16 * 4, 4 * 4, X8x, X9x, X14x, X15x);
	lea (2 * 64)(DST), DST;
	lea (2 * 64)(SRC), SRC;

	cmpq $0, NBLKS;
	je .Lskip1;

.align 16
.Lhandle1:
	/* Process one ChaCha20 block (XMM) */
	movl $20, ROUND;
	subq $1, NBLKS;

	vmovdqa X10x, X0x;
	vmovdqa X11x, X1x;
	vmovdqa X12x, X2x;
	vmovdqa X13x, X3x;

.align 16
.Lround2_1:
	QUARTERROUND1H(X0x, X1x, X2x, X3x, 0x39, 0x4e, 0x93);
	QUARTERROUND1H(X0x, X1x, X2x, X3x, 0x93, 0x4e, 0x39);
	subl $2, ROUND;
	jnz .Lround2_1;

	PLUS(X0x, X10x);
	PLUS(X1x, X11x);
	PLUS(X2x, X12x);
	PLUS(X3x, X13x);

	vpaddq X4x, X13x, X13x; /* Update counter */

	xor_src_dst_4x4(DST, SRC, 0 * 4, 4 * 4, X0x, X1x, X2x, X3x);

.align 16
.Lskip1:
	/* Store counter */
	vmovdqu X13x, (12 * 4)(INPUT);

.align 16
.Ldone:
	vzeroall; /* clears ZMM0-ZMM15 */

	xorl %eax, %eax;
	ret_spec_stop;
	CFI_ENDPROC();
ELF(.size _gcry_chacha20_amd64_avx512_blocks,
	  .-_gcry_chacha20_amd64_avx512_blocks;)

#endif /*defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS)*/
#endif /*__x86_64*/
