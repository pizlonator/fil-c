/* Linux recvmsg syscall wrapper.
   Copyright (C) 2016-2024 Free Software Foundation, Inc.
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
#include <sysdep-cancel.h>
#include <socketcall.h>
#include <pizlonated_syscalls.h>

static int
__recvmsg_syscall (int fd, struct msghdr *msg, int flags)
{
  return zsys_recvmsg (fd, msg, flags);
}

ssize_t
__libc_recvmsg64 (int fd, struct msghdr *msg, int flags)
{
  ssize_t r;
#if __TIMESIZE != 64
  socklen_t orig_controllen = msg != NULL ? msg->msg_controllen : 0;
#endif

  r = __recvmsg_syscall (fd, msg, flags);

#if __TIMESIZE != 64
  if (r >= 0 && orig_controllen != 0)
    __convert_scm_timestamps (msg, orig_controllen);
#endif

  return r;
}
#if __TIMESIZE != 64
weak_alias (__libc_recvmsg64, __recvmsg64)

ssize_t
__libc_recvmsg (int fd, struct msghdr *msg, int flags)
{
  return __recvmsg_syscall (fd, msg, flags);
}
#endif
weak_alias (__libc_recvmsg, recvmsg)
weak_alias (__libc_recvmsg, __recvmsg)
