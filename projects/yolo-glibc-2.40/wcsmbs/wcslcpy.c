/* Copy a null-terminated wide string to a fixed-size buffer.
   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
   <http://www.gnu.org/licenses/>.  */

#include <wchar.h>

size_t
__wcslcpy (wchar_t *__restrict dest, const wchar_t *__restrict src, size_t size)
{
  size_t src_length = __wcslen (src);

  if (__glibc_unlikely (src_length >= size))
    {
      if (size > 0)
	{
	  /* Copy the leading portion of the string.  The last
	     character is subsequently overwritten with the null
	     terminator, but the destination size is usually a
	     multiple of a small power of two, so writing it twice
	     should be more efficient than copying an odd number of
	     character.  */
	  __wmemcpy (dest, src, size);
	  dest[size - 1] = '\0';
	}
    }
  else
    /* Copy the string and its terminating null character.  */
    __wmemcpy (dest, src, src_length + 1);
  return src_length;
}
libc_hidden_def (__wcslcpy)
weak_alias (__wcslcpy, wcslcpy)
