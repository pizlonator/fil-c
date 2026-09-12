/* The clone3 syscall wrapper.  Linux/mips version.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.

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

#include <sys/asm.h>
#include <sysdep.h>
#define _ERRNO_H        1
#include <bits/errno.h>

/* The userland implementation is:
   int clone3 (struct clone_args *cl_args, size_t size,
               int (*func)(void *arg), void *arg);

   the kernel entry is:
   int clone3 (struct clone_args *cl_args, size_t size);

   The parameters are passed in registers from userland:
   a0/$4: cl_args
   a1/$5: size
   a2/$6: func
   a3/$7: arg  */

	.text
	.set		nomips16
#define FRAMESZ ((NARGSAVE*SZREG)+ALSZ)&ALMASK
GPOFF= FRAMESZ-(1*SZREG)
NESTED(__clone3, SZREG, sp)
#ifdef __PIC__
	SETUP_GP
#endif
#if FRAMESZ
	PTR_SUBU sp, FRAMESZ
	cfi_adjust_cfa_offset (FRAMESZ)
#endif
	SETUP_GP64_STACK (GPOFF, __clone3)
#ifdef __PIC__
	SAVE_GP (GPOFF)
#endif
#ifdef PROF
	.set	noat
	move	$1,ra
	jal	_mcount
	.set	at
#endif

	/* Sanity check args.  */
	li	v0, EINVAL
	beqz	a0, L(error)	/* No NULL cl_args pointer.  */
	beqz	a2, L(error)	/* No NULL function pointer.  */

#if _MIPS_SIM == _ABIO32
	/* Both stack and stack_size on clone_args are defined as uint64_t, and
	   there is no need to handle values larger than to 32 bits for o32.  */
# if __BYTE_ORDER == __BIG_ENDIAN
#  define CL_STACKPOINTER_OFFSET  44
#  define CL_STACKSIZE_OFFSET     52
# else
#  define CL_STACKPOINTER_OFFSET  40
#  define CL_STACKSIZE_OFFSET     48
# endif

	/* For o32 we need to setup a minimal stack frame to allow cprestore
	   on __thread_start_clone3.  Also there is no guarantee by kABI that
	   $8 will be preserved after syscall execution (so we need to save it
	   on the provided stack).  */
	lw	t0, CL_STACKPOINTER_OFFSET(a0)	/* Load the stack pointer.  */
	lw	t1, CL_STACKSIZE_OFFSET(a0)	/* Load the stack_size.  */
	addiu	t1, -32				/* Update the stack size.  */
	addu	t2, t1, t0			/* Calculate the thread stack.  */
	sw	a3, 0(t2)			/* Save argument pointer.  */
	sw	t1, CL_STACKSIZE_OFFSET(a0)	/* Save the new stack size.  */
#else
	move	$8, a3		/* a3 is set to 0/1 for syscall success/error
				   while a4/$8 is returned unmodified.  */
#endif

	/* Do the system call, the kernel expects:
	   v0: system call number
	   a0: cl_args
	   a1: size  */
	li		v0, __NR_clone3
	cfi_endproc
	syscall

	bnez		a3, L(error)
	beqz		v0, L(thread_start_clone3)

	/* Successful return from the parent */
	cfi_startproc
#if FRAMESZ
	cfi_adjust_cfa_offset (FRAMESZ)
#endif
	SETUP_GP64_STACK_CFI (GPOFF)
	cfi_remember_state
	RESTORE_GP64_STACK
#if FRAMESZ
	PTR_ADDU	sp, FRAMESZ
	cfi_adjust_cfa_offset (-FRAMESZ)
#endif
	ret

L(error):
	cfi_restore_state
#ifdef __PIC__
	PTR_LA		t9, __syscall_error
	RESTORE_GP64_STACK
	PTR_ADDU	sp, FRAMESZ
	cfi_adjust_cfa_offset (-FRAMESZ)
	jr		t9
#else
	RESTORE_GP64_STACK
	PTR_ADDU	sp, FRAMESZ
	cfi_adjust_cfa_offset (-FRAMESZ)
	j		__syscall_error
#endif
END (__clone3)

/* Load up the arguments to the function.  Put this block of code in
   its own function so that we can terminate the stack trace with our
   debug info.  */

ENTRY(__thread_start_clone3)
L(thread_start_clone3):
	cfi_undefined ($31)
	/* cp is already loaded.  */
	SAVE_GP (GPOFF)
	/* The stackframe has been created on entry of clone3.  */

	/* Restore the arg for user's function.  */
	move		t9, a2		/* Function pointer.  */
#if _MIPS_SIM == _ABIO32
	PTR_L		a0, 0(sp)
#else
	move		a0, $8		/* Argument pointer.  */
#endif

	/* Call the user's function.  */
	jal		t9

	move		a0, v0
	li		v0, __NR_exit
	syscall
END(__thread_start_clone3)

libc_hidden_def (__clone3)
