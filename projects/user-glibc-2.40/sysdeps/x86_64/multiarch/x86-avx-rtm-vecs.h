/* Common config for AVX-RTM VECs
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

#ifndef _X86_AVX_RTM_VECS_H
#define _X86_AVX_RTM_VECS_H			1

#define COND_VZEROUPPER			COND_VZEROUPPER_XTEST
#define ZERO_UPPER_VEC_REGISTERS_RETURN	\
	ZERO_UPPER_VEC_REGISTERS_RETURN_XTEST

#define VZEROUPPER_RETURN		jmp L(return_vzeroupper)

#define USE_WITH_RTM			1
#include "x86-avx-vecs.h"

#undef SECTION
#define SECTION(p)				p##.avx.rtm

#endif
