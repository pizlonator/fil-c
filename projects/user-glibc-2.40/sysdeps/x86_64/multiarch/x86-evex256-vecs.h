/* Common config for EVEX256 VECs
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

#ifndef _EVEX256_VECS_H
#define _EVEX256_VECS_H			1

#ifdef VEC_SIZE
# error "Multiple VEC configs included!"
#endif

#define VEC_SIZE			32
#include "x86-evex-vecs-common.h"

#define USE_WITH_EVEX256	1

#ifndef SECTION
# define SECTION(p)			p##.evex
#endif

#define VMM					VMM_256
#define VMM_lo				VMM_any_ymm
#endif
