/* Define symbols used to communicate dynamic linker state to the
   debugger at runtime.
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
   <https://www.gnu.org/licenses/>.  */

#include <rtld-sizes.h>
#include <sysdep.h>

/* Some targets define a macro to denote the zero register.  */
#undef zero

/* Define 2 symbols, _r_debug_extended and _r_debug, which is an alias
   of _r_debug_extended, but with the size of struct r_debug.  */

	.globl	_r_debug
	.type	_r_debug, %object
	.size	_r_debug, R_DEBUG_SIZE
	.hidden	_r_debug_extended
	.globl	_r_debug_extended
	.type	_r_debug_extended, %object
	.size	_r_debug_extended, R_DEBUG_EXTENDED_SIZE
	.section .bss
	.balign	R_DEBUG_EXTENDED_ALIGN
_r_debug:
_r_debug_extended:
	.zero	R_DEBUG_EXTENDED_SIZE
