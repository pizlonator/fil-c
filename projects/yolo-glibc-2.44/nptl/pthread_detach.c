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

#include <errno.h>
#include "pthreadP.h"
#include <atomic.h>
#include <shlib-compat.h>

int
___pthread_detach (pthread_t th)
{
  struct pthread *pd = (struct pthread *) th;

  /* CONCURRENCY NOTES:

     Concurrent pthread_detach will return EINVAL for the case where the
     thread is already detached (THREAD_STATE_DETACHED).  POSIX states it is
     undefined to call pthread_detach if TH refers to a non-joinable thread.

     In the case the thread is being terminated (THREAD_STATE_EXITING),
     pthread_detach will be responsible for cleaning up the stack.  */

  unsigned int prevstate = atomic_load_relaxed (&pd->joinstate);
  do
    {
      if (prevstate != THREAD_STATE_JOINABLE)
	{
	  if (prevstate == THREAD_STATE_DETACHED)
	    return EINVAL;
	  /* pthread_detach is declared _THROW so it need to call a
	     pthread_join variant that is not a cancellation entrypoint.  */

	  return __pthread_clockjoin_ex (th, 0, 0 /* Ignored */, NULL,
					 false);
	}
    }
  while (!atomic_compare_exchange_weak_acquire (&pd->joinstate, &prevstate,
						THREAD_STATE_DETACHED));
  return 0;
}
versioned_symbol (libc, ___pthread_detach, pthread_detach, GLIBC_2_34);
libc_hidden_ver (___pthread_detach, __pthread_detach)
#ifndef SHARED
strong_alias (___pthread_detach, __pthread_detach)
#endif

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_0, GLIBC_2_34)
compat_symbol (libc, ___pthread_detach, pthread_detach, GLIBC_2_0);
#endif
