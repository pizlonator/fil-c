/* Copyright (C) 1997-2024 Free Software Foundation, Inc.
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

#include <signal.h>
#include <sysdep.h>
#include <pizlonated_syscalls.h>

int
__sigtimedwait64 (const sigset_t *set, siginfo_t *info,
		  const struct __timespec64 *timeout)
{
  return zsys_sigtimedwait (set, info, timeout);
}
#if __TIMESIZE != 64
libc_hidden_def (__sigtimedwait64)

int
__sigtimedwait (const sigset_t *set, siginfo_t *info,
		const struct timespec *timeout)
{
  struct __timespec64 ts64, *pts64 = NULL;
  if (timeout != NULL)
    {
      ts64 = valid_timespec_to_timespec64 (*timeout);
      pts64 = &ts64;
    }
  return __sigtimedwait64 (set, info, pts64);
}
#endif
libc_hidden_def (__sigtimedwait)
weak_alias (__sigtimedwait, sigtimedwait)
