/* Pthread cancellation syscall bridge.  Default Linux version.
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
#include <pthreadP.h>

#warning "This implementation should be use just as reference or for bootstrapping"

/* This is the generic version of the cancellable syscall code which
   adds the label guards (__syscall_cancel_arch_{start,end}) used on SIGCANCEL
   handler to check if the cancelled syscall have side-effects that need to be
   returned to the caller.

   This implementation should be used as a reference one to document the
   implementation constraints:

     1. The __syscall_cancel_arch_start should point just before the test
        that thread is already cancelled,
     2.	The __syscall_cancel_arch_end should point to the immediate next
        instruction after the syscall one.
     3. It should return the syscall value or a negative result if is has
        failed, similar to INTERNAL_SYSCALL_CALL.

   The __syscall_cancel_arch_end one is because the kernel will signal
   interrupted syscall with side effects by setting the signal frame program
   counter (on the ucontext_t third argument from SA_SIGINFO signal handler)
   right after the syscall instruction.

   For some architecture, the INTERNAL_SYSCALL_NCS macro use more instructions
   to get the error condition from kernel (as for powerpc and sparc that
   checks for the conditional register), or uses an out of the line helper
   (ARM thumb), or uses a kernel helper gate (i686 or ia64).  In this case
   the architecture should either adjust the macro or provide a custom
   __syscall_cancel_arch implementation.   */

long int
__syscall_cancel_arch (volatile int *ch, __syscall_arg_t nr,
		       __syscall_arg_t a1, __syscall_arg_t a2,
		       __syscall_arg_t a3, __syscall_arg_t a4,
		       __syscall_arg_t a5, __syscall_arg_t a6
		       __SYSCALL_CANCEL7_ARG_DEF)
{
#define ADD_LABEL(__label)		\
  asm volatile (			\
    ".global " __label "\t\n"		\
    __label ":\n");

  ADD_LABEL ("__syscall_cancel_arch_start");
  if (__glibc_unlikely (*ch & CANCELED_BITMASK))
    __syscall_do_cancel();

  long int result = INTERNAL_SYSCALL_NCS_CALL (nr, a1, a2, a3, a4, a5, a6
					       __SYSCALL_CANCEL7_ARG7);
  ADD_LABEL ("__syscall_cancel_arch_end");
  if (__glibc_unlikely (INTERNAL_SYSCALL_ERROR_P (result)))
    return -INTERNAL_SYSCALL_ERRNO (result);
  return result;
}
