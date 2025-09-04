/* Flush a struct __wprintf_buffer.  Wide version.
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
#include <printf_buffer.h>

#include "printf_buffer-wchar_t.h"
#include "Xprintf_buffer_flush.c"

static void
__wprintf_buffer_do_flush (struct __wprintf_buffer *buf)
{
  switch (buf->mode)
    {
    case __wprintf_buffer_mode_failed:
      return;
    case __wprintf_buffer_mode_to_file:
      __wprintf_buffer_flush_to_file ((struct __wprintf_buffer_to_file *) buf);
      return;
    case __wprintf_buffer_mode_swprintf:
      buf->write_end[-1] = L'\0';
      __set_errno (E2BIG);
      __wprintf_buffer_mark_failed (buf);
      return;
    }
  __builtin_trap ();
}
