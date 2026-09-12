/* Set the cancel state for the calling thread.
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

#include <hurd/signal.h>
#include <pthread.h>
#include <shlib-compat.h>
#include <pt-internal.h>

int
__pthread_setcancelstate (int state, int *oldstate)
{
  struct __pthread *p = _pthread_self ();
  int cancelled;

  switch (state)
    {
    default:
      return EINVAL;
    case PTHREAD_CANCEL_ENABLE:
    case PTHREAD_CANCEL_DISABLE:
      break;
    }

  HURD_CRITICAL_BEGIN;
  __pthread_mutex_lock (&p->cancel_lock);
  if (oldstate != NULL)
    *oldstate = p->cancel_state;
  p->cancel_state = state;
  cancelled = (p->cancel_state == PTHREAD_CANCEL_ENABLE) && p->cancel_pending == 1 && (p->cancel_type == PTHREAD_CANCEL_ASYNCHRONOUS);
  if (cancelled)
    /* Do not achieve cancel when called again, notably from __pthread_exit itself.  */
    p->cancel_pending = 2;
  __pthread_mutex_unlock (&p->cancel_lock);
  HURD_CRITICAL_END;

  if (cancelled)
    __pthread_exit (PTHREAD_CANCELED);

  return 0;
}
libc_hidden_def (__pthread_setcancelstate)
versioned_symbol (libc, __pthread_setcancelstate, pthread_setcancelstate, GLIBC_2_21);

#if OTHER_SHLIB_COMPAT (libpthread, GLIBC_2_12, GLIBC_2_21)
compat_symbol (libc, __pthread_setcancelstate, pthread_setcancelstate, GLIBC_2_12);
#endif
