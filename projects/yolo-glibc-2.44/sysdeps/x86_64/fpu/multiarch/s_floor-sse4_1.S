/* Copyright (C) 2011-2026 Free Software Foundation, Inc.
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

#include <sysdep.h>

#include <sysdeps/x86/isa-level.h>
#if MINIMUM_X86_ISA_LEVEL == SSE4_1_X86_ISA_LEVEL
# include <libm-alias-double.h>
# define __floor_sse41 __floor
	.text
#else
	.section .text.sse4.1,"ax",@progbits
#endif

ENTRY(__floor_sse41)
	roundsd	$9, %xmm0, %xmm0
	ret
END(__floor_sse41)

#if MINIMUM_X86_ISA_LEVEL == SSE4_1_X86_ISA_LEVEL
libm_alias_double (__floor, floor)
#endif
