/* Function atanf vectorized with AVX-512.
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

/* Offsets for data table __svml_satan_data_internal_avx512
 */
#define AbsMask				0
#define Shifter				64
#define MaxThreshold			128
#define MOne				192
#define One				256
#define LargeX				320
#define Zero				384
#define Tbl_H				448
#define Pi2				576
#define coeff_1				640
#define coeff_2				704
#define coeff_3				768

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_atanf_skx)
	vandps	__svml_satan_data_internal_avx512(%rip), %zmm0, %zmm7
	vmovups	MaxThreshold+__svml_satan_data_internal_avx512(%rip), %zmm3
	vmovups	One+__svml_satan_data_internal_avx512(%rip), %zmm8

	/* round to 2 bits after binary point */
	vreduceps $40, {sae}, %zmm7, %zmm5

	/* saturate X range */
	vmovups	LargeX+__svml_satan_data_internal_avx512(%rip), %zmm6
	vmovups	Shifter+__svml_satan_data_internal_avx512(%rip), %zmm2
	vcmpps	$29, {sae}, %zmm3, %zmm7, %k1

	/* table lookup sequence */
	vmovups	Tbl_H+__svml_satan_data_internal_avx512(%rip), %zmm3
	vsubps	{rn-sae}, %zmm5, %zmm7, %zmm4
	vaddps	{rn-sae}, %zmm2, %zmm7, %zmm1
	vxorps	%zmm0, %zmm7, %zmm0
	vfmadd231ps {rn-sae}, %zmm7, %zmm4, %zmm8
	vmovups	coeff_2+__svml_satan_data_internal_avx512(%rip), %zmm4

	/* if|X|>=MaxThreshold, set DiffX=-1 */
	vblendmps MOne+__svml_satan_data_internal_avx512(%rip), %zmm5, %zmm9{%k1}
	vmovups	coeff_3+__svml_satan_data_internal_avx512(%rip), %zmm5

	/* if|X|>=MaxThreshold, set Y=X */
	vminps	{sae}, %zmm7, %zmm6, %zmm8{%k1}

	/* R+Rl = DiffX/Y */
	vgetmantps $0, {sae}, %zmm9, %zmm12
	vgetexpps {sae}, %zmm9, %zmm10
	vpermt2ps Tbl_H+64+__svml_satan_data_internal_avx512(%rip), %zmm1, %zmm3
	vgetmantps $0, {sae}, %zmm8, %zmm15
	vgetexpps {sae}, %zmm8, %zmm11
	vmovups	coeff_1+__svml_satan_data_internal_avx512(%rip), %zmm1

	/* set table value to Pi/2 for large X */
	vblendmps Pi2+__svml_satan_data_internal_avx512(%rip), %zmm3, %zmm9{%k1}
	vrcp14ps %zmm15, %zmm13
	vsubps	{rn-sae}, %zmm11, %zmm10, %zmm2
	vmulps	{rn-sae}, %zmm13, %zmm12, %zmm14
	vfnmadd213ps {rn-sae}, %zmm12, %zmm14, %zmm15
	vfmadd213ps {rn-sae}, %zmm14, %zmm13, %zmm15
	vscalefps {rn-sae}, %zmm2, %zmm15, %zmm7

	/* polynomial evaluation */
	vmulps	{rn-sae}, %zmm7, %zmm7, %zmm8
	vmulps	{rn-sae}, %zmm7, %zmm8, %zmm6
	vfmadd231ps {rn-sae}, %zmm8, %zmm1, %zmm4
	vfmadd213ps {rn-sae}, %zmm5, %zmm4, %zmm8
	vfmadd213ps {rn-sae}, %zmm7, %zmm6, %zmm8
	vaddps	{rn-sae}, %zmm9, %zmm8, %zmm10
	vxorps	%zmm0, %zmm10, %zmm0
	ret

END(_ZGVeN16v_atanf_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_satan_data_internal_avx512_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 AbsMask[16][1];
	__declspec(align(64)) VUINT32 Shifter[16][1];
	__declspec(align(64)) VUINT32 MaxThreshold[16][1];
	__declspec(align(64)) VUINT32 MOne[16][1];
	__declspec(align(64)) VUINT32 One[16][1];
	__declspec(align(64)) VUINT32 LargeX[16][1];
	__declspec(align(64)) VUINT32 Zero[16][1];
	__declspec(align(64)) VUINT32 Tbl_H[32][1];
	__declspec(align(64)) VUINT32 Pi2[16][1];
	__declspec(align(64)) VUINT32 coeff[3][16][1];
} __svml_satan_data_internal_avx512;
#endif
__svml_satan_data_internal_avx512:
	/* AbsMask */
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff
	/* Shifter */
	.align	64
	.long	0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000, 0x4a000000
	/* MaxThreshold */
	.align	64
	.long	0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000, 0x40F80000
	/* MOne */
	.align	64
	.long	0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000, 0xbf800000
	/* One */
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000
	/* LargeX */
	.align	64
	.long	0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000, 0x4f800000
	/* Zero */
	.align	64
	.long	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000
	/* Tbl_H */
	.align	64
	.long	0x00000000, 0x3e7adbb0
	.long	0x3eed6338, 0x3f24bc7d
	.long	0x3f490fdb, 0x3f6563e3
	.long	0x3f7b985f, 0x3f869c79
	.long	0x3f8db70d, 0x3f93877b
	.long	0x3f985b6c, 0x3f9c6b53
	.long	0x3f9fe0bb, 0x3fa2daa4
	.long	0x3fa57088, 0x3fa7b46f
	.long	0x3fa9b465, 0x3fab7b7a
	.long	0x3fad1283, 0x3fae809e
	.long	0x3fafcb99, 0x3fb0f836
	.long	0x3fb20a6a, 0x3fb30581
	.long	0x3fb3ec43, 0x3fb4c10a
	.long	0x3fb585d7, 0x3fb63c64
	.long	0x3fb6e62c, 0x3fb78478
	.long	0x3fb81868, 0x3fb8a2f5
	/* Pi2 */
	.align	64
	.long	0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB, 0x3fc90FDB
	/* coeff3 */
	.align	64
	.long	0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de, 0xbe0fa8de
	.long	0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2, 0x3e4cc8e2
	.long	0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa, 0xbeaaaaaa
	.align	64
	.type	__svml_satan_data_internal_avx512, @object
	.size	__svml_satan_data_internal_avx512, .-__svml_satan_data_internal_avx512
