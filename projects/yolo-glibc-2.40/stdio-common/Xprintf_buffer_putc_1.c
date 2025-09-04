/* Overflow write function for struct __*printf_buffer.  Generic version.
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

#include <assert.h>

void
Xprintf (buffer_putc_1) (struct Xprintf_buffer *buf, CHAR_T ch)
{
  if (__glibc_unlikely (Xprintf_buffer_has_failed (buf))
      || !Xprintf_buffer_flush (buf))
    return;
  assert (buf->write_ptr < buf->write_end);
  *buf->write_ptr++ = ch;
}
