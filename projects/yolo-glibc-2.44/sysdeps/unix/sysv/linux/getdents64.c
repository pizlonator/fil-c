/* Get directory entries.  Linux LFS version.
   Copyright (C) 1997-2026 Free Software Foundation, Inc.
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

#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

/* The kernel struct linux_dirent64 matches the 'struct dirent64' type.  */
ssize_t
__getdents64 (int fd, void *buf, size_t nbytes)
{
  /* The system call takes an unsigned int argument, and some length
     checks in the kernel use an int type.  */
  if (nbytes > INT_MAX)
    nbytes = INT_MAX;
  return INLINE_SYSCALL_CALL (getdents64, fd, buf, nbytes);
}
libc_hidden_def (__getdents64)
weak_alias (__getdents64, getdents64)
