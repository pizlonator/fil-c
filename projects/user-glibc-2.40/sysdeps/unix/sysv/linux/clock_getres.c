/* clock_getres -- Get the resolution of a POSIX clockid_t.  Linux version.
   Copyright (C) 2003-2024 Free Software Foundation, Inc.
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

#include <sysdep.h>
#include <errno.h>
#include <time.h>

#include <shlib-compat.h>
#include <pizlonated_syscalls.h>

/* Get resolution of clock.  */
int
__clock_getres64 (clockid_t clock_id, struct __timespec64 *res)
{
  return zsys_clock_getres (clock_id, res);
}

#if __TIMESIZE != 64
#error "Time is 64-bit bruh"
#endif
libc_hidden_def (__clock_getres)

versioned_symbol (libc, __clock_getres, clock_getres, GLIBC_2_17);
/* clock_getres moved to libc in version 2.17;
   old binaries may expect the symbol version it had in librt.  */
#if SHLIB_COMPAT (libc, GLIBC_2_2, GLIBC_2_17)
strong_alias (__clock_getres, __clock_getres_2);
compat_symbol (libc, __clock_getres_2, clock_getres, GLIBC_2_2);
#endif
