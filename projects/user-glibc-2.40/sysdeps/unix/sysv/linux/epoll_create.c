/* Open an epoll file descriptor.  Linux version.
   Copyright (C) 2011-2024 Free Software Foundation, Inc.
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
   License along with the GNU C Library.  If not, see
   <https://www.gnu.org/licenses/>.  */

#include <sys/epoll.h>
#include <sysdep.h>

libc_hidden_proto (epoll_create)

int
epoll_create (int size)
{
#ifdef __NR_epoll_create
  return INLINE_SYSCALL_CALL (epoll_create, size);
#else
  if (size <= 0)
    {
      __set_errno (EINVAL);
      return -1;
    }

  return INLINE_SYSCALL_CALL (epoll_create1, 0);
#endif
}
libc_hidden_def (epoll_create)
