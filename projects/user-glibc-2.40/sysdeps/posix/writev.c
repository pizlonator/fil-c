/* Copyright (C) 1991-2024 Free Software Foundation, Inc.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <scratch_buffer.h>
#include <stdbool.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <errno.h>


static void
ifree (struct scratch_buffer *sbuf)
{
   scratch_buffer_free (sbuf);
}

/* Write data pointed by the buffers described by VECTOR, which
   is a vector of COUNT 'struct iovec's, to file descriptor FD.
   The data is written in the order specified.
   Operates just like 'write' (see <unistd.h>) except that the data
   are taken from VECTOR instead of a contiguous buffer.  */
ssize_t
__writev (int fd, const struct iovec *vector, int count)
{
  /* Find the total number of bytes to be written.  */
  size_t bytes = 0;
  for (int i = 0; i < count; ++i)
    {
      /* Check for ssize_t overflow.  */
      if (SSIZE_MAX - bytes < vector[i].iov_len)
	{
	  __set_errno (EINVAL);
	  return -1;
	}
      bytes += vector[i].iov_len;
    }

  /* Allocate a temporary buffer to hold the data.  Use a scratch_buffer
     since it's faster for small buffer sizes but can handle larger
     allocations as well.  */

  struct scratch_buffer __attribute__ ((__cleanup__ (ifree))) buf;
  scratch_buffer_init (&buf);
  if (!scratch_buffer_set_array_size (&buf, 1, bytes))
    /* XXX I don't know whether it is acceptable to try writing
       the data in chunks.  Probably not so we just fail here.  */
    return -1;
  char *buffer = buf.data;

  /* Copy the data into BUFFER.  */
  size_t to_copy = bytes;
  char *bp = buffer;
  for (int i = 0; i < count; ++i)
    {
      size_t copy = MIN (vector[i].iov_len, to_copy);

      bp = __mempcpy ((void *) bp, (void *) vector[i].iov_base, copy);

      to_copy -= copy;
      if (to_copy == 0)
	break;
    }

  ssize_t bytes_written = __write (fd, buffer, bytes);

  return bytes_written;
}
libc_hidden_def (__writev)
#ifndef __writev
weak_alias (__writev, writev)
#endif
