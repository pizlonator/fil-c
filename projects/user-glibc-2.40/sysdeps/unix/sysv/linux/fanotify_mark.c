/* Add, remove, or modify an fanotify mark on a filesystem object.
   Linux specific syscall.
   Copyright (C) 2021-2024 Free Software Foundation, Inc.
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

#include <sys/fanotify.h>
#include <sysdep.h>

int
__fanotify_mark (int fd, unsigned int flags, uint64_t mask, int dirfd,
	         const char *pathname)
{
  return INLINE_SYSCALL_CALL (fanotify_mark, fd, flags, SYSCALL_LL64 (mask),
			      dirfd, pathname);
}
#ifdef VERSION_fanotify_mark
# include <shlib-compat.h>
versioned_symbol (libc, __fanotify_mark, fanotify_mark,
		  VERSION_fanotify_mark);
#else
weak_alias (__fanotify_mark, fanotify_mark)
#endif
