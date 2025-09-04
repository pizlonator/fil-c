/* Implementation of epoll_pwait2 syscall wrapper.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <sys/epoll.h>
#include <sysdep.h>

int
__epoll_pwait2_time64 (int fd, struct epoll_event *ev, int maxev,
		       const struct __timespec64 *tmo, const sigset_t *s)
{
  /* The syscall only supports 64-bit time_t.  */
  return SYSCALL_CANCEL (epoll_pwait2, fd, ev, maxev, tmo, s, __NSIG_BYTES);
}
#if __TIMESIZE != 64
libc_hidden_def (__epoll_pwait2_time64)

int
epoll_pwait2 (int fd, struct epoll_event *ev, int maxev,
	      const struct timespec *tmo, const sigset_t *s)
{
  struct __timespec64 tmo64, *ptmo64 = NULL;
  if (tmo != NULL)
    {
      tmo64 = valid_timespec_to_timespec64 (*tmo);
      ptmo64 = &tmo64;
    }
  return __epoll_pwait2_time64 (fd, ev, maxev, ptmo64, s);
}
#endif
