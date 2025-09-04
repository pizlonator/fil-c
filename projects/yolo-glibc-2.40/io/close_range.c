/* Close a range of file descriptors.
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

#include <errno.h>
#include <not-cancel.h>
#include <unistd.h>

/* Close the file descriptors from FIRST up to LAST, inclusive.  */
int
__close_range (unsigned int first, unsigned int last,
	       int flags)
{
  if (first > last || flags != 0)
    {
      __set_errno (EINVAL);
      return -1;
    }

  int maxfd = __getdtablesize ();
  if (maxfd == -1)
    return -1;

  for (int i = first; i <= last && i < maxfd; i++)
    __close_nocancel_nostatus (i);

  return 0;
}
libc_hidden_def (__close_range)
weak_alias (__close_range, close_range)
