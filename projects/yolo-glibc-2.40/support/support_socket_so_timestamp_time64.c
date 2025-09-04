/* Return whether socket supports 64-bit timestamps.
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

#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <support/support.h>
#ifdef __linux__
# include <socket-constants-time64.h>
#endif

bool
support_socket_so_timestamp_time64 (int fd)
{
#ifdef __linux__
# if __LINUX_KERNEL_VERSION >= 0x050100                          \
   || __WORDSIZE == 64                                           \
   || (defined __SYSCALL_WORDSIZE && __SYSCALL_WORDSIZE == 64)
  return true;
# else
  int level = SOL_SOCKET;
  int optname = COMPAT_SO_TIMESTAMP_NEW;
  int optval;
  socklen_t len = sizeof (optval);

  int r = syscall (__NR_getsockopt, fd, level, optname, &optval, &len);
  return r != -1;
# endif
#else
  return false;
#endif
}
