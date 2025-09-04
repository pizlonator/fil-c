/* Copyright (C) 1994-2024 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <printf.h>
#include <printf_buffer.h>

void
__printf_buffer_flush_snprintf (struct __printf_buffer_snprintf *buf)
{
  /* Record the bytes written so far, before switching buffers.  */
  buf->base.written += buf->base.write_ptr - buf->base.write_base;

  if (buf->base.write_base != buf->discard)
    {
      /* We just finished writing the caller-supplied buffer.  Force
	 NUL termination if the string length is not zero.  */
      if (buf->base.write_base != buf->base.write_end)
	buf->base.write_end[-1] = '\0';


      /* Switch to the discard buffer.  */
      buf->base.write_base = buf->discard;
      buf->base.write_ptr = buf->discard;
      buf->base.write_end = array_end (buf->discard);
    }

  buf->base.write_base = buf->discard;
  buf->base.write_ptr = buf->discard;
}

void
__printf_buffer_snprintf_init (struct __printf_buffer_snprintf *buf,
			       char *buffer, size_t length)
{
  __printf_buffer_init (&buf->base, buffer, length,
			__printf_buffer_mode_snprintf);
  if (length > 0)
    /* Historic behavior for trivially overlapping buffers (checked by
       the test suite).  */
    *buffer = '\0';
}

int
__printf_buffer_snprintf_done (struct __printf_buffer_snprintf *buf)
{
  /* NB: Do not check for buf->base.fail here.  Write the null
     terminator even in case of errors. */

  if (buf->base.write_ptr < buf->base.write_end)
    *buf->base.write_ptr = '\0';
  else if (buf->base.write_ptr > buf->base.write_base)
    /* If write_ptr == write_base, nothing has been written.  No null
       termination is needed because of the early truncation in
       __printf_buffer_snprintf_init (the historic behavior).

       We might also be at the start of the discard buffer, but in
       this case __printf_buffer_flush_snprintf has already written
       the NUL terminator.  */
    buf->base.write_ptr[-1] = '\0';

  return __printf_buffer_done (&buf->base);
}

int
__vsnprintf_internal (char *string, size_t maxlen, const char *format,
		      va_list args, unsigned int mode_flags)
{
  struct __printf_buffer_snprintf buf;
  __printf_buffer_snprintf_init (&buf, string, maxlen);
  __printf_buffer (&buf.base, format, args, mode_flags);
  return __printf_buffer_snprintf_done (&buf);
}

int
___vsnprintf (char *string, size_t maxlen, const char *format, va_list args)
{
  return __vsnprintf_internal (string, maxlen, format, args, 0);
}
ldbl_weak_alias (___vsnprintf, __vsnprintf)
ldbl_weak_alias (___vsnprintf, vsnprintf)
