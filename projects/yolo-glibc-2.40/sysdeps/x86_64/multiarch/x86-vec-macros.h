/* Macro helpers for VEC_{type}({vec_num})
   All versions must be listed in ifunc-impl-list.c.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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
   <https://www.gnu.org/licenses/>.  */

#ifndef _X86_VEC_MACROS_H
#define _X86_VEC_MACROS_H			1

#ifndef VEC_SIZE
# error "Never include this file directly. Always include a vector config."
#endif

/* Defines so we can use SSE2 / AVX2 / EVEX / EVEX512 encoding with same
   VMM(N) values.  */
#define VMM_hi_xmm0				xmm16
#define VMM_hi_xmm1				xmm17
#define VMM_hi_xmm2				xmm18
#define VMM_hi_xmm3				xmm19
#define VMM_hi_xmm4				xmm20
#define VMM_hi_xmm5				xmm21
#define VMM_hi_xmm6				xmm22
#define VMM_hi_xmm7				xmm23
#define VMM_hi_xmm8				xmm24
#define VMM_hi_xmm9				xmm25
#define VMM_hi_xmm10			xmm26
#define VMM_hi_xmm11			xmm27
#define VMM_hi_xmm12			xmm28
#define VMM_hi_xmm13			xmm29
#define VMM_hi_xmm14			xmm30
#define VMM_hi_xmm15			xmm31

#define VMM_hi_ymm0				ymm16
#define VMM_hi_ymm1				ymm17
#define VMM_hi_ymm2				ymm18
#define VMM_hi_ymm3				ymm19
#define VMM_hi_ymm4				ymm20
#define VMM_hi_ymm5				ymm21
#define VMM_hi_ymm6				ymm22
#define VMM_hi_ymm7				ymm23
#define VMM_hi_ymm8				ymm24
#define VMM_hi_ymm9				ymm25
#define VMM_hi_ymm10			ymm26
#define VMM_hi_ymm11			ymm27
#define VMM_hi_ymm12			ymm28
#define VMM_hi_ymm13			ymm29
#define VMM_hi_ymm14			ymm30
#define VMM_hi_ymm15			ymm31

#define VMM_hi_zmm0				zmm16
#define VMM_hi_zmm1				zmm17
#define VMM_hi_zmm2				zmm18
#define VMM_hi_zmm3				zmm19
#define VMM_hi_zmm4				zmm20
#define VMM_hi_zmm5				zmm21
#define VMM_hi_zmm6				zmm22
#define VMM_hi_zmm7				zmm23
#define VMM_hi_zmm8				zmm24
#define VMM_hi_zmm9				zmm25
#define VMM_hi_zmm10			zmm26
#define VMM_hi_zmm11			zmm27
#define VMM_hi_zmm12			zmm28
#define VMM_hi_zmm13			zmm29
#define VMM_hi_zmm14			zmm30
#define VMM_hi_zmm15			zmm31

#define PRIMITIVE_VMM(vec, num)		vec##num

#define VMM_any_xmm(i)			PRIMITIVE_VMM(xmm, i)
#define VMM_any_ymm(i)			PRIMITIVE_VMM(ymm, i)
#define VMM_any_zmm(i)			PRIMITIVE_VMM(zmm, i)

#define VMM_hi_xmm(i)			PRIMITIVE_VMM(VMM_hi_xmm, i)
#define VMM_hi_ymm(i)			PRIMITIVE_VMM(VMM_hi_ymm, i)
#define VMM_hi_zmm(i)			PRIMITIVE_VMM(VMM_hi_zmm, i)

#endif
