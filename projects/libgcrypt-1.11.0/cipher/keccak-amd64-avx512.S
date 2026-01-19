/* keccak-amd64-avx512.S  -  x86-64 AVX512 implementation of Keccak
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
 *
 * ---
 *
 * Core function `KeccakF1600_ce` based on ARMv8-CE KeccakF1600 implementation
 * by Andy Polyakov from CRYPTOGAMS distribution `arm/keccak1600-armv8.pl`.
 * `KeccakF1600_ce` was ported to x86-64 AVX512 and converted to use GCC
 * preprocessed assembly and fitted with new absorb function optimized for
 * x86-64. SHA3-256 performance on Intel tigerlake, 5.72 cpB.
 *
 * Original copyright license follows:
 *
 *  Copyright (c) 2006, CRYPTOGAMS by <appro@openssl.org>
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *        * Redistributions of source code must retain copyright notices,
 *          this list of conditions and the following disclaimer.
 *
 *        * Redistributions in binary form must reproduce the above
 *          copyright notice, this list of conditions and the following
 *          disclaimer in the documentation and/or other materials
 *          provided with the distribution.
 *
 *        * Neither the name of the CRYPTOGAMS nor the names of its
 *          copyright holder and contributors may be used to endorse or
 *          promote products derived from this software without specific
 *          prior written permission.
 *
 *  ALTERNATIVELY, provided that this notice is retained in full, this
 *  product may be distributed under the terms of the GNU General Public
 *  License (GPL), in which case the provisions of the GPL apply INSTEAD OF
 *  those given above.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef __x86_64
#include <config.h>
#if defined(HAVE_GCC_INLINE_ASM_AVX512) && \
   (defined(HAVE_COMPATIBLE_GCC_AMD64_PLATFORM_AS) || \
    defined(HAVE_COMPATIBLE_GCC_WIN64_PLATFORM_AS))

#include "asm-common-amd64.h"

.text

/* Register macros. */
#define A_0_0 %xmm31
#define A_0_1 %xmm30
#define A_0_2 %xmm29
#define A_0_3 %xmm28
#define A_0_4 %xmm27
#define A_1_0 %xmm26
#define A_1_1 %xmm25
#define A_1_2 %xmm24
#define A_1_3 %xmm23
#define A_1_4 %xmm22
#define A_2_0 %xmm21
#define A_2_1 %xmm20
#define A_2_2 %xmm19
#define A_2_3 %xmm18
#define A_2_4 %xmm17
#define A_3_0 %xmm16
#define A_3_1 %xmm15
#define A_3_2 %xmm14
#define A_3_3 %xmm13
#define A_3_4 %xmm12
#define A_4_0 %xmm11
#define A_4_1 %xmm10
#define A_4_2 %xmm9
#define A_4_3 %xmm8
#define A_4_4 %xmm7

#define C_0 %xmm6
#define C_1 %xmm5
#define C_2 %xmm4
#define C_3 %xmm3
#define C_4 %xmm2
#define C_5 %xmm1
#define C_6 %xmm0

#define D_0 C_4
#define D_1 C_5
#define D_2 C_6
#define D_3 C_2
#define D_4 C_3

/* Helper macros for ARMv8-CE to x86-64/AVX512 conversion. */
#define eor3_d(dst_s1, s2, s3) \
	vpternlogq $0x96, s3, s2, dst_s1;

#define eor3(dst, s1, s2, s3) \
	vmovdqa s1, dst; \
	eor3_d(dst, s2, s3);

#define rax1_c(dst, s1, s2_rol1) \
	vprolq $1, s2_rol1, dst; \
	vpxor s1, dst, dst;

#define rax1_t(dst_s1, s2_rol1, tmp) \
	vprolq $1, s2_rol1, tmp; \
	vpxor tmp, dst_s1, dst_s1;

#define rax1_s(dst_s1, s2_rol1) \
	vprolq $1, s2_rol1, s2_rol1; \
	vpxor s2_rol1, dst_s1, dst_s1;

#define xar(dst, s1, s2, rol) \
	vpxorq s2, s1, dst; \
	vprolq $(rol), dst, dst;

#define xar_x(dst, s1, s2, rol) \
	vpxor s2, s1, dst; \
	vprolq $(rol), dst, dst;

#define bcax_d(dst_s1, s2, s3) \
	vpternlogq $0xb4, s3, s2, dst_s1;

#define bcax(dst, s1, s2, s3) \
	vmovdqa64 s1, dst; \
	bcax_d(dst, s2, s3);

