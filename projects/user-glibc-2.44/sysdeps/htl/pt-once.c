/* pthread_once.  Generic version.
   Copyright (C) 2002-2026 Free Software Foundation, Inc.
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
   License along with the GNU C Library;  if not, see
   <https://www.gnu.org/licenses/>.  */

#include <pthread.h>
#include <atomic.h>

#include <pt-internal.h>
#include <shlib-compat.h>

static void
clear_once_control (void *arg)
{
  pthread_once_t *once_control = arg;
  __pthread_spin_unlock (&once_control->__lock);
}

int
__pthread_once (pthread_once_t *once_control, void (*init_routine) (void))
{
  ASSERT_TYPE_SIZE (pthread_once_t, __SIZEOF_PTHREAD_ONCE_T);

  if (atomic_load_acquire (&once_control->__run) == 0)
    {
      __pthread_spin_wait (&once_control->__lock);

      if (once_control->__run == 0)
	{
	  pthread_cleanup_push (clear_once_control, once_control);
	  init_routine ();
	  pthread_cleanup_pop (0);

	  atomic_store_release (&once_control->__run, 1);
	}

      __pthread_spin_unlock (&once_control->__lock);
    }

  return 0;
}
libc_hidden_def (__pthread_once)
versioned_symbol (libc, __pthread_once, pthread_once, GLIBC_2_42);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_42)
compat_symbol (libpthread, __pthread_once, pthread_once, GLIBC_2_12);
#endif
