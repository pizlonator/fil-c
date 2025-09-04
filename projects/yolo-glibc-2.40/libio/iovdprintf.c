/* Copyright (C) 1995-2024 Free Software Foundation, Inc.
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

#include <array_length.h>
#include <math_ldbl_opt.h>
#include <printf.h>
#include <stdio_ext.h>
#include <unistd.h>
#include <printf_buffer.h>

struct __printf_buffer_dprintf
{
  struct __printf_buffer base;
  int fd;

  char buf[PRINTF_BUFFER_SIZE_DPRINTF];
};

void
__printf_buffer_flush_dprintf (struct __printf_buffer_dprintf *buf)
{
  char *p = buf->buf;
  char *end = buf->base.write_ptr;
  while (p < end)
    {
      ssize_t ret = TEMP_FAILURE_RETRY (write (buf->fd, p, end - p));
      if (ret < 0)
	{
	  __printf_buffer_mark_failed (&buf->base);
	  return;
	}
      p += ret;
    }
  buf->base.written += buf->base.write_ptr - buf->base.write_base;
  buf->base.write_ptr = buf->buf;
}

int
__vdprintf_internal (int d, const char *format, va_list arg,
		     unsigned int mode_flags)
{
  struct __printf_buffer_dprintf buf;
  __printf_buffer_init (&buf.base, buf.buf, array_length (buf.buf),
			__printf_buffer_mode_dprintf);
  buf.fd = d;
  __printf_buffer (&buf.base, format, arg, mode_flags);
  if (__printf_buffer_has_failed (&buf.base))
    return -1;
  __printf_buffer_flush_dprintf (&buf);
  return __printf_buffer_done (&buf.base);
}

int
__vdprintf (int d, const char *format, va_list arg)
{
  return __vdprintf_internal (d, format, arg, 0);
}
ldbl_weak_alias (__vdprintf, vdprintf)
