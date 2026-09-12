/* Copyright (C) 2000-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2.1 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, see <https://www.gnu.org/licenses/>.  */

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <shlib-compat.h>
#include <rt-libc.h>

#include "posix-timer.h"


/* Delete timer TIMERID.  */
int
__timer_delete (timer_t timerid)
{
  struct timer_node *timer;
  int retval = -1;

  __pthread_mutex_lock (&__timer_mutex);

  timer = timer_id2ptr (timerid);
  if (! timer_valid (timer))
    /* Invalid timer ID or the timer is not in use.  */
    __set_errno (EINVAL);
  else
    {
      if (timer->armed && timer->thread != NULL)
	{
	  struct thread_node *thread = timer->thread;
	  assert (thread != NULL);

	  /* If thread is cancelled while waiting for handler to terminate,
	     the mutex is unlocked and timer_delete is aborted.  */
	  pthread_cleanup_push (__timer_mutex_cancel_handler, &__timer_mutex);

	  /* If timer is currently being serviced, wait for it to finish.  */
	  while (thread->current_timer == timer)
	    __pthread_cond_wait (&thread->cond, &__timer_mutex);

	  pthread_cleanup_pop (0);
        }

      /* Remove timer from whatever queue it may be on and deallocate it.  */
      timer->inuse = TIMER_DELETED;
      list_unlink_ip (&timer->links);
      timer_delref (timer);
      retval = 0;
    }

  __pthread_mutex_unlock (&__timer_mutex);

  return retval;
}
versioned_symbol (libc, __timer_delete, timer_delete, RT_IN_LIBC);
#if OTHER_SHLIB_COMPAT (librt, GLIBC_2_2, RT_IN_LIBC)
compat_symbol (librt, __timer_delete, timer_delete, GLIBC_2_2);
#endif
