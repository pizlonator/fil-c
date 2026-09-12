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

#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <shlib-compat.h>
#include <rt-libc.h>

#include "posix-timer.h"


/* Get current value of timer TIMERID and store it in VALUE.  */
int
__timer_gettime (timer_t timerid, struct itimerspec *value)
{
  struct timer_node *timer;
  struct timespec now, expiry;
  int retval = -1, armed = 0, valid;
  clock_t clock = 0;

  __pthread_mutex_lock (&__timer_mutex);

  timer = timer_id2ptr (timerid);
  valid = timer_valid (timer);

  if (valid) {
    armed = timer->armed;
    expiry = timer->expirytime;
    clock = timer->clock;
    value->it_interval = timer->value.it_interval;
  }

  __pthread_mutex_unlock (&__timer_mutex);

  if (valid)
    {
      if (armed)
	{
	  __clock_gettime (clock, &now);
	  if (timespec_compare (&now, &expiry) < 0)
	    timespec_sub (&value->it_value, &expiry, &now);
	  else
	    {
	      value->it_value.tv_sec = 0;
	      value->it_value.tv_nsec = 0;
	    }
	}
      else
	{
	  value->it_value.tv_sec = 0;
	  value->it_value.tv_nsec = 0;
	}

      retval = 0;
    }
  else
    __set_errno (EINVAL);

  return retval;
}
versioned_symbol (libc, __timer_gettime, timer_gettime, RT_IN_LIBC);
#if OTHER_SHLIB_COMPAT (librt, GLIBC_2_2, RT_IN_LIBC)
compat_symbol (librt, __timer_gettime, timer_gettime, GLIBC_2_2);
#endif