#define bcax_x(dst, s1, s2, s3) \
	vmovdqa s1, dst; \
	bcax_d(dst, s2, s3);

#define eor(dst, s1, s2) \
	vpxorq s2, s1, dst;

/* Misc helper macros. */
#define clear_avx512_4regs(a, b, c, d) \
	eor(a, a, a); eor(b, b, b); eor(c, c, c); eor(d, d, d);

#define clear_regs() \
	vzeroall; /* xmm0-xmm15 */ \
	clear_avx512_4regs(%ymm16, %ymm17, %ymm18, %ymm19); \
	clear_avx512_4regs(%ymm20, %ymm21, %ymm22, %ymm23); \
	clear_avx512_4regs(%ymm24, %ymm25, %ymm26, %ymm27); \
	clear_avx512_4regs(%ymm28, %ymm29, %ymm30, %ymm31);

ELF(.type	KeccakF1600_ce,@function)
.align	64, 0xcc
KeccakF1600_ce:
.Loop_ce:
	CFI_STARTPROC()

	////////////////////////////////////////////////// Theta
	eor3(	C_0, A_4_0, A_3_0, A_2_0)
	eor3(	C_1, A_4_1, A_3_1, A_2_1)
	eor3(	C_3, A_4_3, A_3_3, A_2_3)
	eor3(	C_2, A_4_2, A_3_2, A_2_2)
	eor3(	C_4, A_4_4, A_3_4, A_2_4)
	eor3_d(	C_0, A_1_0, A_0_0)
	eor3_d(	C_1, A_1_1, A_0_1)
	eor3_d(	C_3, A_1_3, A_0_3)
	eor3_d(	C_2, A_1_2, A_0_2)
	eor3_d(	C_4, A_1_4, A_0_4)

	rax1_c(	C_5, C_0, C_2)			// D[1]
	rax1_t(	C_2, C_4, C_6)			// D[3]
	rax1_c(	C_6, C_1, C_3)			// D[2]
	rax1_s(	C_3, C_0)			// D[4]
	rax1_s(	C_4, C_1)			// D[0]

	////////////////////////////////////////////////// Theta+Rho+Pi
	xar(	C_0, A_0_1, D_1, 1)		// C[0]=A[2][0]

	xar(	A_0_1, A_1_1, D_1, 44)
	xar(	A_1_1, A_1_4, D_4, 20)
	xar(	A_1_4, A_4_2, D_2, 61)
	xar(	A_4_2, A_2_4, D_4, 39)
	xar(	A_2_4, A_4_0, D_0, 18)

	xar(	C_1, A_0_2, D_2, 62)		// C[1]=A[4][0]

	xar(	A_0_2, A_2_2, D_2, 43)
	xar(	A_2_2, A_2_3, D_3, 25)
	xar(	A_2_3, A_3_4, D_4, 8)
	xar_x(	A_3_4, A_4_3, D_3, 56)
	xar(	A_4_3, A_3_0, D_0, 41)

	xar(	A_3_0, A_0_4, D_4, 27)

	xar_x(	D_4, A_4_4, D_4, 14)		// D[4]=A[0][4]
	xar_x(	A_4_4, A_4_1, D_1, 2)
	xar(	A_1_3, A_1_3, D_3, 55)		// A[1][3]=A[4][1]
	xar(	A_0_4, A_3_1, D_1, 45)		// A[0][4]=A[1][3]
	xar(	A_3_1, A_1_0, D_0, 36)

	xar(	A_1_0, A_0_3, D_3, 28)

	eor(	A_0_0, A_0_0, D_0)

	xar_x(	D_3, A_3_3, D_3, 21)		// D[3]=A[0][3]
	xar(	A_0_3, A_3_2, D_2, 15)		// A[0][3]=A[3][3]
	xar(	D_1, A_2_1, D_1, 10)		// D[1]=A[3][2]
	xar(	D_2, A_1_2, D_2, 6)		// D[2]=A[2][1]
	xar(	D_0, A_2_0, D_0, 3)		// D[0]=A[1][2]

	////////////////////////////////////////////////// Chi+Iota
	bcax_x(	A_4_0, C_1, A_4_2, A_1_3)	// A[1][3]=A[4][1]
	bcax(	A_4_1, A_1_3, A_4_3, A_4_2)	// A[1][3]=A[4][1]
	bcax_d(	A_4_2, A_4_4, A_4_3)
	bcax_d(	A_4_3, C_1, A_4_4)
	bcax_d(	A_4_4, A_1_3, C_1)		// A[1][3]=A[4][1]

	bcax_x(	A_3_2, D_1, A_3_4, A_0_3)	// A[0][3]=A[3][3]
	bcax(	A_3_3, A_0_3, A_3_0, A_3_4)	// A[0][3]=A[3][3]
	bcax_d(	A_3_4, A_3_1, A_3_0)
	bcax_d(	A_3_0, D_1, A_3_1)
	bcax_d(	A_3_1, A_0_3, D_1)		// A[0][3]=A[3][3]

	bcax(	A_2_0, C_0, A_2_2, D_2)
	bcax(	A_2_1, D_2, A_2_3, A_2_2)
	bcax_d(	A_2_2, A_2_4, A_2_3)
	bcax_d(	A_2_3, C_0, A_2_4)
	bcax_d(	A_2_4, D_2, C_0)

	bcax(	A_1_2, D_0, A_1_4, A_0_4)	// A[0][4]=A[1][3]
	bcax(	A_1_3, A_0_4, A_1_0, A_1_4)	// A[0][4]=A[1][3]
	bcax_d(	A_1_4, A_1_1, A_1_0)
	bcax_d(	A_1_0, D_0, A_1_1)
	bcax_d(	A_1_1, A_0_4, D_0)		// A[0][4]=A[1][3]

	bcax(	A_0_3, D_3, A_0_0, D_4)
	bcax(	A_0_4, D_4, A_0_1, A_0_0)
	bcax_d(	A_0_0, A_0_2, A_0_1)
	bcax_d(	A_0_1, D_3, A_0_2)
	bcax_d(	A_0_2, D_4, D_3)
	eor(	A_0_0, A_0_0, (%r10))

	cmpq	%r10, %r11
	je	.Lend_ce

	addq	$8, %r10
	jmp	.Loop_ce

