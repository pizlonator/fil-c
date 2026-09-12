/* Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include <arpa/inet.h>
#include <stdio.h>

const char *
__inet_ntop_chk (int af, const void *src, char *dst,
		 socklen_t size, size_t dst_size)
{
  if (size > dst_size)
    __chk_fail ();

  return __inet_ntop (af, src, dst, size);
}
libc_hidden_def (__inet_ntop_chk)
