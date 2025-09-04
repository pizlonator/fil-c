/* Make a new name for a file.  Linux version.
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

#include <unistd.h>
#include <fcntl.h>
#include <sysdep.h>

/* Make a link to FROM called TO.  */
int
__link (const char *from, const char *to)
{
#ifdef __NR_link
  return INLINE_SYSCALL_CALL (link, from, to);
#else
  return INLINE_SYSCALL_CALL (linkat, AT_FDCWD, from, AT_FDCWD, to, 0);
#endif
}

weak_alias (__link, link)
