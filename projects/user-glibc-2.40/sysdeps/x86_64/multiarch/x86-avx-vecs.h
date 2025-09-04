/* Common config for AVX VECs
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

#ifndef _X86_AVX_VECS_H
#define _X86_AVX_VECS_H			1

#ifdef VEC_SIZE
# error "Multiple VEC configs included!"
#endif

#define VEC_SIZE			32
#include "x86-vec-macros.h"

#define USE_WITH_AVX2		1

#define SECTION(p)			p##.avx

/* 4-byte mov instructions with AVX2.  */
#define MOV_SIZE			4
/* 1 (ret) + 3 (vzeroupper).  */
#define RET_SIZE			4
#define VZEROUPPER			vzeroupper

#define VMOVU				vmovdqu
#define VMOVA				vmovdqa
#define VMOVNT				vmovntdq

/* Often need to access xmm portion.  */
#define VMM_128				VMM_any_xmm
#define VMM					VMM_any_ymm

#endif
