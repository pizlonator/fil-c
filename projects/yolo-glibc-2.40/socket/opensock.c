/* Create socket with an unspecified address family for use with ioctl.
   Copyright (C) 1999-2024 Free Software Foundation, Inc.
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
#include <sys/socket.h>

/* Return a socket of any type.  The socket can be used in subsequent
   ioctl calls to talk to the kernel.  */
int
__opensock (void)
{
  /* SOCK_DGRAM is supported by all address families.  */
  int type = SOCK_DGRAM | SOCK_CLOEXEC;
  int fd;

  fd = __socket (AF_UNIX, type, 0);
  if (fd >= 0)
    return fd;
  fd = __socket (AF_INET, type, 0);
  if (fd >= 0)
    return fd;
  fd = __socket (AF_INET6, type, 0);
  if (fd >= 0)
    return fd;
  __set_errno (ENOENT);
  return fd;
}
