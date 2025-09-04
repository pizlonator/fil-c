/* Function cbrt vectorized with AVX-512.
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
 *   x=2^{3*k+j} * 1.b1 b2 ... b5 b6 ... b52
 *   Let r=(x*2^{-3k-j} - 1.b1 b2 ... b5 1)* rcp[b1 b2 ..b5],
 *   where rcp[b1 b2 .. b5]=1/(1.b1 b2 b3 b4 b5 1) in double precision
 *   cbrt(2^j * 1. b1 b2 .. b5 1) is approximated as T[j][b1..b5]+D[j][b1..b5]
 *   (T stores the high 53 bits, D stores the low order bits)
 *   Result=2^k*T+(2^k*T*r)*P+2^k*D
 *   where P=p1+p2*r+..+p8*r^7
 *
 */

/* Offsets for data table __svml_dcbrt_data_internal_avx512
 */
#define etbl_H				0
#define etbl_L				64
#define cbrt_tbl_H			128
#define BiasL				256
#define SZero				320
#define OneThird			384
#define Bias3				448
#define Three				512
#define One				576
#define poly_coeff10			640
#define poly_coeff9			704
#define poly_coeff8			768
#define poly_coeff7			832
#define poly_coeff6			896
#define poly_coeff5			960
#define poly_coeff4			1024
#define poly_coeff3			1088
#define poly_coeff2			1152
#define poly_coeff1			1216

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_cbrt_skx)
	vgetmantpd $0, {sae}, %zmm0, %zmm14

	/* GetExp(x) */
	vgetexppd {sae}, %zmm0, %zmm7
	vmovups	BiasL+__svml_dcbrt_data_internal_avx512(%rip), %zmm8

	/* exponent/3 */
	vmovups	OneThird+__svml_dcbrt_data_internal_avx512(%rip), %zmm9
	vmovups	Bias3+__svml_dcbrt_data_internal_avx512(%rip), %zmm10

	/* Reduced argument: R = DblRcp*Mantissa - 1 */
	vmovups	One+__svml_dcbrt_data_internal_avx512(%rip), %zmm2

	/* exponent%3 (to be used as index) */
	vmovups	Three+__svml_dcbrt_data_internal_avx512(%rip), %zmm11

	/* DblRcp ~ 1/Mantissa */
	vrcp14pd %zmm14, %zmm13
	vaddpd	{rn-sae}, %zmm8, %zmm7, %zmm12
	vandpd	SZero+__svml_dcbrt_data_internal_avx512(%rip), %zmm0, %zmm6

	/* round DblRcp to 3 fractional bits (RN mode, no Precision exception) */
	vrndscalepd $72, {sae}, %zmm13, %zmm15
	vfmsub231pd {rn-sae}, %zmm12, %zmm9, %zmm10

	/* polynomial */
	vmovups	poly_coeff10+__svml_dcbrt_data_internal_avx512(%rip), %zmm0
	vmovups	poly_coeff8+__svml_dcbrt_data_internal_avx512(%rip), %zmm7
	vmovups	poly_coeff7+__svml_dcbrt_data_internal_avx512(%rip), %zmm9
	vfmsub231pd {rn-sae}, %zmm15, %zmm14, %zmm2
	vrndscalepd $9, {sae}, %zmm10, %zmm5

	/* Table lookup */
	vmovups	cbrt_tbl_H+__svml_dcbrt_data_internal_avx512(%rip), %zmm10
	vmovups	poly_coeff6+__svml_dcbrt_data_internal_avx512(%rip), %zmm8
	vmovups	poly_coeff3+__svml_dcbrt_data_internal_avx512(%rip), %zmm13
	vfmadd231pd {rn-sae}, %zmm2, %zmm7, %zmm9
	vfnmadd231pd {rn-sae}, %zmm5, %zmm11, %zmm12
	vmovups	poly_coeff5+__svml_dcbrt_data_internal_avx512(%rip), %zmm11
	vmovups	poly_coeff1+__svml_dcbrt_data_internal_avx512(%rip), %zmm14

	/* Prepare table index */
	vpsrlq	$49, %zmm15, %zmm1

	/* Table lookup: 2^(exponent%3) */
	vpermpd	__svml_dcbrt_data_internal_avx512(%rip), %zmm12, %zmm4
	vpermpd	etbl_L+__svml_dcbrt_data_internal_avx512(%rip), %zmm12, %zmm3
	vpermt2pd cbrt_tbl_H+64+__svml_dcbrt_data_internal_avx512(%rip), %zmm1, %zmm10
	vmovups	poly_coeff9+__svml_dcbrt_data_internal_avx512(%rip), %zmm1
	vfmadd231pd {rn-sae}, %zmm2, %zmm8, %zmm11
	vmovups	poly_coeff2+__svml_dcbrt_data_internal_avx512(%rip), %zmm12
	vscalefpd {rn-sae}, %zmm5, %zmm10, %zmm15
	vfmadd231pd {rn-sae}, %zmm2, %zmm0, %zmm1
	vmovups	poly_coeff4+__svml_dcbrt_data_internal_avx512(%rip), %zmm5
	vfmadd231pd {rn-sae}, %zmm2, %zmm12, %zmm14
	vmulpd	{rn-sae}, %zmm2, %zmm2, %zmm0
	vfmadd231pd {rn-sae}, %zmm2, %zmm5, %zmm13

	/* Sh*R */
	vmulpd	{rn-sae}, %zmm2, %zmm4, %zmm2
	vfmadd213pd {rn-sae}, %zmm9, %zmm0, %zmm1
	vfmadd213pd {rn-sae}, %zmm11, %zmm0, %zmm1
	vfmadd213pd {rn-sae}, %zmm13, %zmm0, %zmm1
	vfmadd213pd {rn-sae}, %zmm14, %zmm0, %zmm1

	/* Sl + (Sh*R)*Poly */
	vfmadd213pd {rn-sae}, %zmm3, %zmm1, %zmm2

	/*
	 * branch-free
	 * scaled_Th*(Sh+Sl+Sh*R*Poly)
	 */
	vaddpd	{rn-sae}, %zmm4, %zmm2, %zmm3
	vmulpd	{rn-sae}, %zmm15, %zmm3, %zmm4
	vorpd	%zmm6, %zmm4, %zmm0
	ret

