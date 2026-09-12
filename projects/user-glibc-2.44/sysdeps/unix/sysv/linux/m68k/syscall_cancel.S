/* Cancellable syscall wrapper.  Linux/m68k version.
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
#include <descr-const.h>

/* long int __syscall_cancel_arch (int *cancelhandling,
				   __syscall_arg_t nr,
				   __syscall_arg_t arg1,
				   __syscall_arg_t arg2,
				   __syscall_arg_t arg3,
				   __syscall_arg_t arg4,
				   __syscall_arg_t arg5,
				   __syscall_arg_t arg6)  */


ENTRY (__syscall_cancel_arch)
#ifdef __mcoldfire__
	lea	(-16,%sp),%sp
	movem.l	%d2-%d5,(%sp)
#else
	movem.l	%d2-%d5,-(%sp)
#endif
	cfi_def_cfa_offset (20)
	cfi_offset (2, -20)
	cfi_offset (3, -16)
	cfi_offset (4, -12)
	cfi_offset (5, -8)

	.global __syscall_cancel_arch_start
__syscall_cancel_arch_start:

	move.l	20(%sp),%a0
	move.l	(%a0),%d0
#ifdef __mcoldfire__
	move.w	%d0,%ccr
	jeq	1f
#else
	btst	#TCB_CANCELED_BIT,%d0
	jne 	1f
#endif

	move.l	48(%sp),%a0
	move.l	44(%sp),%d5
	move.l	40(%sp),%d4
	move.l	36(%sp),%d3
	move.l	32(%sp),%d2
	move.l	28(%sp),%d1
	move.l	24(%sp),%d0
	trap #0

	.global __syscall_cancel_arch_end
__syscall_cancel_arch_end:

#ifdef __mcoldfire__
	movem.l	(%sp),%d2-%d5
	lea	(16,%sp),%sp
#else
	movem.l	(%sp)+,%d2-%d5
#endif
	rts

1:
#ifdef PIC
	bsr.l __syscall_do_cancel
#else
	jsr __syscall_do_cancel
#endif
END (__syscall_cancel_arch)
