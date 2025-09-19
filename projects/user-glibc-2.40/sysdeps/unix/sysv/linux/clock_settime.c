/* Copyright (C) 2003-2024 Free Software Foundation, Inc.
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
#include <sysdep.h>
#include <time.h>
#include <shlib-compat.h>
#include <kernel-features.h>
#include <pizlonated_syscalls.h>

/* Set CLOCK to value TP.  */
int
__clock_settime64 (clockid_t clock_id, const struct __timespec64 *tp)
{
  return zsys_clock_settime (clock_id, tp);
}

#if __TIMESIZE != 64
libc_hidden_def (__clock_settime64)

int
__clock_settime (clockid_t clock_id, const struct timespec *tp)
{
  struct __timespec64 ts64 = valid_timespec_to_timespec64 (*tp);

  return __clock_settime64 (clock_id, &ts64);
}
#endif

libc_hidden_def (__clock_settime)

versioned_symbol (libc, __clock_settime, clock_settime, GLIBC_2_17);
/* clock_settime moved to libc in version 2.17;
   old binaries may expect the symbol version it had in librt.  */
#if SHLIB_COMPAT (libc, GLIBC_2_2, GLIBC_2_17)
strong_alias (__clock_settime, __clock_settime_2);
compat_symbol (libc, __clock_settime_2, clock_settime, GLIBC_2_2);
#endif
