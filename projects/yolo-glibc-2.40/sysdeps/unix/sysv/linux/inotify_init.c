/* Initialize an inotify instance.  Linux version.
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

#include <sys/inotify.h>
#include <sysdep.h>

libc_hidden_proto (inotify_init)

int
inotify_init (void)
{
#ifdef __NR_inotify_init
  return INLINE_SYSCALL_CALL (inotify_init);
#else
  return INLINE_SYSCALL_CALL (inotify_init1, 0);
#endif
}
libc_hidden_def (inotify_init)