.align	64, 0xcc
.Lend_ce:
	ret_spec_stop
	CFI_ENDPROC()
ELF(.size	KeccakF1600_ce,.-KeccakF1600_ce)

.globl		_gcry_keccak_f1600_state_permute64_avx512
ELF(.type	_gcry_keccak_f1600_state_permute64_avx512,@function)
.align	64, 0xcc
_gcry_keccak_f1600_state_permute64_avx512:
	/* input:
	 *	%rdi: state
	 *	%rsi: round constants
	 */
	CFI_STARTPROC()

	spec_stop_avx512;

	leaq		12*8(%rdi), %rax
	leaq		(24-1)*8(%rsi), %r11

	vmovdqu64	0*8(%rdi), A_0_0
	vmovdqu64	1*8(%rdi), A_0_1
	vmovdqu64	2*8(%rdi), A_0_2
	vmovdqu64	3*8(%rdi), A_0_3
	vmovdqu64	4*8(%rdi), A_0_4
	vmovdqu64	5*8(%rdi), A_1_0
	vmovdqu64	6*8(%rdi), A_1_1
	vmovdqu64	7*8(%rdi), A_1_2
	vmovdqu64	8*8(%rdi), A_1_3
	vmovdqu64	9*8(%rdi), A_1_4
	vmovdqu64	10*8(%rdi), A_2_0
	vmovdqu64	11*8(%rdi), A_2_1
	vmovdqu64	0*8(%rax), A_2_2
	vmovdqu64	1*8(%rax), A_2_3
	vmovdqu64	2*8(%rax), A_2_4
	vmovdqu64	3*8(%rax), A_3_0
	vmovdqu		4*8(%rax), A_3_1
	vmovdqu		5*8(%rax), A_3_2
	vmovdqu		6*8(%rax), A_3_3
	vmovdqu		7*8(%rax), A_3_4
	vmovdqu		8*8(%rax), A_4_0
	vmovdqu		9*8(%rax), A_4_1
	vmovdqu		10*8(%rax), A_4_2
	vmovdqu		11*8(%rax), A_4_3
	vmovq		12*8(%rax), A_4_4

	movq		%rsi, %r10
	call		KeccakF1600_ce

	vpunpcklqdq	A_0_1, A_0_0, A_0_0
	vpunpcklqdq	A_0_3, A_0_2, A_0_2
	vpunpcklqdq	A_1_0, A_0_4, A_0_4
	vpunpcklqdq	A_1_2, A_1_1, A_1_1
	vpunpcklqdq	A_1_4, A_1_3, A_1_3
	vpunpcklqdq	A_2_1, A_2_0, A_2_0
	vpunpcklqdq	A_2_3, A_2_2, A_2_2
	vpunpcklqdq	A_3_0, A_2_4, A_2_4
	vpunpcklqdq	A_3_2, A_3_1, A_3_1
	vpunpcklqdq	A_3_4, A_3_3, A_3_3
	vpunpcklqdq	A_4_1, A_4_0, A_4_0
	vpunpcklqdq	A_4_3, A_4_2, A_4_2
	vmovdqu64	A_0_0, 0*8(%rdi)
	vmovdqu64	A_0_2, 2*8(%rdi)
	vmovdqu64	A_0_4, 4*8(%rdi)
	vmovdqu64	A_1_1, 6*8(%rdi)
	vmovdqu64	A_1_3, 8*8(%rdi)
	vmovdqu64	A_2_0, 10*8(%rdi)
	vmovdqu64	A_2_2, 0*8(%rax)
	vmovdqu64	A_2_4, 2*8(%rax)
	vmovdqu		A_3_1, 4*8(%rax)
	vmovdqu		A_3_3, 6*8(%rax)
	vmovdqu		A_4_0, 8*8(%rax)
	vmovdqu		A_4_2, 10*8(%rax)
	vmovq		A_4_4, 12*8(%rax)

	xorl		%eax, %eax

	clear_regs()
	ret_spec_stop
	CFI_ENDPROC()
