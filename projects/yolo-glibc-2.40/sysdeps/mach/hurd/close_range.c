/* Copyright (C) 2021-2024 Free Software Foundation, Inc.
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
#include <hurd.h>
#include <hurd/fd.h>

/* Close the file descriptors from FIRST up to LAST, inclusive.
   If CLOSE_RANGE_CLOEXEC is set in FLAGS, set the FD_CLOEXEC flag
   instead of closing.  */
int
__close_range (unsigned int first, unsigned int last,
               int flags)
{
  int i;

  if (first > last)
    return __hurd_fail (EINVAL);
  if (flags & ~CLOSE_RANGE_CLOEXEC)
    return __hurd_fail (EINVAL);

  HURD_CRITICAL_BEGIN;
  __mutex_lock (&_hurd_dtable_lock);

  for (i = first; i <= last && i < _hurd_dtablesize; i++)
    {
      struct hurd_fd *fd = _hurd_dtable[i];

      if (fd == NULL || fd->port.port == MACH_PORT_NULL)
        continue;

      __spin_lock (&fd->port.lock);

      if (flags & CLOSE_RANGE_CLOEXEC)
        fd->flags |= FD_CLOEXEC;
      else
        {
          _hurd_port_set (&fd->ctty, MACH_PORT_NULL);
          _hurd_port_locked_set (&fd->port, MACH_PORT_NULL);
        }

      __spin_unlock (&fd->port.lock);
    }

  __mutex_unlock (&_hurd_dtable_lock);
  HURD_CRITICAL_END;

  return 0;
}

libc_hidden_def (__close_range)
weak_alias (__close_range, close_range)