END(_ZGVeN8v_cbrt_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_dcbrt_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 etbl_H[8][2];
	__declspec(align(64)) VUINT32 etbl_L[8][2];
	__declspec(align(64)) VUINT32 cbrt_tbl_H[16][2];
	__declspec(align(64)) VUINT32 BiasL[8][2];
	__declspec(align(64)) VUINT32 SZero[8][2];
	__declspec(align(64)) VUINT32 OneThird[8][2];
	__declspec(align(64)) VUINT32 Bias3[8][2];
	__declspec(align(64)) VUINT32 Three[8][2];
	__declspec(align(64)) VUINT32 One[8][2];
	__declspec(align(64)) VUINT32 poly_coeff10[8][2];
	__declspec(align(64)) VUINT32 poly_coeff9[8][2];
	__declspec(align(64)) VUINT32 poly_coeff8[8][2];
	__declspec(align(64)) VUINT32 poly_coeff7[8][2];
	__declspec(align(64)) VUINT32 poly_coeff6[8][2];
	__declspec(align(64)) VUINT32 poly_coeff5[8][2];
	__declspec(align(64)) VUINT32 poly_coeff4[8][2];
	__declspec(align(64)) VUINT32 poly_coeff3[8][2];
	__declspec(align(64)) VUINT32 poly_coeff2[8][2];
	__declspec(align(64)) VUINT32 poly_coeff1[8][2];
} __svml_dcbrt_data_internal_avx512;
#endif
__svml_dcbrt_data_internal_avx512:
	/* etbl_H */
	.quad	0x3ff0000000000000
	.quad	0x3ff428a2f98d728b
	.quad	0x3ff965fea53d6e3d
	.quad	0x0000000000000000
	.quad	0xbff0000000000000
	.quad	0xbff428a2f98d728b
	.quad	0xbff965fea53d6e3d
	.quad	0x0000000000000000
	/* etbl_L */
	.align	64
	.quad	0x0000000000000000
	.quad	0xbc7ddc22548ea41e
	.quad	0xbc9f53e999952f09
	.quad	0x0000000000000000
	.quad	0x0000000000000000
	.quad	0x3c7ddc22548ea41e
	.quad	0x3c9f53e999952f09
	.quad	0x0000000000000000
	/* cbrt_tbl_H */
	.align	64
	.quad	0x3ff428a2f98d728b
	.quad	0x3ff361f35ca116ff
	.quad	0x3ff2b6b5edf6b54a
	.quad	0x3ff220e6dd675180
	.quad	0x3ff19c3b38e975a8
	.quad	0x3ff12589c21fb842
	.quad	0x3ff0ba6ee5f9aad4
	.quad	0x3ff059123d3a9848
	.quad	0x3ff0000000000000
	.quad	0x0000000000000000
	.quad	0x0000000000000000
	.quad	0x0000000000000000
	.quad	0x0000000000000000
	.quad	0x0000000000000000
	.quad	0x0000000000000000
	.quad	0x0000000000000000
	/* BiasL */
	.align	64
	.quad	0x4338000000000000, 0x4338000000000000, 0x4338000000000000, 0x4338000000000000, 0x4338000000000000, 0x4338000000000000, 0x4338000000000000, 0x4338000000000000
	/* Zero */
	.align	64
	.quad	0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000, 0x8000000000000000
	/* OneThird */
	.align	64
	.quad	0x3fd5555555555556, 0x3fd5555555555556, 0x3fd5555555555556, 0x3fd5555555555556, 0x3fd5555555555556, 0x3fd5555555555556, 0x3fd5555555555556, 0x3fd5555555555556
	/* Bias3 */
	.align	64
	.quad	0x4320000000000000, 0x4320000000000000, 0x4320000000000000, 0x4320000000000000, 0x4320000000000000, 0x4320000000000000, 0x4320000000000000, 0x4320000000000000
	/* Three */
	.align	64
	.quad	0x4008000000000000, 0x4008000000000000, 0x4008000000000000, 0x4008000000000000, 0x4008000000000000, 0x4008000000000000, 0x4008000000000000, 0x4008000000000000
	/* One */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* poly_coeff10 */
	.align	64
	.quad	0xbf882e3b6adeca62, 0xbf882e3b6adeca62, 0xbf882e3b6adeca62, 0xbf882e3b6adeca62, 0xbf882e3b6adeca62, 0xbf882e3b6adeca62, 0xbf882e3b6adeca62, 0xbf882e3b6adeca62
	/* poly_coeff9 */
	.align	64
	.quad	0x3f8bda24bae48875, 0x3f8bda24bae48875, 0x3f8bda24bae48875, 0x3f8bda24bae48875, 0x3f8bda24bae48875, 0x3f8bda24bae48875, 0x3f8bda24bae48875, 0x3f8bda24bae48875
	/* poly_coeff8 */
	.align	64
	.quad	0xbf9036b87c71d55f, 0xbf9036b87c71d55f, 0xbf9036b87c71d55f, 0xbf9036b87c71d55f, 0xbf9036b87c71d55f, 0xbf9036b87c71d55f, 0xbf9036b87c71d55f, 0xbf9036b87c71d55f
	/* poly_coeff7 */
	.align	64
	.quad	0x3f9374ed9398b914, 0x3f9374ed9398b914, 0x3f9374ed9398b914, 0x3f9374ed9398b914, 0x3f9374ed9398b914, 0x3f9374ed9398b914, 0x3f9374ed9398b914, 0x3f9374ed9398b914
	/* poly_coeff6 */
	.align	64
	.quad	0xbf98090d77f2468e, 0xbf98090d77f2468e, 0xbf98090d77f2468e, 0xbf98090d77f2468e, 0xbf98090d77f2468e, 0xbf98090d77f2468e, 0xbf98090d77f2468e, 0xbf98090d77f2468e
	/* poly_coeff5 */
	.align	64
	.quad	0x3f9ee71141dcf569, 0x3f9ee71141dcf569, 0x3f9ee71141dcf569, 0x3f9ee71141dcf569, 0x3f9ee71141dcf569, 0x3f9ee71141dcf569, 0x3f9ee71141dcf569, 0x3f9ee71141dcf569
	/* poly_coeff4 */
	.align	64
	.quad	0xbfa511e8d2b0363e, 0xbfa511e8d2b0363e, 0xbfa511e8d2b0363e, 0xbfa511e8d2b0363e, 0xbfa511e8d2b0363e, 0xbfa511e8d2b0363e, 0xbfa511e8d2b0363e, 0xbfa511e8d2b0363e
	/* poly_coeff3 */
	.align	64
	.quad	0x3faf9add3c0b7e31, 0x3faf9add3c0b7e31, 0x3faf9add3c0b7e31, 0x3faf9add3c0b7e31, 0x3faf9add3c0b7e31, 0x3faf9add3c0b7e31, 0x3faf9add3c0b7e31, 0x3faf9add3c0b7e31
	/* poly_coeff2 */
	.align	64
	.quad	0xbfbc71c71c71c741, 0xbfbc71c71c71c741, 0xbfbc71c71c71c741, 0xbfbc71c71c71c741, 0xbfbc71c71c71c741, 0xbfbc71c71c71c741, 0xbfbc71c71c71c741, 0xbfbc71c71c71c741
	/* poly_coeff1 */
	.align	64
	.quad	0x3fd5555555555557, 0x3fd5555555555557, 0x3fd5555555555557, 0x3fd5555555555557, 0x3fd5555555555557, 0x3fd5555555555557, 0x3fd5555555555557, 0x3fd5555555555557
	.align	64
	.type	__svml_dcbrt_data_internal_avx512, @object
	.size	__svml_dcbrt_data_internal_avx512, .-__svml_dcbrt_data_internal_avx512
