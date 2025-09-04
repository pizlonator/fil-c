/* Function erff vectorized with AVX-512.
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
 *   erf(x) is computed as higher precision simple polynomial
 *   with no lookup table:
 *
 *     R = P0 + x^2*(P1 + x^2*(P2 + .... x^2*P12));
 *     erf(x) = R * R * x;
 *
 *   Special cases:
 *
 *   erf(0)    = 0
 *   erf(+INF) = +1
 *   erf(-INF) = -1
 *   erf(QNaN) = QNaN
 *   erf(SNaN) = QNaN
 *
 */

/* Offsets for data table __svml_serf_data_internal
 */
#define _AbsMask			0
#define _One				64
#define _gf_MaxThreshold_LA		128
#define _gf_la_poly_0			192
#define _gf_la_poly_1			256
#define _gf_la_poly_2			320
#define _gf_la_poly_3			384
#define _gf_la_poly_4			448
#define _gf_la_poly_5			512
#define _gf_la_poly_6			576
#define _gf_la_poly_7			640
#define _gf_la_poly_8			704
#define _gf_la_poly_9			768
#define _gf_la_poly_10			832
#define _gf_la_poly_11			896
#define _gf_la_poly_12			960

#include <sysdep.h>

	.section .text.evex512, "ax", @progbits
ENTRY(_ZGVeN16v_erff_skx)
	vmovaps	%zmm0, %zmm8
	vmulps	{rn-sae}, %zmm8, %zmm8, %zmm11
	vmovups	_gf_la_poly_11+__svml_serf_data_internal(%rip), %zmm15
	vmovups	_gf_la_poly_12+__svml_serf_data_internal(%rip), %zmm10
	vmovups	_gf_la_poly_10+__svml_serf_data_internal(%rip), %zmm9
	vmovups	_gf_la_poly_9+__svml_serf_data_internal(%rip), %zmm7
	vmovups	_gf_la_poly_8+__svml_serf_data_internal(%rip), %zmm0
	vmovups	_gf_la_poly_7+__svml_serf_data_internal(%rip), %zmm1
	vmovups	_gf_la_poly_6+__svml_serf_data_internal(%rip), %zmm2
	vmovups	_gf_la_poly_5+__svml_serf_data_internal(%rip), %zmm3
	vmovups	_gf_la_poly_4+__svml_serf_data_internal(%rip), %zmm4
	vmovups	_gf_la_poly_3+__svml_serf_data_internal(%rip), %zmm5
	vmovups	_gf_la_poly_2+__svml_serf_data_internal(%rip), %zmm6
	vextractf32x8 $1, %zmm8, %ymm13
	vcvtps2pd {sae}, %ymm8, %zmm12
	vcvtps2pd {sae}, %ymm13, %zmm14
	vmulpd	{rn-sae}, %zmm12, %zmm12, %zmm12
	vmulpd	{rn-sae}, %zmm14, %zmm14, %zmm13

	/* R = P0 + x^2*(P1 + x^2*(P2 + .... x^2*P12)); */
	vmovaps	%zmm15, %zmm14
	vfmadd231pd {rn-sae}, %zmm12, %zmm10, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm10, %zmm15
	vmovups	_gf_la_poly_1+__svml_serf_data_internal(%rip), %zmm10
	vfmadd213pd {rn-sae}, %zmm9, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm15, %zmm9
	vfmadd213pd {rn-sae}, %zmm7, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm9, %zmm7
	vfmadd213pd {rn-sae}, %zmm0, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm7, %zmm0
	vmovups	_gf_MaxThreshold_LA+__svml_serf_data_internal(%rip), %zmm7
	vfmadd213pd {rn-sae}, %zmm1, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm0, %zmm1
	vmovups	_gf_la_poly_0+__svml_serf_data_internal(%rip), %zmm0
	vcmpps	$22, {sae}, %zmm11, %zmm7, %k1
	vfmadd213pd {rn-sae}, %zmm2, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm1, %zmm2
	vfmadd213pd {rn-sae}, %zmm3, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm2, %zmm3
	vfmadd213pd {rn-sae}, %zmm4, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm3, %zmm4
	vfmadd213pd {rn-sae}, %zmm5, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm4, %zmm5
	vfmadd213pd {rn-sae}, %zmm6, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm5, %zmm6
	vmovups	_AbsMask+__svml_serf_data_internal(%rip), %zmm5
	vfmadd213pd {rn-sae}, %zmm10, %zmm12, %zmm14
	vfmadd231pd {rn-sae}, %zmm13, %zmm6, %zmm10
	vandnps	%zmm8, %zmm5, %zmm6
	vfmadd213pd {rn-sae}, %zmm0, %zmm14, %zmm12
	vfmadd213pd {rn-sae}, %zmm0, %zmm10, %zmm13
	vorps	_One+__svml_serf_data_internal(%rip), %zmm6, %zmm0
	vmulpd	{rn-sae}, %zmm12, %zmm12, %zmm1
	vmulpd	{rn-sae}, %zmm13, %zmm13, %zmm3
	vcvtpd2ps {rn-sae}, %zmm1, %ymm2
	vcvtpd2ps {rn-sae}, %zmm3, %ymm4
	vinsertf32x8 $1, %ymm4, %zmm2, %zmm9

	/* erf(x) = R * R * x; */
	vmulps	{rn-sae}, %zmm8, %zmm9, %zmm0{%k1}
	ret

END(_ZGVeN16v_erff_skx)

	.section .rodata, "a"
	.align	64

