/* Final status reporting for struct __*printf_buffer.  Generic version.
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

#include <errno.h>
#include <intprops.h>
#include <stdint.h>

int
Xprintf_buffer_done (struct Xprintf_buffer *buf)
{
  if (Xprintf_buffer_has_failed (buf))
    return -1;

  /* Use uintptr_t here because for sprintf, the buffer range may
     cover more than half of the address space.  */
  uintptr_t written_current = buf->write_ptr - buf->write_base;
  int written_total;
  if (INT_ADD_WRAPV (buf->written, written_current, &written_total))
    {
      __set_errno (EOVERFLOW);
      return -1;
    }
  else
    return written_total;
}
