/* Set current context.
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
#include "sys/regdef.h"
#include "ucontext-macros.h"

/*  int __setcontext (const ucontext_t *ucp)

  Restores the machine context in UCP and thereby resumes execution
  in that context.

  This implementation is intended to be used for *synchronous* context
  switches only.  Therefore, it does not have to restore anything
  other than the PRESERVED state.  */

	.text
LEAF (__setcontext)

	addi.d		sp, sp, -16
	st.d		a0, sp, 0	/* Save ucp to stack */

/* rt_sigprocmask (SIG_SETMASK, &ucp->uc_sigmask, NULL, _NSIG8) */
	li.d		a3, _NSIG8
	li.d		a2, 0
	li.d		a1, UCONTEXT_SIGMASK
	add.d		a1, a1, a0
	li.d		a0, SIG_SETMASK

	li.d		a7, SYS_ify (rt_sigprocmask)
	syscall		0

	blt		a0, $r0, 99f

	ld.d		t0, sp, 0	/* Load ucp to t0 */
	cfi_def_cfa (12, 0)

/* Note the contents of argument registers will be random
   unless makecontext() has been called.  */
	RESTORE_INT_REG(ra,   1, t0)
	RESTORE_INT_REG(sp,   3, t0)
	RESTORE_INT_REG(a0,   4, t0)
	RESTORE_INT_REG(a1,   5, t0)
	RESTORE_INT_REG(a2,   6, t0)
	RESTORE_INT_REG(a3,   7, t0)
	RESTORE_INT_REG(a4,   8, t0)
	RESTORE_INT_REG(a5,   9, t0)
	RESTORE_INT_REG(a6,  10, t0)
	RESTORE_INT_REG(a7,  11, t0)
	RESTORE_INT_REG(x,   21, t0)
	RESTORE_INT_REG(fp,  22, t0)
	RESTORE_INT_REG(s0,  23, t0)
	RESTORE_INT_REG(s1,  24, t0)
	RESTORE_INT_REG(s2,  25, t0)
	RESTORE_INT_REG(s3,  26, t0)
	RESTORE_INT_REG(s4,  27, t0)
	RESTORE_INT_REG(s5,  28, t0)
	RESTORE_INT_REG(s6,  29, t0)
	RESTORE_INT_REG(s7,  30, t0)
	RESTORE_INT_REG(s8,  31, t0)

	ld.d		t1, t0, MCONTEXT_PC
	jirl		$r0,t1,0

99:
	addi.d		sp, sp, 16
	b		__syscall_error

PSEUDO_END (__setcontext)
weak_alias (__setcontext, setcontext)

LEAF (__start_context)

	/* Terminate call stack by noting ra == 0.  Happily, s0 == 0 here.  */
	cfi_register (1, 23)

	/* Call the function passed to makecontext.  */
	jirl		$r1,s1,0

	/* Invoke subsequent context if present, else exit(0).  */
	ori		a0, s2, 0
	beqz		s2, 1f
	bl		__setcontext
1:
	b		HIDDEN_JUMPTARGET(exit)

PSEUDO_END (__start_context)
