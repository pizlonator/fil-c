/* Check if kernel supports PID file descriptors.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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

#include <atomic.h>
#include <sys/wait.h>
#include <sysdep.h>

/* The PID file descriptors was added during multiple releases:
   - Linux 5.2 added CLONE_PIDFD support for clone and __clone_pidfd_supported
     syscall.
   - Linux 5.3 added support for poll and CLONE_PIDFD for clone3.
   - Linux 5.4 added P_PIDFD support on waitid.

   For internal usage on spawn and fork, it only make sense to return a file
   descriptor if caller can actually waitid on it.  */

static int __waitid_pidfd_supported = 0;

bool
__clone_pidfd_supported (void)
{
  int state = atomic_load_relaxed (&__waitid_pidfd_supported);
  if (state == 0)
    {
      /* Linux define the maximum allocated file descriptor value as
	 0x7fffffc0 (from fs/file.c):

         #define __const_min(x, y) ((x) < (y) ? (x) : (y))
         unsigned int sysctl_nr_open_max =
	   __const_min(INT_MAX, ~(size_t)0/sizeof(void *)) & -BITS_PER_LONG;

	 So we can detect whether kernel supports all pidfd interfaces by
	 using a valid but never allocated file descriptor: if is not
	 supported waitid will return EINVAL, otherwise EBADF.

         Also the waitid is a cancellation entrypoint, so issue the syscall
	 directly.  */
      int r = INTERNAL_SYSCALL_CALL (waitid, P_PIDFD, INT_MAX, NULL,
				     WEXITED | WNOHANG);
      state = r == -EBADF ? 1 : -1;
      atomic_store_relaxed (&__waitid_pidfd_supported, state);
    }

  return state > 0;
}
