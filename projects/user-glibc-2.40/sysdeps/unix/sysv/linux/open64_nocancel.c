/* Linux open syscall implementation, LFS, non-cancellable.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

#include <not-cancel.h>
#include <pizlonated_syscalls.h>

int
__open64_nocancel (const char *file, int oflag, ...)
{
  return *(int *) zcall (zsys_open, zargs ());
}

hidden_def (__open64_nocancel)

#ifdef __OFF_T_MATCHES_OFF64_T
strong_alias (__open64_nocancel, __open_nocancel)
hidden_def (__open_nocancel)
#endif
