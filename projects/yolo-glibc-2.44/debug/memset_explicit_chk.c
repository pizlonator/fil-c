/* Generic implementation of __memset_explicit_chk.
   Copyright (C) 1991-2026 Free Software Foundation, Inc.
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

/* This is the generic definition of __memset_explicit_chk.  The
   __memset_explicit_chk symbol is used as the implementation of
   memset_explicit throughout glibc.  If this file is overridden by an
   architecture, both __memset_explicit_chk and
   __memset_explicit_chk_internal have to be defined (the latter not as
   an IFUNC).  */

#include <string.h>

void *
__memset_explicit_chk (void *dst, int c, size_t len, size_t dstlen)
{
  /* Inline __memset_chk to avoid a PLT reference to __memset_chk.  */
  if (__glibc_unlikely (dstlen < len))
    __chk_fail ();
  memset (dst, c, len);
  /* Compiler barrier.  */
  asm volatile ("" ::: "memory");
  return dst;
}

/* libc-internal references use the hidden
   __memset_explicit_chk_internal symbol.  This is necessary if
   __memset_explicit_chk is implemented as an IFUNC because some
   targets do not support hidden references to IFUNC symbols.  */
strong_alias (__memset_explicit_chk, __memset_explicit_chk_internal)
