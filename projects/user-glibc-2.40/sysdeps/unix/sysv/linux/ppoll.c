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

#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/poll.h>
#include <sysdep-cancel.h>
#include <pizlonated_syscalls.h>

int
__ppoll64 (struct pollfd *fds, nfds_t nfds, const struct __timespec64 *timeout,
           const sigset_t *sigmask)
{
  return zsys_ppoll (fds, nfds, timeout, sigmask);
}

#if __TIMESIZE != 64
libc_hidden_def (__ppoll64)

int
ppoll (struct pollfd *fds, nfds_t nfds, const struct timespec *timeout,
         const sigset_t *sigmask)
{
  struct __timespec64 ts64;
  if (timeout)
    ts64 = valid_timespec_to_timespec64 (*timeout);

  return __ppoll64 (fds, nfds, timeout ? &ts64 : NULL, sigmask);
}
#endif
libc_hidden_def (ppoll)
