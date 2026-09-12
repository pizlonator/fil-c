/* Copyright (C) 1991-2026 Free Software Foundation, Inc.
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
#include <sys/time.h>
#include <hurd.h>
#include <mach/time_value.h>

/* Adjust the current time of day by the amount in DELTA.
   If OLDDELTA is not NULL, it is filled in with the amount
   of time adjustment remaining to be done from the last `__adjtime' call.
   This call is restricted to the super-user.  */
int
__adjtime (const struct timeval *delta, struct timeval *olddelta)
{
  error_t err;
  mach_port_t hostpriv;
  time_value_t rpc_delta, rpc_olddelta;

  err = __get_privileged_ports (&hostpriv, NULL);
  if (err)
    return __hurd_fail (EPERM);

  if (delta != NULL)
    {
      if (delta->tv_usec >=  TIME_MICROS_MAX ||
          delta->tv_usec <= -TIME_MICROS_MAX)
       return __hurd_fail (EINVAL);

      rpc_delta.seconds = delta->tv_sec;
      rpc_delta.microseconds = delta->tv_usec;
    }
  else
#ifdef MACH_ADJTIME_USECS_OMIT
    {
      /* gnumach will not attempt to update the system time if the
	 specified 'microseconds' is specifically
	 MACH_ADJTIME_USECS_OMIT. It will still return the olddelta
	 under these circumstances. */
      rpc_delta.seconds = 0;
      rpc_delta.microseconds = MACH_ADJTIME_USECS_OMIT;
    }
#else
    return __hurd_fail (EINVAL);
#endif

  err = __host_adjust_time (hostpriv, rpc_delta, &rpc_olddelta);
  __mach_port_deallocate (__mach_task_self (), hostpriv);

  if (err)
    return __hurd_fail (err);

  if (olddelta != NULL)
    {
      olddelta->tv_sec = rpc_olddelta.seconds;
      olddelta->tv_usec = rpc_olddelta.microseconds;
    }

  return 0;
}

weak_alias (__adjtime, adjtime)
