/* Transfer data between file descriptors.  Linux version.
   Copyright (C) 2022-2024 Free Software Foundation, Inc.
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

#include <sys/sendfile.h>
#include <stddef.h>
#include <errno.h>
#include <sysdep.h>

/* Send COUNT bytes from file associated with IN_FD starting at OFFSET to
   descriptor OUT_FD.  */
ssize_t
sendfile64 (int out_fd, int in_fd, off64_t *offset, size_t count)
{
#ifndef __NR_sendfile64
# define __NR_sendfile64 __NR_sendfile
#endif
  return INLINE_SYSCALL_CALL (sendfile64, out_fd, in_fd, offset, count);
}

#ifdef __OFF_T_MATCHES_OFF64_T
strong_alias (sendfile64, sendfile)
#endif
