/* Copyright (C) 2011-2024 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <sys/select.h>


long int
__fdelt_chk (long int d)
{
  if (d < 0 || d >= FD_SETSIZE)
    __fortify_fail ("bit out of range 0 - FD_SETSIZE on fd_set");

  return d / __NFDBITS;
}
strong_alias (__fdelt_chk, __fdelt_warn)
libc_hidden_def (__fdelt_chk)
