/* Fortified version of wcslcat.
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
   <https://www.gnu.org/licenses/>.  */

#include <wchar.h>

/* Check that the user-supplied size does not exceed the
   compiler-determined size, and then forward to wcslcat.  */
size_t
__wcslcat_chk (wchar_t *__restrict s1, const wchar_t *__restrict s2,
               size_t n, size_t s1len)
{
  if (__glibc_unlikely (s1len < n))
    __chk_fail ();

  return __wcslcat (s1, s2, n);
}
