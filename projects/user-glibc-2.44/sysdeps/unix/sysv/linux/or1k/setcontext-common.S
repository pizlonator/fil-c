/* Set current context.  OpenRISC common version.
   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

/* This common setcontext and startcontext template helps define
   different implementations using control macros.  Before including
   this file in another file define the following:

     __CONTEXT_FUNC_NAME
     __CONTEXT_ENABLE_FPCSR
     __CONTEXT_SIGMASK_OFFSET
     __STARTCONTEXT_FUNC_NAME
 */

/* int setcontext (const ucontext_t *ucp) */
	.text
ENTRY(__CONTEXT_FUNC_NAME)
	l.ori	r30, r3, 0

	/* Restore signal mask.  */
	/* rt_sigprocmask (SIG_SETMASK, &ucp->uc_sigmask, NULL, _NSIG8) */
	l.ori	r6, r0, _NSIG8
	l.ori	r5, r0, 0
	l.addi	r4, r3, __CONTEXT_SIGMASK_OFFSET
	l.ori	r3, r0, SIG_SETMASK
	l.ori	r11, r0, SYS_ify (rt_sigprocmask)
	/* Do the syscall.  */
	l.sys 1
	 l.nop

	/* if -4096 < ret < 0 holds, it's an error */
	l.sfgeui r11, 0xf001
	l.bf	1f
	 l.nop
#ifdef __CONTEXT_ENABLE_FPCSR
# ifdef __or1k_hard_float__
	/* Restore the floating point state.  */
	l.lwz	r28, (MCONTEXT_FPCSR)(r30)
	l.mtspr	r0, r28, 20
# endif /* __or1k_hard_float__ */
#endif /* __CONTEXT_ENABLE_FPCSR */
	/* Restore argument registers, for the makecontext case.  */
	l.lwz	r3, (UCONTEXT_MCONTEXT +  3*4)(r30)
	l.lwz	r4, (UCONTEXT_MCONTEXT +  4*4)(r30)
	l.lwz	r5, (UCONTEXT_MCONTEXT +  5*4)(r30)
	l.lwz	r6, (UCONTEXT_MCONTEXT +  6*4)(r30)
	l.lwz	r7, (UCONTEXT_MCONTEXT +  7*4)(r30)
	l.lwz	r8, (UCONTEXT_MCONTEXT +  8*4)(r30)

	/* Restore registers stored in getcontext.  */
	l.lwz	r1,  (UCONTEXT_MCONTEXT +  1*4)(r30)
	l.lwz	r2,  (UCONTEXT_MCONTEXT +  2*4)(r30)
	l.lwz	r9,  (UCONTEXT_MCONTEXT +  9*4)(r30)
	l.lwz	r10, (UCONTEXT_MCONTEXT + 10*4)(r30)
	l.lwz	r11, (UCONTEXT_MCONTEXT + 11*4)(r30)
	/* Restore r14-r30 even, callee saved registers.  */
	l.lwz	r14, (UCONTEXT_MCONTEXT + 14*4)(r30)
	l.lwz	r16, (UCONTEXT_MCONTEXT + 16*4)(r30)
	l.lwz	r18, (UCONTEXT_MCONTEXT + 18*4)(r30)
	l.lwz	r20, (UCONTEXT_MCONTEXT + 20*4)(r30)
	l.lwz	r22, (UCONTEXT_MCONTEXT + 22*4)(r30)
	l.lwz	r24, (UCONTEXT_MCONTEXT + 24*4)(r30)
	l.lwz	r26, (UCONTEXT_MCONTEXT + 26*4)(r30)
	l.lwz	r28, (UCONTEXT_MCONTEXT + 28*4)(r30)
	l.lwz	r30, (UCONTEXT_MCONTEXT + 30*4)(r30)

	l.jr	r11
	 l.ori	r11, r0, 0

1:	l.j	__syscall_error
	 l.ori	r3, r11, 0

END (__CONTEXT_FUNC_NAME)

	/* We add a NOP here because when the unwinder is looking for the
	   enclosing function of the link register (r9) address FDE lookup will
	   use '$r9 - 1' finding setcontext which is wrong.  This is because in
	   makecontext we have set r9 to the start of &__startcontext.

	   If this NOP did not exist the unwinder would repeatedly find
	   __setcontext's FDE in an infinite loop.  Modifying/deleting the below
	   __startcontext's FDE has no help on this.  */
	l.nop

ENTRY(__STARTCONTEXT_FUNC_NAME)

	l.ori	r3, r14, 0
	l.sfeq	r3, r0
	/* If uc_link is not 0 resume there, otherwise exit.  */
	l.bnf	__CONTEXT_FUNC_NAME
	 l.nop

#ifdef SHARED
	/* Obtain a pointer to .got in r16 */
	l.jal	0x8
	 l.movhi r16, gotpchi(_GLOBAL_OFFSET_TABLE_-4)
	l.ori	r16, r16, gotpclo(_GLOBAL_OFFSET_TABLE_+0)
	l.add	r16, r16, r9
	l.lwz	r16, got(exit)(r16)
	l.jr	r16
#else
	l.j	exit
#endif
	 l.nop

END(__STARTCONTEXT_FUNC_NAME)
