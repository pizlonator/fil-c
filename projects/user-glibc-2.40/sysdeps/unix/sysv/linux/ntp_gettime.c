/* Copyright (C) 1999-2024 Free Software Foundation, Inc.
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

#define ntp_gettime ntp_gettime_redirect

#include <time.h>
#include <sys/timex.h>

#undef ntp_gettime

#ifndef MOD_OFFSET
# define modes mode
#endif

/* glibc 2.12 added the 'tai' field to follow along the kernel, but it did
   not add a compatibility symbol, instead it added __ntp_gettimex.  However
   ntptimeval was still used in both cases, so to keep compatibility it
   can not set all the new field.  */

/* clock_adjtime64 with CLOCK_REALTIME does not trigger EINVAL,
   ENODEV, or EOPNOTSUPP.  It might still trigger EPERM.  */

int
__ntp_gettime64 (struct __ntptimeval64 *ntv)
{
  struct __timex64 tntx;
  int result;

  tntx.modes = 0;
  result = __clock_adjtime64 (CLOCK_REALTIME, &tntx);
  ntv->time = tntx.time;
  ntv->maxerror = tntx.maxerror;
  ntv->esterror = tntx.esterror;

  return result;
}

#if __TIMESIZE != 64
libc_hidden_def (__ntp_gettime64)

int
__ntp_gettime (struct ntptimeval *ntv)
{
  struct __ntptimeval64 ntv64;
  int result;

  result = __ntp_gettime64 (&ntv64);
  ntv->time = valid_timeval64_to_timeval (ntv64.time);
  ntv->maxerror = ntv64.maxerror;
  ntv->esterror = ntv64.esterror;

  return result;
}
#endif
strong_alias (__ntp_gettime, ntp_gettime)
