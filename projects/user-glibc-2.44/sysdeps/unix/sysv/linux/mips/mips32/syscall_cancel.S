/* Cancellable syscall wrapper.  Linux/mips32 version.
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
   <http://www.gnu.org/licenses/>.  */

#include <sysdep.h>
#include <sys/asm.h>
#include <descr-const.h>

/* long int __syscall_cancel_arch (int *cancelhandling,
				   __syscall_arg_t nr,
				   __syscall_arg_t arg1,
				   __syscall_arg_t arg2,
				   __syscall_arg_t arg3,
				   __syscall_arg_t arg4,
				   __syscall_arg_t arg5,
				   __syscall_arg_t arg6,
				   __syscall_arg_t arg7)  */

#define FRAME_SIZE 56

NESTED (__syscall_cancel_arch, FRAME_SIZE, fp)
	.mask	0xc0070000,-SZREG
	.fmask	0x00000000,0

	PTR_ADDIU sp, -FRAME_SIZE
	cfi_def_cfa_offset (FRAME_SIZE)

	sw	fp, 48(sp)
	sw	ra, 52(sp)
	sw	s2, 44(sp)
	sw	s1, 40(sp)
	sw	s0, 36(sp)
#ifdef __PIC__
	.cprestore	16
#endif
	cfi_offset (ra, -4)
	cfi_offset (fp, -8)
	cfi_offset (s2, -12)
	cfi_offset (s1, -16)
	cfi_offset (s0, -20)

	move	fp ,sp
	cfi_def_cfa_register (fp)

	.globl __syscall_cancel_arch_start
__syscall_cancel_arch_start:

	lw	v0, 0(a0)
	andi	v0, v0, TCB_CANCELED_BITMASK
	bne	v0, zero, 2f

	addiu	sp, sp, -16
	addiu	v0, sp, 16
	sw	v0, 24(fp)

	move	s0, a1
	move	a0, a2
	move	a1, a3
	lw	a2, 72(fp)
	lw	a3, 76(fp)
	lw	v0, 84(fp)
	lw	s1, 80(fp)
	lw	s2, 88(fp)

	.set	noreorder
	subu	sp, 32
	sw	s1, 16(sp)
	sw	v0, 20(sp)
	sw	s2, 24(sp)
	move	v0, s0
	syscall

	.globl __syscall_cancel_arch_end
__syscall_cancel_arch_end:
	addiu	sp, sp, 32
	.set	reorder

	beq	a3, zero, 1f
	subu	v0, zero, v0
1:
	move	sp, fp
	cfi_remember_state
	cfi_def_cfa_register (sp)
	lw	ra, 52(fp)
	lw	fp, 48(sp)
	lw	s2, 44(sp)
	lw	s1, 40(sp)
	lw	s0, 36(sp)

	.set	noreorder
	.set	nomacro
	jr	ra
	addiu	sp,sp,FRAME_SIZE

	.set	macro
	.set	reorder

	cfi_def_cfa_offset (0)
	cfi_restore (s0)
	cfi_restore (s1)
	cfi_restore (s2)
	cfi_restore (fp)
	cfi_restore (ra)

2:
	cfi_restore_state
#ifdef __PIC__
	PTR_LA	t9, __syscall_do_cancel
	jalr	t9
#else
	jal	__syscall_do_cancel
#endif

END (__syscall_cancel_arch)