ELF(.size	_gcry_keccak_f1600_state_permute64_avx512,
		.-_gcry_keccak_f1600_state_permute64_avx512)

.globl		_gcry_keccak_absorb_blocks_avx512
ELF(.type	_gcry_keccak_absorb_blocks_avx512,@function)
.align	64, 0xcc
_gcry_keccak_absorb_blocks_avx512:
	/* input:
	 *	%rdi: state
	 *	%rsi: round constants
	 *	%rdx: lanes
	 *	%rcx: nlanes
	 *	%r8 : blocklanes
	 *	%r9 : lanes output pointer
	 */
	CFI_STARTPROC()

	spec_stop_avx512;

	leaq		12*8(%rdi), %rax
	leaq		(24-1)*8(%rsi), %r11

	vmovdqu64	0*8(%rdi), A_0_0
	vmovdqu64	1*8(%rdi), A_0_1
	vmovdqu64	2*8(%rdi), A_0_2
	vmovdqu64	3*8(%rdi), A_0_3
	vmovdqu64	4*8(%rdi), A_0_4
	vmovdqu64	5*8(%rdi), A_1_0
	vmovdqu64	6*8(%rdi), A_1_1
	vmovdqu64	7*8(%rdi), A_1_2
	vmovdqu64	8*8(%rdi), A_1_3
	vmovdqu64	9*8(%rdi), A_1_4
	vmovdqu64	10*8(%rdi), A_2_0
	vmovdqu64	11*8(%rdi), A_2_1
	vmovdqu64	0*8(%rax), A_2_2
	vmovdqu64	1*8(%rax), A_2_3
	vmovdqu64	2*8(%rax), A_2_4
	vmovdqu64	3*8(%rax), A_3_0
	vmovdqu		4*8(%rax), A_3_1
	vmovdqu		5*8(%rax), A_3_2
	vmovdqu		6*8(%rax), A_3_3
	vmovdqu		7*8(%rax), A_3_4
	vmovdqu		8*8(%rax), A_4_0
	vmovdqu		9*8(%rax), A_4_1
	vmovdqu		10*8(%rax), A_4_2
	vmovdqu		11*8(%rax), A_4_3
	vmovq		12*8(%rax), A_4_4

	cmpq		$(104 >> 3), %r8
	jb		.Loop_absorb_72_ce
	je		.Loop_absorb_104_ce
	cmpq		$(144 >> 3), %r8
	jb		.Loop_absorb_136_ce
	je		.Loop_absorb_144_ce
	jmp		.Loop_absorb_168_ce

