/* Syscall wrapper that do not set errno.  Linux version.
   Copyright (C) 2017-2024 Free Software Foundation, Inc.
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

#include <sysdep.h>
#include <fcntl.h>

static inline int
__kill_noerrno (pid_t pid, int sig)
{
  int res;
  res = INTERNAL_SYSCALL_CALL (kill, pid, sig);
  if (INTERNAL_SYSCALL_ERROR_P (res))
    return INTERNAL_SYSCALL_ERRNO (res);
  return 0;
}
