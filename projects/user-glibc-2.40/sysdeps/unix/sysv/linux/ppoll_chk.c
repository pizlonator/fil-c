/* Fortify ppoll implementation.  Linux version.
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

#include <poll.h>

int
__ppoll64_chk (struct pollfd *fds, nfds_t nfds,
	       const struct __timespec64 *tmo, const __sigset_t *ss,
	       __SIZE_TYPE__ fdslen)
{
  if (fdslen / sizeof (*fds) < nfds)
    __chk_fail ();

  return __ppoll64 (fds, nfds, tmo, ss);
}

#if __TIMESIZE != 64
int
__ppoll_chk (struct pollfd *fds, nfds_t nfds, const struct timespec *tmo,
	     const __sigset_t *ss, __SIZE_TYPE__ fdslen)
{
  if (fdslen / sizeof (*fds) < nfds)
    __chk_fail ();

  return ppoll (fds, nfds, tmo, ss);
}
#endif
