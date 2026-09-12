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

int
__inet_pton_chk (int af, const char *src, void *dst, size_t dst_size)
{
  if ((af == AF_INET && dst_size < 4)
      || (af == AF_INET6 && dst_size < 16))
    __chk_fail ();

  return __inet_pton (af, src, dst);
}
libc_hidden_def (__inet_pton_chk)
