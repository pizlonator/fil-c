/* Function atan vectorized with AVX-512.
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
#define Shifter				64
#define MaxThreshold			128
#define MOne				192
#define One				256
#define LargeX				320
#define Zero				384
#define Tbl_H				448
#define dIndexMed			704
#define Pi2				768
#define coeff_1				832
#define coeff_2				896
#define coeff_3				960
#define coeff_4				1024
#define coeff_5				1088
#define coeff_6				1152

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN8v_atan_skx)
	vmovups	Shifter+__svml_datan_data_internal_avx512(%rip), %zmm4
	vmovups	MaxThreshold+__svml_datan_data_internal_avx512(%rip), %zmm3
	vmovups	One+__svml_datan_data_internal_avx512(%rip), %zmm9

	/* saturate X range */
	vmovups	LargeX+__svml_datan_data_internal_avx512(%rip), %zmm7
	vandpd	__svml_datan_data_internal_avx512(%rip), %zmm0, %zmm8

	/* R+Rl = DiffX/Y */
	vbroadcastsd .FLT_10(%rip), %zmm15
	vaddpd	{rn-sae}, %zmm4, %zmm8, %zmm2
	vxorpd	%zmm0, %zmm8, %zmm1
	vcmppd	$29, {sae}, %zmm3, %zmm8, %k2

	/* round to 2 bits after binary point */
	vreducepd $40, {sae}, %zmm8, %zmm6
	vsubpd	{rn-sae}, %zmm4, %zmm2, %zmm5

	/*
	 * if|X|>=MaxThreshold, set DiffX=-1
	 * VMSUB(D, DiffX, LargeMask, Zero, One);
	 */
	vblendmpd MOne+__svml_datan_data_internal_avx512(%rip), %zmm6, %zmm10{%k2}
	vfmadd231pd {rn-sae}, %zmm8, %zmm5, %zmm9
	vmovups	dIndexMed+__svml_datan_data_internal_avx512(%rip), %zmm5

	/* table lookup sequence */
	vmovups	Tbl_H+__svml_datan_data_internal_avx512(%rip), %zmm6
	vgetmantpd $0, {sae}, %zmm10, %zmm14
	vgetexppd {sae}, %zmm10, %zmm11
	vmovups	coeff_5+__svml_datan_data_internal_avx512(%rip), %zmm10

	/*
	 * if|X|>=MaxThreshold, set Y=X
	 * VMADD(D, Y, LargeMask, X, Zero);
	 */
	vminpd	{sae}, %zmm8, %zmm7, %zmm9{%k2}
	vcmppd	$29, {sae}, %zmm5, %zmm2, %k1
	vmovups	Tbl_H+128+__svml_datan_data_internal_avx512(%rip), %zmm7
	vmovups	coeff_1+__svml_datan_data_internal_avx512(%rip), %zmm8
	vgetmantpd $0, {sae}, %zmm9, %zmm3
	vgetexppd {sae}, %zmm9, %zmm12
	vmovups	coeff_3+__svml_datan_data_internal_avx512(%rip), %zmm9
	vpermt2pd Tbl_H+64+__svml_datan_data_internal_avx512(%rip), %zmm2, %zmm6
	vsubpd	{rn-sae}, %zmm12, %zmm11, %zmm4
	vpermt2pd Tbl_H+192+__svml_datan_data_internal_avx512(%rip), %zmm2, %zmm7
	vrcp14pd %zmm3, %zmm13
	vmovups	coeff_4+__svml_datan_data_internal_avx512(%rip), %zmm12
	vmovups	coeff_6+__svml_datan_data_internal_avx512(%rip), %zmm11
	vblendmpd %zmm7, %zmm6, %zmm2{%k1}
	vmulpd	{rn-sae}, %zmm13, %zmm14, %zmm0
	vfnmadd231pd {rn-sae}, %zmm3, %zmm13, %zmm15
	vfnmadd213pd {rn-sae}, %zmm14, %zmm0, %zmm3
	vfmadd213pd {rn-sae}, %zmm15, %zmm15, %zmm15
	vfmadd213pd {rn-sae}, %zmm13, %zmm13, %zmm15
	vfmadd213pd {rn-sae}, %zmm0, %zmm15, %zmm3
	vscalefpd {rn-sae}, %zmm4, %zmm3, %zmm0

	/* set table value to Pi/2 for large X */
	vblendmpd Pi2+__svml_datan_data_internal_avx512(%rip), %zmm2, %zmm3{%k2}
	vmovups	coeff_2+__svml_datan_data_internal_avx512(%rip), %zmm2

	/* polynomial evaluation */
	vmulpd	{rn-sae}, %zmm0, %zmm0, %zmm14
	vmulpd	{rn-sae}, %zmm14, %zmm14, %zmm13
	vmulpd	{rn-sae}, %zmm0, %zmm14, %zmm15
	vfmadd231pd {rn-sae}, %zmm14, %zmm8, %zmm2
	vfmadd231pd {rn-sae}, %zmm14, %zmm9, %zmm12
	vfmadd213pd {rn-sae}, %zmm11, %zmm10, %zmm14
	vfmadd213pd {rn-sae}, %zmm12, %zmm13, %zmm2
	vfmadd213pd {rn-sae}, %zmm14, %zmm13, %zmm2
	vfmadd213pd {rn-sae}, %zmm0, %zmm15, %zmm2
	vaddpd	{rn-sae}, %zmm3, %zmm2, %zmm0
	vxorpd	%zmm1, %zmm0, %zmm0
	ret

