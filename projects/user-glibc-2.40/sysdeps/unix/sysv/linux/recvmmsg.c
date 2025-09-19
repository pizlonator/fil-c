/* Copyright (C) 2010-2024 Free Software Foundation, Inc.
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

#include <sys/socket.h>
#include <sysdep.h>
#include <socketcall.h>
#include <pizlonated_syscalls.h>

int
__recvmmsg64 (int fd, struct mmsghdr *vmessages, unsigned int vlen, int flags,
	      struct __timespec64 *timeout)
{
  return zsys_recvmmsg(fd, vmessages, vlen, flags, timeout);
}
#if __TIMESIZE != 64
#error "wtf"
libc_hidden_def (__recvmmsg64)

int
__recvmmsg (int fd, struct mmsghdr *vmessages, unsigned int vlen, int flags,
	    struct timespec *timeout)
{
  struct __timespec64 ts64, *pts64 = NULL;
  if (timeout != NULL)
    {
      ts64 = valid_timespec_to_timespec64 (*timeout);
      pts64 = &ts64;
    }
  int r = recvmmsg_syscall (fd, vmessages, vlen, flags, pts64);
  if (r >= 0 && timeout != NULL)
    /* The remanining timeout will be always less the input TIMEOUT.  */
    *timeout = valid_timespec64_to_timespec (ts64);
  return r;
}
#endif
weak_alias (__recvmmsg, recvmmsg)
