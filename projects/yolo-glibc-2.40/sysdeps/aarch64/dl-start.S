/* ld.so _start code.
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

#include <sysdep.h>

ENTRY (_start)
	/* Create an initial frame with 0 LR and FP */
	cfi_undefined (x30)
	mov	x29, #0
	mov	x30, #0

	mov	x0, sp
	PTR_ARG (0)
	bl	_dl_start
	/* Returns user entry point in x0.  */
	mov	PTR_REG (21), PTR_REG (0)
.globl _dl_start_user
.type _dl_start_user, %function
_dl_start_user:
	/* Get argc.  */
	ldr	PTR_REG (1), [sp]
	/* Get argv.  */
	add	x2, sp, PTR_SIZE
	/* Compute envp.  */
	add	PTR_REG (3), PTR_REG (2), PTR_REG (1), lsl PTR_LOG_SIZE
	add	PTR_REG (3), PTR_REG (3), PTR_SIZE
	adrp	x16, _rtld_local
	add	PTR_REG (16), PTR_REG (16), :lo12:_rtld_local
	ldr	PTR_REG (0), [x16]
	bl	_dl_init
	/* Load the finalizer function.  */
	adrp	x0, _dl_fini
	add	PTR_REG (0), PTR_REG (0), :lo12:_dl_fini
	/* Jump to the user's entry point.  */
	mov     x16, x21
	br      x16
END (_start)