END(_ZGVeN8v_atan_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_datan_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 AbsMask[8][2];
	__declspec(align(64)) VUINT32 Shifter[8][2];
	__declspec(align(64)) VUINT32 MaxThreshold[8][2];
	__declspec(align(64)) VUINT32 MOne[8][2];
	__declspec(align(64)) VUINT32 One[8][2];
	__declspec(align(64)) VUINT32 LargeX[8][2];
	__declspec(align(64)) VUINT32 Zero[8][2];
	__declspec(align(64)) VUINT32 Tbl_H[32][2];
	__declspec(align(64)) VUINT32 dIndexMed[8][2];
	__declspec(align(64)) VUINT32 Pi2[8][2];
	__declspec(align(64)) VUINT32 coeff[6][8][2];
} __svml_datan_data_internal_avx512;
#endif
__svml_datan_data_internal_avx512:
	/* AbsMask */
	.quad	0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff, 0x7fffffffffffffff
	/* Shifter */
	.align	64
	.quad	0x4318000000000000, 0x4318000000000000, 0x4318000000000000, 0x4318000000000000, 0x4318000000000000, 0x4318000000000000, 0x4318000000000000, 0x4318000000000000
	/* MaxThreshold */
	.align	64
	.quad	0x401f800000000000, 0x401f800000000000, 0x401f800000000000, 0x401f800000000000, 0x401f800000000000, 0x401f800000000000, 0x401f800000000000, 0x401f800000000000
	/* MOne */
	.align	64
	.quad	0xbff0000000000000, 0xbff0000000000000, 0xbff0000000000000, 0xbff0000000000000, 0xbff0000000000000, 0xbff0000000000000, 0xbff0000000000000, 0xbff0000000000000
	/* One */
	.align	64
	.quad	0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000, 0x3ff0000000000000
	/* LargeX */
	.align	64
	.quad	0x47f0000000000000, 0x47f0000000000000, 0x47f0000000000000, 0x47f0000000000000, 0x47f0000000000000, 0x47f0000000000000, 0x47f0000000000000, 0x47f0000000000000
	/* Zero */
	.align	64
	.quad	0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000
	/* Tbl_H */
	.align	64
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
	/* dIndexMed */
	.align	64
	.quad	0x4318000000000010, 0x4318000000000010, 0x4318000000000010, 0x4318000000000010, 0x4318000000000010, 0x4318000000000010, 0x4318000000000010, 0x4318000000000010
	/* Pi2 */
	.align	64
	.quad	0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18, 0x3ff921fb54442d18
	/* coeff6 */
	.align	64
	.quad	0x3fb2e9b9f5c4fe97, 0x3fb2e9b9f5c4fe97, 0x3fb2e9b9f5c4fe97, 0x3fb2e9b9f5c4fe97, 0x3fb2e9b9f5c4fe97, 0x3fb2e9b9f5c4fe97, 0x3fb2e9b9f5c4fe97, 0x3fb2e9b9f5c4fe97
	.quad	0xbfb74257c46790cc, 0xbfb74257c46790cc, 0xbfb74257c46790cc, 0xbfb74257c46790cc, 0xbfb74257c46790cc, 0xbfb74257c46790cc, 0xbfb74257c46790cc, 0xbfb74257c46790cc
	.quad	0x3fbc71bfeff916a0, 0x3fbc71bfeff916a0, 0x3fbc71bfeff916a0, 0x3fbc71bfeff916a0, 0x3fbc71bfeff916a0, 0x3fbc71bfeff916a0, 0x3fbc71bfeff916a0, 0x3fbc71bfeff916a0
	.quad	0xbfc249248eef04da, 0xbfc249248eef04da, 0xbfc249248eef04da, 0xbfc249248eef04da, 0xbfc249248eef04da, 0xbfc249248eef04da, 0xbfc249248eef04da, 0xbfc249248eef04da
	.quad	0x3fc999999998741e, 0x3fc999999998741e, 0x3fc999999998741e, 0x3fc999999998741e, 0x3fc999999998741e, 0x3fc999999998741e, 0x3fc999999998741e, 0x3fc999999998741e
	.quad	0xbfd555555555554d, 0xbfd555555555554d, 0xbfd555555555554d, 0xbfd555555555554d, 0xbfd555555555554d, 0xbfd555555555554d, 0xbfd555555555554d, 0xbfd555555555554d
	.align	64
	.type	__svml_datan_data_internal_avx512, @object
	.size	__svml_datan_data_internal_avx512, .-__svml_datan_data_internal_avx512
	.align	8

.FLT_10:
	.long	0x00000000, 0x3ff00000
	.type	.FLT_10, @object
	.size	.FLT_10, 8