.align	64, 0xcc
.Loop_absorb_168_ce:
	subq		%r8, %rcx	// len - bsz
	jb		.Labsorbed_ce

	vpxorq		0*8(%rdx), A_0_0, A_0_0
	vpxorq		1*8(%rdx), A_0_1, A_0_1
	vpxorq		2*8(%rdx), A_0_2, A_0_2
	vpxorq		3*8(%rdx), A_0_3, A_0_3
	vpxorq		4*8(%rdx), A_0_4, A_0_4
	vpxorq		5*8(%rdx), A_1_0, A_1_0
	vpxorq		6*8(%rdx), A_1_1, A_1_1
	vpxorq		7*8(%rdx), A_1_2, A_1_2
	vpxorq		8*8(%rdx), A_1_3, A_1_3
	vpxorq		9*8(%rdx), A_1_4, A_1_4
	vpxorq		10*8(%rdx), A_2_0, A_2_0
	vpxorq		11*8(%rdx), A_2_1, A_2_1
	vpxorq		12*8(%rdx), A_2_2, A_2_2
	vpxorq		13*8(%rdx), A_2_3, A_2_3
	vpxorq		14*8(%rdx), A_2_4, A_2_4
	vpxorq		15*8(%rdx), A_3_0, A_3_0
	vpxor		16*8(%rdx), A_3_1, A_3_1
	vpxor		17*8(%rdx), A_3_2, A_3_2
	vpxor		18*8(%rdx), A_3_3, A_3_3
	vpxor		19*8(%rdx), A_3_4, A_3_4
	vmovq		20*8(%rdx), C_0
	leaq		21*8(%rdx), %rdx
	vpxorq		C_0, A_4_0, A_4_0

	movq		%rsi, %r10
	call		KeccakF1600_ce

	jmp		.Loop_absorb_168_ce

.align	64, 0xcc
.Loop_absorb_144_ce:
	subq		%r8, %rcx	// len - bsz
	jb		.Labsorbed_ce

	vpxorq		0*8(%rdx), A_0_0, A_0_0
	vpxorq		1*8(%rdx), A_0_1, A_0_1
	vpxorq		2*8(%rdx), A_0_2, A_0_2
	vpxorq		3*8(%rdx), A_0_3, A_0_3
	vpxorq		4*8(%rdx), A_0_4, A_0_4
	vpxorq		5*8(%rdx), A_1_0, A_1_0
	vpxorq		6*8(%rdx), A_1_1, A_1_1
	vpxorq		7*8(%rdx), A_1_2, A_1_2
	vpxorq		8*8(%rdx), A_1_3, A_1_3
	vpxorq		9*8(%rdx), A_1_4, A_1_4
	vpxorq		10*8(%rdx), A_2_0, A_2_0
	vpxorq		11*8(%rdx), A_2_1, A_2_1
	vpxorq		12*8(%rdx), A_2_2, A_2_2
	vpxorq		13*8(%rdx), A_2_3, A_2_3
	vpxorq		14*8(%rdx), A_2_4, A_2_4
	vpxorq		15*8(%rdx), A_3_0, A_3_0
	vpxor		16*8(%rdx), A_3_1, A_3_1
	vmovq		17*8(%rdx), C_0
	leaq		18*8(%rdx), %rdx
	vpxor		C_0, A_3_2, A_3_2

	movq		%rsi, %r10
	call		KeccakF1600_ce

	jmp		.Loop_absorb_144_ce

.align	64, 0xcc
.Loop_absorb_136_ce:
	subq		%r8, %rcx	// len - bsz
	jb		.Labsorbed_ce

	vpxorq		0*8(%rdx), A_0_0, A_0_0
	vpxorq		1*8(%rdx), A_0_1, A_0_1
	vpxorq		2*8(%rdx), A_0_2, A_0_2
	vpxorq		3*8(%rdx), A_0_3, A_0_3
	vpxorq		4*8(%rdx), A_0_4, A_0_4
	vpxorq		5*8(%rdx), A_1_0, A_1_0
	vpxorq		6*8(%rdx), A_1_1, A_1_1
	vpxorq		7*8(%rdx), A_1_2, A_1_2
	vpxorq		8*8(%rdx), A_1_3, A_1_3
	vpxorq		9*8(%rdx), A_1_4, A_1_4
	vpxorq		10*8(%rdx), A_2_0, A_2_0
	vpxorq		11*8(%rdx), A_2_1, A_2_1
	vpxorq		12*8(%rdx), A_2_2, A_2_2
	vpxorq		13*8(%rdx), A_2_3, A_2_3
	vpxorq		14*8(%rdx), A_2_4, A_2_4
	vpxorq		15*8(%rdx), A_3_0, A_3_0
	vmovq		16*8(%rdx), C_0
	leaq		17*8(%rdx), %rdx
	vpxor		C_0, A_3_1, A_3_1

	movq		%rsi, %r10
	call		KeccakF1600_ce

	jmp		.Loop_absorb_136_ce

