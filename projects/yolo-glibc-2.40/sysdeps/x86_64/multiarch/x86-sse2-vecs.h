/* Common config for SSE2 VECs
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

#ifndef _X86_SSE2_VECS_H
#define _X86_SSE2_VECS_H			1

#ifdef VEC_SIZE
# error "Multiple VEC configs included!"
#endif

#define VEC_SIZE			16
#include "x86-vec-macros.h"

#define USE_WITH_SSE2		1
#define SECTION(p)			p

/* 3-byte mov instructions with SSE2.  */
#define MOV_SIZE			3
/* No vzeroupper needed.  */
#define RET_SIZE			1
#define VZEROUPPER

#define VMOVU				movups
#define VMOVA				movaps
#define VMOVNT				movntdq

#define VMM_128				VMM_any_xmm
#define VMM					VMM_any_xmm


#endif
