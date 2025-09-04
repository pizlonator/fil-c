/* Copyright (C) 2012-2024 Free Software Foundation, Inc.
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
#include <shlib-compat.h>

ENTRY(__pthread_spin_lock)
	/* Always return zero.  */
	xor	%eax, %eax
	LOCK
	decl	0(%rdi)
	jne	1f
	ret

	.align	16
1:
	/* `rep nop` == `pause`.  */
	rep
	nop
	cmpl	%eax, 0(%rdi)
	jle	1b
	/* Just repeat the `lock decl` logic here.  The code size save
	   of jumping back to entry doesn't change how many 16-byte
	   chunks (default function alignment) that the code fits in.  */
	LOCK
	decl	0(%rdi)
	jne	1b
	ret
END(__pthread_spin_lock)
versioned_symbol (libc, __pthread_spin_lock, pthread_spin_lock, GLIBC_2_34)

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_2, GLIBC_2_34)
compat_symbol (libpthread, __pthread_spin_lock, pthread_spin_lock, GLIBC_2_2)
#endif
