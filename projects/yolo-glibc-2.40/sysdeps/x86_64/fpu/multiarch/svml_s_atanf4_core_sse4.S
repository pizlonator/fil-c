/* Function atanf vectorized with SSE4.
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

/* Offsets for data table __svml_satan_data_internal
 */
#define _sSIGN_MASK			0
#define _sABS_MASK			16
#define _sONE				32
#define _sPIO2				48
#define _sPC8				64
#define _sPC7				80
#define _sPC6				96
#define _sPC5				112
#define _sPC4				128
#define _sPC3				144
#define _sPC2				160
#define _sPC1				176
#define _sPC0				192

#include <sysdep.h>

	.section .text.sse4, "ax", @progbits
ENTRY(_ZGVbN4v_atanf_sse4)
	/*
	 * To use minps\maxps operations for argument reduction
	 * uncomment _AT_USEMINMAX_ definition
	 *  Declarations
	 * Variables
	 * Constants
	 */
	movups	_sABS_MASK+__svml_satan_data_internal(%rip), %xmm2

	/*
	 * 1) If x>1,      then r=-1/x, PIO2=Pi/2
	 * 2) If -1<=x<=1, then r=x,    PIO2=0
	 * 3) If x<-1,     then r=-1/x, PIO2=-Pi/2
	 */
	movups	_sONE+__svml_satan_data_internal(%rip), %xmm1
	andps	%xmm0, %xmm2
	movaps	%xmm2, %xmm9
	movaps	%xmm1, %xmm3
	cmpleps	%xmm1, %xmm9
	maxps	%xmm2, %xmm3
	minps	%xmm2, %xmm1
	divps	%xmm3, %xmm1
	movups	__svml_satan_data_internal(%rip), %xmm4
	movaps	%xmm9, %xmm10
	andps	%xmm4, %xmm0
	andnps	%xmm4, %xmm9
	pxor	%xmm0, %xmm9
	pxor	%xmm1, %xmm9

	/* Polynomial. */
	movaps	%xmm9, %xmm8
	mulps	%xmm9, %xmm8
	movaps	%xmm8, %xmm7
	mulps	%xmm8, %xmm7
	movups	_sPC8+__svml_satan_data_internal(%rip), %xmm6
	mulps	%xmm7, %xmm6
	movups	_sPC7+__svml_satan_data_internal(%rip), %xmm5
	mulps	%xmm7, %xmm5
	addps	_sPC6+__svml_satan_data_internal(%rip), %xmm6
	mulps	%xmm7, %xmm6
	addps	_sPC5+__svml_satan_data_internal(%rip), %xmm5
	mulps	%xmm7, %xmm5
	addps	_sPC4+__svml_satan_data_internal(%rip), %xmm6
	mulps	%xmm7, %xmm6
	addps	_sPC3+__svml_satan_data_internal(%rip), %xmm5
	mulps	%xmm5, %xmm7
	addps	_sPC2+__svml_satan_data_internal(%rip), %xmm6
	mulps	%xmm8, %xmm6
	addps	_sPC1+__svml_satan_data_internal(%rip), %xmm7
	andnps	_sPIO2+__svml_satan_data_internal(%rip), %xmm10
	addps	%xmm6, %xmm7
	mulps	%xmm7, %xmm8
	pxor	%xmm0, %xmm10
	addps	_sPC0+__svml_satan_data_internal(%rip), %xmm8

	/* Reconstruction. */
	mulps	%xmm8, %xmm9
	addps	%xmm9, %xmm10
	movaps	%xmm10, %xmm0
	ret

END(_ZGVbN4v_atanf_sse4)

	.section .rodata, "a"
	.align	16

#ifdef __svml_satan_data_internal_typedef
typedef unsigned int VUINT32;
typedef struct {
	__declspec(align(16)) VUINT32 _sSIGN_MASK[4][1];
	__declspec(align(16)) VUINT32 _sABS_MASK[4][1];
	__declspec(align(16)) VUINT32 _sONE[4][1];
	__declspec(align(16)) VUINT32 _sPIO2[4][1];
	__declspec(align(16)) VUINT32 _sPC8[4][1];
	__declspec(align(16)) VUINT32 _sPC7[4][1];
	__declspec(align(16)) VUINT32 _sPC6[4][1];
	__declspec(align(16)) VUINT32 _sPC5[4][1];
	__declspec(align(16)) VUINT32 _sPC4[4][1];
	__declspec(align(16)) VUINT32 _sPC3[4][1];
	__declspec(align(16)) VUINT32 _sPC2[4][1];
	__declspec(align(16)) VUINT32 _sPC1[4][1];
	__declspec(align(16)) VUINT32 _sPC0[4][1];
} __svml_satan_data_internal;
#endif
__svml_satan_data_internal:
	.long	0x80000000, 0x80000000, 0x80000000, 0x80000000 // _sSIGN_MASK
	.align	16
	.long	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF // _sABS_MASK
	.align	16
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 // _sONE
	.align	16
	.long	0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB, 0x3FC90FDB // _sPIO2
	.align	16
	.long	0x3B322CC0, 0x3B322CC0, 0x3B322CC0, 0x3B322CC0 // _sPC8
	.align	16
	.long	0xBC7F2631, 0xBC7F2631, 0xBC7F2631, 0xBC7F2631 // _sPC7
	.align	16
	.long	0x3D2BC384, 0x3D2BC384, 0x3D2BC384, 0x3D2BC384 // _sPC6
	.align	16
	.long	0xBD987629, 0xBD987629, 0xBD987629, 0xBD987629 // _sPC5
	.align	16
	.long	0x3DD96474, 0x3DD96474, 0x3DD96474, 0x3DD96474 // _sPC4
	.align	16
	.long	0xBE1161F8, 0xBE1161F8, 0xBE1161F8, 0xBE1161F8 // _sPC3
	.align	16
	.long	0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F, 0x3E4CB79F // _sPC2
	.align	16
	.long	0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49, 0xBEAAAA49 // _sPC1
	.align	16
	.long	0x3f800000, 0x3f800000, 0x3f800000, 0x3f800000 // _sPC0
	.align	16
	.type	__svml_satan_data_internal, @object
	.size	__svml_satan_data_internal, .-__svml_satan_data_internal
