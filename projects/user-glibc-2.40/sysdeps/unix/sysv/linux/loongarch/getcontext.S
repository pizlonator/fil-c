/* Save current context.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include "ucontext-macros.h"

/* int getcontext (ucontext_t *ucp) */

	.text
LEAF (__getcontext)
	SAVE_INT_REG (ra,   1, a0)
	SAVE_INT_REG (sp,   3, a0)
	SAVE_INT_REG (zero, 4, a0) /* return 0 by overwriting a0.  */
	SAVE_INT_REG (x,   21, a0)
	SAVE_INT_REG (fp,  22, a0)
	SAVE_INT_REG (s0,  23, a0)
	SAVE_INT_REG (s1,  24, a0)
	SAVE_INT_REG (s2,  25, a0)
	SAVE_INT_REG (s3,  26, a0)
	SAVE_INT_REG (s4,  27, a0)
	SAVE_INT_REG (s5,  28, a0)
	SAVE_INT_REG (s6,  29, a0)
	SAVE_INT_REG (s7,  30, a0)
	SAVE_INT_REG (s8,  31, a0)
	st.d		ra, a0, MCONTEXT_PC

/* rt_sigprocmask (SIG_BLOCK, NULL, &ucp->uc_sigmask, _NSIG8) */
	li.d		a3, _NSIG8
	li.d		a2, UCONTEXT_SIGMASK
	add.d		a2, a2, a0
	ori		a1, zero,0
	li.d		a0, SIG_BLOCK

	li.d		a7, SYS_ify (rt_sigprocmask)
	syscall		0
	blt		a0, zero, 99f

	jirl		$r0, $r1, 0

99:
	b		__syscall_error

PSEUDO_END (__getcontext)

weak_alias (__getcontext, getcontext)
