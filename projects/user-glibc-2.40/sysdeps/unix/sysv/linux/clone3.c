/* Linux close syscall implementation.
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

#include <unistd.h>
#include <sched.h>
#include <stdfil.h>
#include <clone3.h>
#include <clone_internal.h>

/* Close the file descriptor FD.  */
int
__clone3 (struct clone_args *cl_args, size_t size,
          int (*__func) (void *__arg), void *__arg)
{
  zerror("clone3 not implemented.");
  return -1;
}
libc_hidden_def (__clone3)
strong_alias (__clone3, __libc_clone3)
weak_alias (__clone3, clone3)