#ifdef __svml_serf_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(64)) VUINT32 _AbsMask[16][1];
	__declspec(align(64)) VUINT32 _One[16][1];
	__declspec(align(64)) VUINT32 _gf_MaxThreshold_LA[16][1];
	__declspec(align(64)) VUINT32 _gf_la_poly_0[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_1[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_2[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_3[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_4[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_5[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_6[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_7[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_8[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_9[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_10[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_11[8][2];
	__declspec(align(64)) VUINT32 _gf_la_poly_12[8][2];
} __svml_serf_data_internal;
#endif
__svml_serf_data_internal:
	.long	0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff /* _AbsMask */
	.align	64
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 /* _One */
	.align	64
	.long	0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a, 0x41558c5a /* _gf_MaxThreshold_LA */
	.align	64
	.quad	0x3ff0fefbd933b903, 0x3ff0fefbd933b903, 0x3ff0fefbd933b903, 0x3ff0fefbd933b903, 0x3ff0fefbd933b903, 0x3ff0fefbd933b903, 0x3ff0fefbd933b903, 0x3ff0fefbd933b903 /* _gf_la_poly_0 */
	.align	64
	.quad	0xbfc6a948101e6367, 0xbfc6a948101e6367, 0xbfc6a948101e6367, 0xbfc6a948101e6367, 0xbfc6a948101e6367, 0xbfc6a948101e6367, 0xbfc6a948101e6367, 0xbfc6a948101e6367 /* _gf_la_poly_1 */
	.align	64
	.quad	0x3fa3a334ce602c6b, 0x3fa3a334ce602c6b, 0x3fa3a334ce602c6b, 0x3fa3a334ce602c6b, 0x3fa3a334ce602c6b, 0x3fa3a334ce602c6b, 0x3fa3a334ce602c6b, 0x3fa3a334ce602c6b /* _gf_la_poly_2 */
	.align	64
	.quad	0xbf799309ea0c81dc, 0xbf799309ea0c81dc, 0xbf799309ea0c81dc, 0xbf799309ea0c81dc, 0xbf799309ea0c81dc, 0xbf799309ea0c81dc, 0xbf799309ea0c81dc, 0xbf799309ea0c81dc /* _gf_la_poly_3 */
	.align	64
	.quad	0x3f476df64a40e392, 0x3f476df64a40e392, 0x3f476df64a40e392, 0x3f476df64a40e392, 0x3f476df64a40e392, 0x3f476df64a40e392, 0x3f476df64a40e392, 0x3f476df64a40e392 /* _gf_la_poly_4 */
	.align	64
	.quad	0xbf0a5216b9508ede, 0xbf0a5216b9508ede, 0xbf0a5216b9508ede, 0xbf0a5216b9508ede, 0xbf0a5216b9508ede, 0xbf0a5216b9508ede, 0xbf0a5216b9508ede, 0xbf0a5216b9508ede /* _gf_la_poly_5 */
	.align	64
	.quad	0x3ea5794b95c8e8a0, 0x3ea5794b95c8e8a0, 0x3ea5794b95c8e8a0, 0x3ea5794b95c8e8a0, 0x3ea5794b95c8e8a0, 0x3ea5794b95c8e8a0, 0x3ea5794b95c8e8a0, 0x3ea5794b95c8e8a0 /* _gf_la_poly_6 */
	.align	64
	.quad	0x3e94b6c0b485f30f, 0x3e94b6c0b485f30f, 0x3e94b6c0b485f30f, 0x3e94b6c0b485f30f, 0x3e94b6c0b485f30f, 0x3e94b6c0b485f30f, 0x3e94b6c0b485f30f, 0x3e94b6c0b485f30f /* _gf_la_poly_7 */
	.align	64
	.quad	0xbe65806ce17f0523, 0xbe65806ce17f0523, 0xbe65806ce17f0523, 0xbe65806ce17f0523, 0xbe65806ce17f0523, 0xbe65806ce17f0523, 0xbe65806ce17f0523, 0xbe65806ce17f0523 /* _gf_la_poly_8 */
	.align	64
	.quad	0x3e2715640470db47, 0x3e2715640470db47, 0x3e2715640470db47, 0x3e2715640470db47, 0x3e2715640470db47, 0x3e2715640470db47, 0x3e2715640470db47, 0x3e2715640470db47 /* _gf_la_poly_9 */
	.align	64
	.quad	0xbdddcb2653d80f03, 0xbdddcb2653d80f03, 0xbdddcb2653d80f03, 0xbdddcb2653d80f03, 0xbdddcb2653d80f03, 0xbdddcb2653d80f03, 0xbdddcb2653d80f03, 0xbdddcb2653d80f03 /* _gf_la_poly_10 */
	.align	64
	.quad	0x3d85eadfc762d3eb, 0x3d85eadfc762d3eb, 0x3d85eadfc762d3eb, 0x3d85eadfc762d3eb, 0x3d85eadfc762d3eb, 0x3d85eadfc762d3eb, 0x3d85eadfc762d3eb, 0x3d85eadfc762d3eb /* _gf_la_poly_11 */
	.align	64
	.quad	0xbd1c668a2871f0f1, 0xbd1c668a2871f0f1, 0xbd1c668a2871f0f1, 0xbd1c668a2871f0f1, 0xbd1c668a2871f0f1, 0xbd1c668a2871f0f1, 0xbd1c668a2871f0f1, 0xbd1c668a2871f0f1 /* _gf_la_poly_12 */
	.align	64
	.type	__svml_serf_data_internal, @object
	.size	__svml_serf_data_internal, .-__svml_serf_data_internal
