/* Flush a struct __printf_buffer.  Multibyte version.
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

#include "printf_buffer-char.h"
#include "Xprintf_buffer_flush.c"

/* The __printf_buffer_flush_* functions are defined together with
   functions that are pulled in by strong references.  */
#ifndef SHARED
# pragma weak __printf_buffer_flush_snprintf
# pragma weak __printf_buffer_flush_to_file
# pragma weak __printf_buffer_flush_asprintf
# pragma weak __printf_buffer_flush_dprintf
# pragma weak __printf_buffer_flush_fp
# pragma weak __printf_buffer_flush_fp_to_wide
# pragma weak __printf_buffer_flush_fphex_to_wide
# pragma weak __printf_buffer_flush_obstack
#endif /* !SHARED */

static void
__printf_buffer_do_flush (struct __printf_buffer *buf)
{
  switch (buf->mode)
    {
    case __printf_buffer_mode_failed:
    case __printf_buffer_mode_sprintf:
      return;
    case __printf_buffer_mode_snprintf:
      __printf_buffer_flush_snprintf ((struct __printf_buffer_snprintf *) buf);
      return;
    case __printf_buffer_mode_sprintf_chk:
      __chk_fail ();
      break;
    case __printf_buffer_mode_to_file:
      __printf_buffer_flush_to_file ((struct __printf_buffer_to_file *) buf);
      return;
    case __printf_buffer_mode_asprintf:
      __printf_buffer_flush_asprintf ((struct __printf_buffer_asprintf *) buf);
      return;
    case __printf_buffer_mode_dprintf:
      __printf_buffer_flush_dprintf ((struct __printf_buffer_dprintf *) buf);
      return;
    case __printf_buffer_mode_strfmon:
      __set_errno (E2BIG);
      __printf_buffer_mark_failed (buf);
      return;
    case __printf_buffer_mode_fp:
      __printf_buffer_flush_fp ((struct __printf_buffer_fp *) buf);
      return;
    case __printf_buffer_mode_fp_to_wide:
      __printf_buffer_flush_fp_to_wide
        ((struct __printf_buffer_fp_to_wide *) buf);
      return;
    case __printf_buffer_mode_fphex_to_wide:
      __printf_buffer_flush_fphex_to_wide
        ((struct __printf_buffer_fphex_to_wide *) buf);
      return;
    case __printf_buffer_mode_obstack:
      __printf_buffer_flush_obstack ((struct __printf_buffer_obstack *) buf);
      return;
    }
  __builtin_trap ();
}
