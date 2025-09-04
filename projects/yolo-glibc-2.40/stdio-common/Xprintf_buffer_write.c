/* Blob write function for struct __*printf_buffer.  Generic version.
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

#include <printf_buffer.h>

#include <assert.h>
#include <string.h>

void
Xprintf_buffer_write (struct Xprintf_buffer *buf,
                        const CHAR_T *s, size_t count)
{
  if (__glibc_unlikely (Xprintf_buffer_has_failed (buf)))
    return;

  while (count > 0)
    {
      if (buf->write_ptr == buf->write_end && !Xprintf_buffer_flush (buf))
        return;
      assert (buf->write_ptr != buf->write_end);
      size_t to_copy = buf->write_end - buf->write_ptr;
      if (to_copy > count)
        to_copy = count;
      MEMCPY (buf->write_ptr, s, to_copy);
      buf->write_ptr += to_copy;
      s += to_copy;
      count -= to_copy;
    }
}
