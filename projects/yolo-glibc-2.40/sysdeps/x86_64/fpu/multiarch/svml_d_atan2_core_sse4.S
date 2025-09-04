/* Function atan vectorized with SSE4.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   https://www.gnu.org/licenses/.  */

/*
 * ALGORITHM DESCRIPTION:
 *
 *      For    0.0    <= x <=  7.0/16.0: atan(x) = atan(0.0) + atan(s), where s=(x-0.0)/(1.0+0.0*x)
 *      For  7.0/16.0 <= x <= 11.0/16.0: atan(x) = atan(0.5) + atan(s), where s=(x-0.5)/(1.0+0.5*x)
 *      For 11.0/16.0 <= x <= 19.0/16.0: atan(x) = atan(1.0) + atan(s), where s=(x-1.0)/(1.0+1.0*x)
 *      For 19.0/16.0 <= x <= 39.0/16.0: atan(x) = atan(1.5) + atan(s), where s=(x-1.5)/(1.0+1.5*x)
 *      For 39.0/16.0 <= x <=    inf   : atan(x) = atan(inf) + atan(s), where s=-1.0/x
 *      Where atan(s) ~= s+s^3*Poly11(s^2) on interval |s|<7.0/0.16.
 *
 */

/* Offsets for data table __svml_datan_data_internal_avx512
 */
#define AbsMask				0
#define Shifter				16
#define MaxThreshold			32
#define MOne				48
#define One				64
#define LargeX				80
#define Zero				96
#define Tbl_H				112
#define Tbl_L				368
#define dIndexMed			624
#define Pi2				640
#define Pi2_low				656
#define coeff				672

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN2v_atan_sse4)
	lea	Tbl_H+128+__svml_datan_data_internal_avx512(%rip), %rcx
	movups	__svml_datan_data_internal_avx512(%rip), %xmm4
	movups	Shifter+__svml_datan_data_internal_avx512(%rip), %xmm3
	andps	%xmm0, %xmm4
	movaps	%xmm3, %xmm12
	movaps	%xmm4, %xmm5
	addpd	%xmm4, %xmm12
	movaps	%xmm12, %xmm7

	/*
	 * table lookup sequence
	 * VPERMUTE not available
	 */
	movaps	%xmm12, %xmm10
	subpd	%xmm3, %xmm7
	subpd	%xmm7, %xmm5
	mulpd	%xmm4, %xmm7
	movups	MaxThreshold+__svml_datan_data_internal_avx512(%rip), %xmm2
	psllq	$3, %xmm10

	/* saturate X range */
	movups	LargeX+__svml_datan_data_internal_avx512(%rip), %xmm8
	pxor	%xmm4, %xmm0
	cmplepd	%xmm4, %xmm2
	addpd	One+__svml_datan_data_internal_avx512(%rip), %xmm7
	minpd	%xmm4, %xmm8
	movups	MOne+__svml_datan_data_internal_avx512(%rip), %xmm6
	movaps	%xmm2, %xmm1
	movaps	%xmm2, %xmm9
	andnps	%xmm5, %xmm1
	andps	%xmm2, %xmm6
	andnps	%xmm7, %xmm9
	andps	%xmm2, %xmm8
	orps	%xmm6, %xmm1
	orps	%xmm8, %xmm9

	/* R+Rl = DiffX/Y */
	divpd	%xmm9, %xmm1
	pand	.FLT_11(%rip), %xmm10

	/* set table value to Pi/2 for large X */
	movups	Pi2+__svml_datan_data_internal_avx512(%rip), %xmm4
	movd	%xmm10, %eax
	andps	%xmm2, %xmm4
	pshufd	$2, %xmm10, %xmm11
	movaps	%xmm2, %xmm10

	/* polynomial evaluation */
	movaps	%xmm1, %xmm2
	mulpd	%xmm1, %xmm2
	movd	%xmm11, %edx
	movups	coeff+__svml_datan_data_internal_avx512(%rip), %xmm5
	movaps	%xmm2, %xmm7
	movups	coeff+32+__svml_datan_data_internal_avx512(%rip), %xmm6
	movaps	%xmm2, %xmm9
	mulpd	%xmm2, %xmm5
	mulpd	%xmm2, %xmm7
	addpd	coeff+16+__svml_datan_data_internal_avx512(%rip), %xmm5
	mulpd	%xmm2, %xmm6
	mulpd	%xmm7, %xmm5
	addpd	coeff+48+__svml_datan_data_internal_avx512(%rip), %xmm6
	mulpd	%xmm1, %xmm9
	addpd	%xmm5, %xmm6
	movups	coeff+64+__svml_datan_data_internal_avx512(%rip), %xmm8
	mulpd	%xmm2, %xmm8
	mulpd	%xmm6, %xmm7
	addpd	coeff+80+__svml_datan_data_internal_avx512(%rip), %xmm8
	addpd	%xmm7, %xmm8
	mulpd	%xmm8, %xmm9
	movups	dIndexMed+__svml_datan_data_internal_avx512(%rip), %xmm14
	cmplepd	%xmm12, %xmm14
	addpd	%xmm9, %xmm1
	movslq	%eax, %rax
	movaps	%xmm14, %xmm3
	movslq	%edx, %rdx
	movsd	-128(%rax, %rcx), %xmm13
	movsd	(%rcx, %rax), %xmm15
	movhpd	-128(%rdx, %rcx), %xmm13
	movhpd	(%rcx, %rdx), %xmm15
	andnps	%xmm13, %xmm3
	andps	%xmm14, %xmm15
	orps	%xmm15, %xmm3
	andnps	%xmm3, %xmm10
	orps	%xmm4, %xmm10
	addpd	%xmm1, %xmm10
	pxor	%xmm10, %xmm0
	ret

