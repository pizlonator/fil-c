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


/* Get expiration overrun for timer TIMERID.  */
int
__timer_getoverrun (timer_t timerid)
{
  struct timer_node *timer;
  int retval = -1;

  __pthread_mutex_lock (&__timer_mutex);

  if (! timer_valid (timer = timer_id2ptr (timerid)))
    __set_errno (EINVAL);
  else
    retval = timer->overrun_count;

  __pthread_mutex_unlock (&__timer_mutex);

  return retval;
}
versioned_symbol (libc, __timer_getoverrun, timer_getoverrun, RT_IN_LIBC);
#if OTHER_SHLIB_COMPAT (librt, GLIBC_2_2, RT_IN_LIBC)
compat_symbol (librt, __timer_getoverrun, timer_getoverrun, GLIBC_2_2);
#endif