.align	64, 0xcc
.Loop_absorb_104_ce:
	subq		%r8, %rcx	// len - bsz
	jb		.Labsorbed_ce

	vpxorq		0*8(%rdx), A_0_0, A_0_0
	vpxorq		1*8(%rdx), A_0_1, A_0_1
	vpxorq		2*8(%rdx), A_0_2, A_0_2
	vpxorq		3*8(%rdx), A_0_3, A_0_3
	vpxorq		4*8(%rdx), A_0_4, A_0_4
	vpxorq		5*8(%rdx), A_1_0, A_1_0
	vpxorq		6*8(%rdx), A_1_1, A_1_1
	vpxorq		7*8(%rdx), A_1_2, A_1_2
	vpxorq		8*8(%rdx), A_1_3, A_1_3
	vpxorq		9*8(%rdx), A_1_4, A_1_4
	vpxorq		10*8(%rdx), A_2_0, A_2_0
	vpxorq		11*8(%rdx), A_2_1, A_2_1
	vmovq		12*8(%rdx), C_0
	leaq		13*8(%rdx), %rdx
	vpxorq		C_0, A_2_2, A_2_2

	movq		%rsi, %r10
	call		KeccakF1600_ce

	jmp		.Loop_absorb_104_ce

.align	64, 0xcc
.Loop_absorb_72_ce:
	subq		%r8, %rcx	// len - bsz
	jb		.Labsorbed_ce

	vpxorq		0*8(%rdx), A_0_0, A_0_0
	vpxorq		1*8(%rdx), A_0_1, A_0_1
	vpxorq		2*8(%rdx), A_0_2, A_0_2
	vpxorq		3*8(%rdx), A_0_3, A_0_3
	vpxorq		4*8(%rdx), A_0_4, A_0_4
	vpxorq		5*8(%rdx), A_1_0, A_1_0
	vpxorq		6*8(%rdx), A_1_1, A_1_1
	vpxorq		7*8(%rdx), A_1_2, A_1_2
	vmovq		8*8(%rdx), C_0
	leaq		9*8(%rdx), %rdx
	vpxorq		C_0, A_1_3, A_1_3

	movq		%rsi, %r10
	call		KeccakF1600_ce

	jmp		.Loop_absorb_72_ce

.align	64, 0xcc
.Labsorbed_ce:
	vpunpcklqdq	A_0_1, A_0_0, A_0_0
	vpunpcklqdq	A_0_3, A_0_2, A_0_2
	vpunpcklqdq	A_1_0, A_0_4, A_0_4
	vpunpcklqdq	A_1_2, A_1_1, A_1_1
	vpunpcklqdq	A_1_4, A_1_3, A_1_3
	vpunpcklqdq	A_2_1, A_2_0, A_2_0
	vpunpcklqdq	A_2_3, A_2_2, A_2_2
	vpunpcklqdq	A_3_0, A_2_4, A_2_4
	vpunpcklqdq	A_3_2, A_3_1, A_3_1
	vpunpcklqdq	A_3_4, A_3_3, A_3_3
	vpunpcklqdq	A_4_1, A_4_0, A_4_0
	vpunpcklqdq	A_4_3, A_4_2, A_4_2
	vmovdqu64	A_0_0, 0*8(%rdi)
	vmovdqu64	A_0_2, 2*8(%rdi)
	vmovdqu64	A_0_4, 4*8(%rdi)
	vmovdqu64	A_1_1, 6*8(%rdi)
	vmovdqu64	A_1_3, 8*8(%rdi)
	vmovdqu64	A_2_0, 10*8(%rdi)
	vmovdqu64	A_2_2, 0*8(%rax)
	vmovdqu64	A_2_4, 2*8(%rax)
	vmovdqu		A_3_1, 4*8(%rax)
	vmovdqu		A_3_3, 6*8(%rax)
	vmovdqu		A_4_0, 8*8(%rax)
	vmovdqu		A_4_2, 10*8(%rax)
	vmovq		A_4_4, 12*8(%rax)

	leaq		(%r8, %rcx), %rax		// return value
	movq		%rdx, (%r9)			// return buffer pointer

	clear_regs()
	ret_spec_stop
	CFI_ENDPROC()
ELF(.size	_gcry_keccak_absorb_blocks_avx512,
		.-_gcry_keccak_absorb_blocks_avx512)

#endif /* HAVE_GCC_INLINE_ASM_AVX512 */
#endif /* __x86_64 */
