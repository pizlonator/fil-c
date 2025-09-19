/* Copyright (C) 2006-2024 Free Software Foundation, Inc.
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

#include <sys/select.h>
#include <sysdep-cancel.h>

int
__pselect64 (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	     const struct __timespec64 *timeout, const sigset_t *sigmask)
{
  return zsys_pselect (nfds, readfds, writefds, exceptfds, timeout, sigmask);
}

#if __TIMESIZE != 64
libc_hidden_def (__pselect64)

int
__pselect (int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	   const struct timespec *timeout, const sigset_t *sigmask)
{
  struct __timespec64 ts64, *pts64 = NULL;
  if (timeout != NULL)
    {
      ts64 = valid_timespec_to_timespec64 (*timeout);
      pts64 = &ts64;
    }
  return __pselect64 (nfds, readfds, writefds, exceptfds, pts64, sigmask);
}
#endif

#ifndef __pselect
weak_alias (__pselect, pselect)
#endif