END(_ZGVbN2v_atan_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_datan_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 AbsMask[2][2];
	__declspec(align(16)) VUINT32 Shifter[2][2];
	__declspec(align(16)) VUINT32 MaxThreshold[2][2];
	__declspec(align(16)) VUINT32 MOne[2][2];
	__declspec(align(16)) VUINT32 One[2][2];
	__declspec(align(16)) VUINT32 LargeX[2][2];
	__declspec(align(16)) VUINT32 Zero[2][2];
	__declspec(align(16)) VUINT32 Tbl_H[32][2];
	__declspec(align(16)) VUINT32 Tbl_L[32][2];
	__declspec(align(16)) VUINT32 dIndexMed[2][2];
	__declspec(align(16)) VUINT32 Pi2[2][2];
	__declspec(align(16)) VUINT32 Pi2_low[2][2];
	__declspec(align(16)) VUINT32 coeff[6][2][2];
} __svml_datan_data_internal_avx512;
#endif
__svml_datan_data_internal_avx512:
	/* AbsMask */
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff
	/* Shifter */
	.align	16
	.quad	0x4318000000000000, 0x4318000000000000
	/* MaxThreshold */
	.align	16
	.quad	0x401f800000000000, 0x401f800000000000
	/* MOne */
	.align	16
	.quad	0xbff0000000000000, 0xbff0000000000000
	/* One */
	.align	16
	.quad	0x3ff0000000000000, 0x3ff0000000000000
	/* LargeX */
	.align	16
	.quad	0x47f0000000000000, 0x47f0000000000000
	/* Zero */
	.align	16
	.quad	0x0000000000000000, 0x0000000000000000
	/* Tbl_H */
	.align	16
	.quad	0x0000000000000000, 0x3fcf5b75f92c80dd
	.quad	0x3fddac670561bb4f, 0x3fe4978fa3269ee1
	.quad	0x3fe921fb54442d18, 0x3fecac7c57846f9e
	.quad	0x3fef730bd281f69b, 0x3ff0d38f2c5ba09f
	.quad	0x3ff1b6e192ebbe44, 0x3ff270ef55a53a25
	.quad	0x3ff30b6d796a4da8, 0x3ff38d6a6ce13353
	.quad	0x3ff3fc176b7a8560, 0x3ff45b54837351a0
	.quad	0x3ff4ae10fc6589a5, 0x3ff4f68dea672617
	.quad	0x3ff5368c951e9cfd, 0x3ff56f6f33a3e6a7
	.quad	0x3ff5a25052114e60, 0x3ff5d013c41adabd
	.quad	0x3ff5f97315254857, 0x3ff61f06c6a92b89
	.quad	0x3ff6414d44094c7c, 0x3ff660b02c736a06
	.quad	0x3ff67d8863bc99bd, 0x3ff698213a9d5053
	.quad	0x3ff6b0bae830c070, 0x3ff6c78c7edeb195
	.quad	0x3ff6dcc57bb565fd, 0x3ff6f08f07435fec
	.quad	0x3ff7030cf9403197, 0x3ff7145eac2088a4
	/* Tbl_L */
	.align	16
	.quad	0x0000000000000000, 0x3c68ab6e3cf7afbd
	.quad	0x3c7a2b7f222f65e2, 0x3c72419a87f2a458
	.quad	0x3c81a62633145c07, 0x3c80dae13ad18a6b
	.quad	0x3c7007887af0cbbd, 0xbc9bd0dc231bfd70
	.quad	0x3c9b1b466a88828e, 0xbc9a66b1af5f84fb
	.quad	0x3c96254cb03bb199, 0xbc812c77e8a80f5c
	.quad	0xbc4441a3bd3f1084, 0x3c79e4a72eedacc4
	.quad	0xbc93b03e8a27f555, 0x3c9934f9f2b0020e
	.quad	0xbc996f47948a99f1, 0xbc7df6edd6f1ec3b
	.quad	0x3c78c2d0c89de218, 0x3c9f82bba194dd5d
	.quad	0xbc831151a43b51ca, 0xbc8487d50bceb1a5
	.quad	0xbc9c5f60a65c7397, 0xbc7acb6afb332a0f
	.quad	0xbc99b7bd2e1e8c9c, 0xbc9b9839085189e3
	.quad	0xbc97d1ab82ffb70b, 0x3c99239ad620ffe2
	.quad	0xbc929c86447928e7, 0xbc8957a7170df016
	.quad	0xbc7cbe1896221608, 0xbc9fda5797b32a0b
	/* dIndexMed */
	.align	16
	.quad	0x4318000000000010, 0x4318000000000010
	/* Pi2 */
	.align	16
	.quad	0x3ff921fb54442d18, 0x3ff921fb54442d18
	/* Pi2_low */
	.align	16
	.quad	0x3c91a62633145c07, 0x3c91a62633145c07
	/* coeff6 */
	.align	16
	.quad	0x3fb2e9b9f5c4fe97, 0x3fb2e9b9f5c4fe97
	.quad	0xbfb74257c46790cc, 0xbfb74257c46790cc
	.quad	0x3fbc71bfeff916a0, 0x3fbc71bfeff916a0
	.quad	0xbfc249248eef04da, 0xbfc249248eef04da
	.quad	0x3fc999999998741e, 0x3fc999999998741e
	.quad	0xbfd555555555554d, 0xbfd555555555554d
	.align	16
	.type	__svml_datan_data_internal_avx512, @object
	.size	__svml_datan_data_internal_avx512, .-__svml_datan_data_internal_avx512
	.align	16

.FLT_11:
	.long	0x00000078, 0x00000000, 0x00000078, 0x00000000
	.type	.FLT_11, @object
	.size	.FLT_11, 16
