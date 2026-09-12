/* Copyright (C) 2002-2026 Free Software Foundation, Inc.
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

#include <setjmp.h>
#include <stdlib.h>
#include "pthreadP.h"

/* Called by the INTERNAL_SYSCALL_CANCEL macro, check for cancellation and
   returns the syscall value or its negative error code.  */
long int
__internal_syscall_cancel (__syscall_arg_t a1, __syscall_arg_t a2,
			   __syscall_arg_t a3, __syscall_arg_t a4,
			   __syscall_arg_t a5, __syscall_arg_t a6,
			   __SYSCALL_CANCEL7_ARG_DEF
			   __syscall_arg_t nr)
{
  long int result;
  struct pthread *pd = THREAD_SELF;

  /* If cancellation is not enabled, call the syscall directly and also
     for thread terminatation to avoid call __syscall_do_cancel while
     executing cleanup handlers.  */
  int ch = atomic_load_relaxed (&pd->cancelhandling);
  if (SINGLE_THREAD_P || !cancel_enabled (ch) || cancel_exiting (ch))
    {
      result = INTERNAL_SYSCALL_NCS_CALL (nr, a1, a2, a3, a4, a5, a6
					  __SYSCALL_CANCEL7_ARCH_ARG7);
      if (INTERNAL_SYSCALL_ERROR_P (result))
	return -INTERNAL_SYSCALL_ERRNO (result);
      return result;
    }

  /* Call the arch-specific entry points that contains the globals markers
     to be checked by SIGCANCEL handler.  */
  result = __syscall_cancel_arch (&pd->cancelhandling, nr, a1, a2, a3, a4, a5,
			          a6 __SYSCALL_CANCEL7_ARCH_ARG7);

  /* If the cancellable syscall was interrupted by SIGCANCEL and it has no
     side-effect, cancel the thread if cancellation is enabled.  */
  ch = atomic_load_relaxed (&pd->cancelhandling);
  /* The behaviour here assumes that EINTR is returned only if there are no
     visible side effects.  POSIX Issue 7 has not yet provided any stronger
     language for close, and in theory the close syscall could return EINTR
     and leave the file descriptor open (conforming and leaks).  It expects
     that no such kernel is used with glibc.  */
  if (result == -EINTR && cancel_enabled_and_canceled (ch))
    __syscall_do_cancel ();

  return result;
}

/* Called by the SYSCALL_CANCEL macro, check for cancellation and return the
   syscall expected success value (usually 0) or, in case of failure, -1 and
   sets errno to syscall return value.  */
long int
__syscall_cancel (__syscall_arg_t a1, __syscall_arg_t a2,
		  __syscall_arg_t a3, __syscall_arg_t a4,
		  __syscall_arg_t a5, __syscall_arg_t a6,
		  __SYSCALL_CANCEL7_ARG_DEF __syscall_arg_t nr)
{
  long int r = __internal_syscall_cancel (a1, a2, a3, a4, a5, a6,
					  __SYSCALL_CANCEL7_ARG nr);
  return __glibc_unlikely (INTERNAL_SYSCALL_ERROR_P (r))
	 ? SYSCALL_ERROR_LABEL (INTERNAL_SYSCALL_ERRNO (r))
	 : r;
}

/* Called by __syscall_cancel_arch or function above start the thread
   cancellation.  */
_Noreturn void
__syscall_do_cancel (void)
{
  struct pthread *self = THREAD_SELF;

  /* Disable thread cancellation to avoid cancellable entrypoints calling
     __syscall_do_cancel recursively.  We atomic load relaxed to check the
     state of cancelhandling, there is no particular ordering requirement
     between the syscall call and the other thread setting our cancelhandling
     with a atomic store acquire.

     POSIX Issue 7 notes that the cancellation occurs asynchronously on the
     target thread, that implies there is no ordering requirements.  It does
     not need a MO release store here.  */
  int oldval = atomic_load_relaxed (&self->cancelhandling);
  while (1)
    {
      int newval = oldval | CANCELSTATE_BITMASK;
      if (oldval == newval)
	break;
      if (atomic_compare_exchange_weak_acquire (&self->cancelhandling,
						&oldval, newval))
	break;
    }

  __do_cancel (PTHREAD_CANCELED);
}
