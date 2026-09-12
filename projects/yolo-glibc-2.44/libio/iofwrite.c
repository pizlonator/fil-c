/* Copyright (C) 1993-2026 Free Software Foundation, Inc.
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
   <https://www.gnu.org/licenses/>.

   As a special exception, if you link the code in this file with
   files compiled with a GNU compiler to produce an executable,
   that does not cause the resulting executable to be covered by
   the GNU Lesser General Public License.  This exception does not
   however invalidate any other reasons why the executable file
   might be covered by the GNU Lesser General Public License.
   This exception applies to code released by its copyright holders
   in files containing the exception.  */

#include "libioP.h"

size_t
_IO_fwrite (const void *buf, size_t size, size_t count, FILE *fp)
{
  size_t request = size * count;
  size_t written = 0;
  CHECK_FILE (fp, 0);
  if (request == 0)
    return 0;
  _IO_acquire_lock (fp);
  if (_IO_vtable_offset (fp) != 0 || _IO_fwide (fp, -1) == -1)
    {
      /* Compute actually written bytes plus pending buffer
         contents.  */
      uint64_t original_total_written
        = fp->_total_written + (fp->_IO_write_ptr - fp->_IO_write_base);
      written = _IO_sputn (fp, (const char *) buf, request);
      if (written == EOF)
        {
          /* An error happened and we need to find the appropriate return
             value.  There 3 possible scenarios:
             1. If the number of bytes written is between 0..[buffer content],
                we need to return 0 because none of the bytes from this
                request have been written;
             2. If the number of bytes written is between
                [buffer content]+1..request-1, that means we managed to write
                data requested in this fwrite call;
             3. We might have written all the requested data and got an error
                anyway.  We can't return success, which means we still have to
                return less than request.  */
          if (fp->_total_written > original_total_written)
            {
              written = fp->_total_written - original_total_written;
              /* If everything was reported as written and somehow an
                 error occurred afterwards, avoid reporting success.  */
              if (written == request)
                --written;
            }
          else
            /* Only already-pending buffer contents was written.  */
            written = 0;
        }
    }
  _IO_release_lock (fp);
  /* We have written all of the input in case the return value indicates
     this.  */
  if (written == request)
    return count;
  else
    return written / size;
}
libc_hidden_def (_IO_fwrite)

# include <stdio.h>
static_weak_alias (_IO_fwrite, fwrite)
libc_hidden_weak (fwrite)
# ifndef _IO_MTSAFE_IO
weak_alias (_IO_fwrite, fwrite_unlocked)
libc_hidden_weak (fwrite_unlocked)
# endif
