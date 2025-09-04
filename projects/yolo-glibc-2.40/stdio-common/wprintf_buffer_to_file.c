/* Wide printf buffers writing data to a FILE *.
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
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* This implementation is not optimized (unlikely the multibyte
   implementation) and always writes to the temporary buffer first.  */

#include <printf_buffer_to_file.h>

#include <array_length.h>
#include <libio/libioP.h>

void
__wprintf_buffer_flush_to_file (struct __wprintf_buffer_to_file *buf)
{
  size_t count = buf->base.write_ptr - buf->stage;
  if ((size_t) _IO_sputn (buf->fp, buf->stage, count) != count)
    {
      __wprintf_buffer_mark_failed (&buf->base);
      return;
    }
  buf->base.written += count;
  buf->base.write_ptr = buf->stage;
}

void
__wprintf_buffer_to_file_init (struct __wprintf_buffer_to_file *buf, FILE *fp)
{
  __wprintf_buffer_init (&buf->base, buf->stage, array_length (buf->stage),
                         __wprintf_buffer_mode_to_file);
  buf->fp = fp;
}

int
__wprintf_buffer_to_file_done (struct __wprintf_buffer_to_file *buf)
{
  if (__wprintf_buffer_has_failed (&buf->base))
    return -1;
  __wprintf_buffer_flush_to_file (buf);
  return __wprintf_buffer_done (&buf->base);
}
