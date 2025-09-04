/* FILE * interface to a struct __wprintf_buffer.  Wide version.
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

#include <printf_buffer_as_file.h>

#include <assert.h>
#include <printf_buffer.h>

/* Commit the data directly written through the stdio stream.  */
static void
__wprintf_buffer_as_file_commit (struct __wprintf_buffer_as_file *file)
{
  /* Check that the write pointers in the file stream are consistent
     with the next buffer.  */
  assert (file->wide_stream._IO_write_ptr >= file->next->write_ptr);
  assert (file->wide_stream._IO_write_ptr <= file->next->write_end);
  assert (file->wide_stream._IO_write_base == file->next->write_base);
  assert (file->wide_stream._IO_write_end == file->next->write_end);

  file->next->write_ptr = file->wide_stream._IO_write_ptr;
}

/* Pointer the FILE * write buffer into the active struct __wprintf_buffer
   area.  */
static void
__wprintf_buffer_as_file_switch_to_buffer (struct __wprintf_buffer_as_file *file)
{
  file->wide_stream._IO_write_base = file->next->write_base;
  file->wide_stream._IO_write_ptr = file->next->write_ptr;
  file->wide_stream._IO_write_end = file->next->write_end;
}

/* Only a small subset of the vtable functions is implemented here,
   following _IO_obstack_jumps.  */

wint_t
__wprintf_buffer_as_file_overflow (FILE *fp, int ch)
{
  struct __wprintf_buffer_as_file *file
    = (struct __wprintf_buffer_as_file *) fp;

  __wprintf_buffer_as_file_commit (file);

  /* EOF means only a flush is requested.   */
  if (ch != WEOF)
    __wprintf_buffer_putc (file->next, ch);
  else
    ch = 0;

  /* Ensure that flushing actually produces room.  */
  if (!__wprintf_buffer_has_failed (file->next)
      && file->next->write_ptr == file->next->write_end)
    __wprintf_buffer_flush (file->next);

  __wprintf_buffer_as_file_switch_to_buffer (file);

  if (!__wprintf_buffer_has_failed (file->next))
    return (unsigned char) ch;
  else
    return WEOF;
}

size_t
__wprintf_buffer_as_file_xsputn (FILE *fp, const void *buf, size_t len)
{
  struct __wprintf_buffer_as_file *file
    = (struct __wprintf_buffer_as_file *) fp;

  __wprintf_buffer_as_file_commit (file);

  /* Copy the data.  */
  __wprintf_buffer_write (file->next, buf, len);

  __wprintf_buffer_as_file_switch_to_buffer (file);

  if (!__wprintf_buffer_has_failed (file->next))
    return len;
  else
    /* We may actually have written something.  But the stream is
       corrupted in this case anyway, so try not to divine the write
       count here.  */
    return 0;
}

void
__wprintf_buffer_as_file_init (struct __wprintf_buffer_as_file *file,
                               struct __wprintf_buffer *next)
{
  file->stream._lock = NULL;
  _IO_no_init (&file->stream, _IO_USER_LOCK, 0, &file->wide_stream,
               &_IO_wprintf_buffer_as_file_jumps);
  _IO_fwide (&file->stream, 1);

  /* Set up the write buffer from the next buffer.  */
  file->next = next;
  __wprintf_buffer_as_file_switch_to_buffer (file);

  /* Mark the read area as inactive, by making all pointers equal.  */
  file->stream._IO_read_base = file->stream._IO_write_base;
  file->stream._IO_read_ptr = file->stream._IO_write_base;
  file->stream._IO_read_end = file->stream._IO_write_base;
}

bool
__wprintf_buffer_as_file_terminate (struct __wprintf_buffer_as_file *file)
{
  if (file->stream._flags & _IO_ERR_SEEN)
    return false;
  else
    {
      __wprintf_buffer_as_file_commit (file);
      return true;
    }
}
