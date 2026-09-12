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

#include "pthreadP.h"
#include <shlib-compat.h>

int
__pthread_tryjoin_np (pthread_t threadid, void **thread_return)
{
  /* Return right away if the thread hasn't terminated yet.  In Fil-C the
     exiting thread itself sets PD->dead and wakes the futex on it: threads
     are created by the Fil-C runtime instead of the clone syscall, so the
     kernel never sets PD->joinstate to THREAD_STATE_EXITED, and PD->dead is
     what tells us that the thread has terminated.  PD->dead does not change
     during the thread lifetime once the thread has terminated.  The
     __pthread_clockjoin_ex will only call the cancellable futex if the
     thread has not terminated yet, so calling it should be safe wrt not
     making pthread_tryjoin_np a cancellable entrypoint (since it is marked
     as __THROW).  */

  struct pthread *pd = (struct pthread *) threadid;
  return atomic_load_acquire (&pd->dead) == 0
	 ? EBUSY
	 : __pthread_clockjoin_ex (threadid, thread_return, 0, NULL, false);
}
versioned_symbol (libc, __pthread_tryjoin_np, pthread_tryjoin_np, GLIBC_2_34);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_3_3, GLIBC_2_34)
compat_symbol (libc, __pthread_tryjoin_np, pthread_tryjoin_np, GLIBC_2_3_3);
#endif
