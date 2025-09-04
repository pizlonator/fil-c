/* clone() implementation for ARC.
   Copyright (C) 2020-2024 Free Software Foundation, Inc.
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
#define _ERRNO_H	1
#include <bits/errno.h>

/* int clone(int (*fn)(void *), void *child_stack,
           int flags, void *arg, ...
           < pid_t *ptid, struct user_desc *tls, pid_t *ctid > );

 NOTE: I'm assuming that the last 3 args are NOT var-args and in case all
	3 are not relevant, caller will nevertheless pass those as NULL.

 clone syscall in kernel (ABI: CONFIG_CLONE_BACKWARDS)

  int sys_clone(unsigned long int clone_flags,
	        unsigned long int newsp,
		int __user *parent_tidptr,
		void *tls,
		int __user *child_tidptr).  */

ENTRY (__clone)
	cmp	r0, 0		/* @fn can't be NULL.  */
	and	r1,r1,-4	/* @child_stack be 4 bytes aligned per ABI.  */
	cmp.ne	r1, 0		/* @child_stack can't be NULL.  */
	bz	L (__sys_err)

	/* save some of the orig args
	   r0 containing @fn will be clobbered AFTER syscall (with ret val)
	   rest are clobbered BEFORE syscall due to different arg ordering.  */
	mov	r10, r0		/* @fn.  */
	mov	r11, r3		/* @args.  */
	mov	r12, r2		/* @clone_flags.  */
	mov	r9,  r5		/* @tls.  */

	/* adjust libc args for syscall.  */

	mov 	r0, r2		/* libc @flags is 1st syscall arg.  */
	mov	r2, r4		/* libc @ptid.  */
	mov	r3, r5		/* libc @tls.  */
	mov	r4, r6		/* libc @ctid.  */
	mov	r8, __NR_clone
	ARC_TRAP_INSN

	cmp	r0, 0		/* return code : 0 new process, !0 parent.  */
	beq	thread_start_clone
	blt	L (__sys_err2)	/* < 0 (signed) error.  */
	j	[blink]		/* Parent returns.  */

L (__sys_err):
	mov	r0, -EINVAL
L (__sys_err2):
	/* (1) No need to make -ve kernel error code as positive errno
	       __syscall_error expects the -ve error code returned by kernel
	   (2) r0 still had orig -ve kernel error code
	   (3) Tail call to __syscall_error so we dont have to come back
	       here hence instead of jmp-n-link (reg push/pop) we do jmp
	   (4) No need to route __syscall_error via PLT, B is inherently
	       position independent.  */
	b   __syscall_error
PSEUDO_END (__clone)


	.align 4
	.type thread_start_clone, %function
thread_start_clone:
	cfi_startproc
	/* Terminate call stack by noting ra is undefined.  */
	cfi_undefined (blink)

	/* Child jumps off to @fn with @arg as argument.  */
	jl.d	[r10]
	mov	r0, r11

	/* exit() with result from @fn (already in r0).  */
	mov	r8, __NR_exit
	ARC_TRAP_INSN

	/* In case it ever came back.  */
	flag	1

	cfi_endproc
	.size thread_start_clone, .-thread_start_clone

libc_hidden_def (__clone)
weak_alias (__clone, clone)
