/* Copyright (C) 2002-2024 Free Software Foundation, Inc.
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
#include <errno.h>
#include <shlib-compat.h>

ENTRY(__pthread_spin_trylock)
	xorl	%ecx, %ecx
	/* xchg has implicit LOCK prefix.  */
	xchgl	%ecx, (%rdi)

	/* Branch on result.  Expectation is the use of trylock will be
	   branching on success/failure so this branch can be used to
	   to predict the coming branch.  It has the benefit of
	   breaking the likely expensive memory dependency on (%rdi).  */
	cmpl	$1, %ecx
	jnz	1f
	xorl	%eax, %eax
	ret
1:
	movl	$EBUSY, %eax
	ret
END(__pthread_spin_trylock)
versioned_symbol (libc, __pthread_spin_trylock, pthread_spin_trylock,
		  GLIBC_2_34)

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_2, GLIBC_2_34)
compat_symbol (libpthread, __pthread_spin_trylock, pthread_spin_trylock,
	       GLIBC_2_2)
#endif
