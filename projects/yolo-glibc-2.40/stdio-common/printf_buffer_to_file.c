/* Multibyte printf buffers writing data to a FILE * stream.
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

#include <printf_buffer_to_file.h>

#include <assert.h>
#include <array_length.h>
#include <libio/libioP.h>

/* Switch to the file buffer if possible.  If the file has write_ptr
   == write_end, use the stage buffer instead.  */
void
__printf_buffer_to_file_switch (struct __printf_buffer_to_file *buf)
{
  if (buf->fp->_IO_write_ptr < buf->fp->_IO_write_end)
    {
      /* buf->fp has a buffer associated with it, so write directly to
         it from now on.  */
      buf->base.write_ptr = buf->fp->_IO_write_ptr;
      buf->base.write_end = buf->fp->_IO_write_end;
    }
  else
    {
      /* Use the staging area if no buffer is available in buf->fp.  */
      buf->base.write_ptr = buf->stage;
      buf->base.write_end = array_end (buf->stage);
    }

  buf->base.write_base = buf->base.write_ptr;
}

void
__printf_buffer_flush_to_file (struct __printf_buffer_to_file *buf)
{
  /* The bytes in the buffer are always consumed.  */
  buf->base.written += buf->base.write_ptr - buf->base.write_base;

  if (buf->base.write_end == array_end (buf->stage))
    {
      /* If the stage buffer is used, make a copy into the file.  The
         stage buffer is always consumed fully, even if just partially
         written, to ensure that the file stream has all the data.  */
      size_t count = buf->base.write_ptr - buf->stage;
      if ((size_t) _IO_sputn (buf->fp, buf->stage, count) != count)
        {
          __printf_buffer_mark_failed (&buf->base);
          return;
        }
      /* buf->fp may have a buffer now.  */
      __printf_buffer_to_file_switch (buf);
      return;
    }
  else if (buf->base.write_end == buf->stage + 1)
    {
      /* Special one-character buffer case.  This is used to avoid
         flush-only overflow below.  */
      if (buf->base.write_ptr == buf->base.write_end)
        {
          if (__overflow (buf->fp, (unsigned char) *buf->stage) == EOF)
            {
              __printf_buffer_mark_failed (&buf->base);
              return;
            }
          __printf_buffer_to_file_switch (buf);
        }
      /* Else there is nothing to write.  */
      return;
    }

  /* We have written directly into the buf->fp buffer.  */
  assert (buf->base.write_end == buf->fp->_IO_write_end);

  /* Mark the bytes as written.  */
  buf->fp->_IO_write_ptr = buf->base.write_ptr;

  if (buf->base.write_ptr == buf->base.write_end)
    {
      /* The buffer in buf->fp has been filled.  This should just call
         __overflow (buf->fp, EOF), but flush-only overflow is obscure
         and not always correctly implemented.  See bug 28949.  Be
         conservative and switch to a one-character buffer instead, to
         obtain one more character for a regular __overflow call.  */
      buf->base.write_ptr = buf->stage;
      buf->base.write_end = buf->stage + 1;
    }
  /* The bytes in the file stream were already marked as written above.  */

  buf->base.write_base = buf->base.write_ptr;
}

void
__printf_buffer_to_file_init (struct __printf_buffer_to_file *buf, FILE *fp)
{
  __printf_buffer_init (&buf->base, buf->stage, array_length (buf->stage),
                        __printf_buffer_mode_to_file);
  buf->fp = fp;
  __printf_buffer_to_file_switch (buf);
}

int
__printf_buffer_to_file_done (struct __printf_buffer_to_file *buf)
{
  if (__printf_buffer_has_failed (&buf->base))
    return -1;
  __printf_buffer_flush_to_file (buf);
  return __printf_buffer_done (&buf->base);
}
