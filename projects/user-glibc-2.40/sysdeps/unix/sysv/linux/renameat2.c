/* Linux implementation for renameat2 function.
   Copyright (C) 2018-2024 Free Software Foundation, Inc.
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

#include <errno.h>
#include <stdio.h>
#include <sysdep.h>
#include <pizlonated_syscalls.h>

int
__renameat2 (int oldfd, const char *old, int newfd, const char *new,
           unsigned int flags)
{
  return zsys_renameat2 (oldfd, old, newfd, new, flags);
}
libc_hidden_def (__renameat2)
weak_alias (__renameat2, renameat2)
