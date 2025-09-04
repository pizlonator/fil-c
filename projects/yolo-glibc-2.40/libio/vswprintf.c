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

#include <errno.h>
#include <math_ldbl_opt.h>
#include <printf.h>
#include <printf_buffer.h>

int
__vswprintf_internal (wchar_t *string, size_t maxlen, const wchar_t *format,
		      va_list args, unsigned int mode_flags)
{
  if (maxlen == 0)
    /* Since we have to write at least the terminating L'\0' a buffer
       length of zero always makes the function fail.  */
    return -1;

  struct __wprintf_buffer buf;
  __wprintf_buffer_init (&buf, string, maxlen, __wprintf_buffer_mode_swprintf);

  __wprintf_buffer (&buf, format, args, mode_flags);

  if (buf.write_ptr == buf.write_end)
    {
      /* Buffer has been filled exactly, excluding the null wide
	 character.  This is an error because the null wide character
	 is required.  */
      buf.write_end[-1] = L'\0';
      return -1;
    }

  buf.write_ptr[0] = L'\0';

  return __wprintf_buffer_done (&buf);
}

int
__vswprintf (wchar_t *string, size_t maxlen, const wchar_t *format,
	     va_list args)
{
  return __vswprintf_internal (string, maxlen, format, args, 0);
}
ldbl_weak_alias (__vswprintf, vswprintf)
