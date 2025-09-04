/* Function cbrtf vectorized with AVX-512.
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
 *     x=2^{3*k+j} * 1.b1 b2 ... b5 b6 ... b52
 *     Let r=(x*2^{-3k-j} - 1.b1 b2 ... b5 1)* rcp[b1 b2 ..b5],
 *     where rcp[b1 b2 .. b5]=1/(1.b1 b2 b3 b4 b5 1) in single precision
 *     cbrtf(2^j * 1. b1 b2 .. b5 1) is approximated as T[j][b1..b5]+D[j][b1..b5]
 *     (T stores the high 24 bits, D stores the low order bits)
 *     Result=2^k*T+(2^k*T*r)*P+2^k*D
 *      where P=p1+p2*r+..
 *
 */

/* Offsets for data table __svml_scbrt_data_internal_avx512
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
#define poly_coeff3			640
#define poly_coeff2			704
#define poly_coeff1			768

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_cbrtf_skx)
	vgetmantps $0, {sae}, %zmm0, %zmm8

	/* GetExp(x) */
	vgetexpps {sae}, %zmm0, %zmm1
	vmovups	BiasL+__svml_scbrt_data_internal_avx512(%rip), %zmm2

	/* exponent/3 */
	vmovups	OneThird+__svml_scbrt_data_internal_avx512(%rip), %zmm3
	vmovups	Bias3+__svml_scbrt_data_internal_avx512(%rip), %zmm4
	vmovups	One+__svml_scbrt_data_internal_avx512(%rip), %zmm15

	/* exponent%3 (to be used as index) */
	vmovups	Three+__svml_scbrt_data_internal_avx512(%rip), %zmm5

	/* polynomial */
	vmovups	poly_coeff3+__svml_scbrt_data_internal_avx512(%rip), %zmm11
	vmovups	poly_coeff1+__svml_scbrt_data_internal_avx512(%rip), %zmm14

	/* Table lookup */
	vmovups	cbrt_tbl_H+__svml_scbrt_data_internal_avx512(%rip), %zmm12

	/* DblRcp ~ 1/Mantissa */
	vrcp14ps %zmm8, %zmm7
	vaddps	{rn-sae}, %zmm2, %zmm1, %zmm6
	vandps	SZero+__svml_scbrt_data_internal_avx512(%rip), %zmm0, %zmm0

	/* round DblRcp to 3 fractional bits (RN mode, no Precision exception) */
	vrndscaleps $88, {sae}, %zmm7, %zmm9
	vfmsub231ps {rn-sae}, %zmm6, %zmm3, %zmm4
	vmovups	poly_coeff2+__svml_scbrt_data_internal_avx512(%rip), %zmm7

	/* Reduced argument: R = DblRcp*Mantissa - 1 */
	vfmsub231ps {rn-sae}, %zmm9, %zmm8, %zmm15
	vrndscaleps $9, {sae}, %zmm4, %zmm13

	/* Prepare table index */
	vpsrld	$19, %zmm9, %zmm10
	vfmadd231ps {rn-sae}, %zmm15, %zmm11, %zmm7
	vfnmadd231ps {rn-sae}, %zmm13, %zmm5, %zmm6
	vpermt2ps cbrt_tbl_H+64+__svml_scbrt_data_internal_avx512(%rip), %zmm10, %zmm12
	vfmadd213ps {rn-sae}, %zmm14, %zmm15, %zmm7
	vscalefps {rn-sae}, %zmm13, %zmm12, %zmm2

	/* Table lookup: 2^(exponent%3) */
	vpermps	__svml_scbrt_data_internal_avx512(%rip), %zmm6, %zmm1
	vpermps	etbl_L+__svml_scbrt_data_internal_avx512(%rip), %zmm6, %zmm6

	/* Sh*R */
	vmulps	{rn-sae}, %zmm15, %zmm1, %zmm14

	/* Sl + (Sh*R)*Poly */
	vfmadd213ps {rn-sae}, %zmm6, %zmm7, %zmm14

	/*
	 * branch-free
	 * scaled_Th*(Sh+Sl+Sh*R*Poly)
	 */
	vaddps	{rn-sae}, %zmm1, %zmm14, %zmm15
	vmulps	{rn-sae}, %zmm2, %zmm15, %zmm3
	vorps	%zmm0, %zmm3, %zmm0
	ret

END(_ZGVeN16v_cbrtf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_scbrt_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 etbl_H[16][1];
	__declspec(align(64)) VUINT32 etbl_L[16][1];
	__declspec(align(64)) VUINT32 cbrt_tbl_H[32][1];
	__declspec(align(64)) VUINT32 BiasL[16][1];
	__declspec(align(64)) VUINT32 SZero[16][1];
	__declspec(align(64)) VUINT32 OneThird[16][1];
	__declspec(align(64)) VUINT32 Bias3[16][1];
	__declspec(align(64)) VUINT32 Three[16][1];
	__declspec(align(64)) VUINT32 One[16][1];
	__declspec(align(64)) VUINT32 poly_coeff3[16][1];
	__declspec(align(64)) VUINT32 poly_coeff2[16][1];
	__declspec(align(64)) VUINT32 poly_coeff1[16][1];
} __svml_scbrt_data_internal_avx512;
#endif
__svml_scbrt_data_internal_avx512:
	/* etbl_H */
	.long	0x3f800000
	.long	0x3fa14518
	.long	0x3fcb2ff5
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	/* etbl_L */
	.align	64
	.long	0x00000000
	.long	0xb2ce51af
	.long	0x32a7adc8
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	/* cbrt_tbl_H */
	.align	64
	.long	0x3fa14518
	.long	0x3f9e0b2b
	.long	0x3f9b0f9b
	.long	0x3f984a9a
	.long	0x3f95b5af
	.long	0x3f934b6c
	.long	0x3f910737
	.long	0x3f8ee526
	.long	0x3f8ce1da
	.long	0x3f8afa6a
	.long	0x3f892c4e
	.long	0x3f87754e
	.long	0x3f85d377
	.long	0x3f844510
	.long	0x3f82c892
	.long	0x3f815c9f
	.long	0x3f800000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	.long	0x00000000
	/* BiasL */
	.align	64
	.long	0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000, 0x4b400000
	/* Zero */
	.align	64
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000
	/* OneThird */
	.align	64
	.long	0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab, 0x3eaaaaab
	/* Bias3 */
	.align	64
	.long	0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000, 0x4a800000
	/* Three */
	.align	64
	.long	0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000, 0x40400000
	/* One */
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* poly_coeff3 */
	.align	64
	.long	0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c, 0x3d7d057c
	/* poly_coeff2 */
	.align	64
	.long	0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363, 0xbde3a363
	/* poly_coeff1 */
	.align	64
	.long	0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa, 0x3eaaaaaa
	.align	64
	.type	__svml_scbrt_data_internal_avx512, @object
	.size	__svml_scbrt_data_internal_avx512, .-__svml_scbrt_data_internal_avx512
