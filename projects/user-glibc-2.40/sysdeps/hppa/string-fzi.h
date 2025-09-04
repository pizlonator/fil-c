/* string-fzi.h -- zero byte indexes.  HPPA version.
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

#ifndef _STRING_FZI_H
#define _STRING_FZI_H 1

#include <string-optype.h>
#include <string-fza.h>

_Static_assert (sizeof (op_t) == 4, "64-bit not supported");

static __always_inline unsigned int
index_first (find_t c)
{
  unsigned int ret;

  /* Since we have no clz insn, direct tests of the bytes is faster
     than loading up the constants to do the masking.  */
  asm ("extrw,u,= %1,23,8,%%r0\n\t"
       "ldi 2,%0\n\t"
       "extrw,u,= %1,15,8,%%r0\n\t"
       "ldi 1,%0\n\t"
       "extrw,u,= %1,7,8,%%r0\n\t"
       "ldi 0,%0"
       : "=r"(ret) : "r"(c), "0"(3));

  return ret;
}

static __always_inline unsigned int
index_last (find_t c)
{
  unsigned int ret;

  /* Since we have no ctz insn, direct tests of the bytes is faster
     than loading up the constants to do the masking.  */
  asm ("extrw,u,= %1,15,8,%%r0\n\t"
       "ldi 1,%0\n\t"
       "extrw,u,= %1,23,8,%%r0\n\t"
       "ldi 2,%0\n\t"
       "extrw,u,= %1,31,8,%%r0\n\t"
       "ldi 3,%0"
       : "=r"(ret) : "r"(c), "0"(0));

  return ret;
}

#endif /* _STRING_FZI_H */
