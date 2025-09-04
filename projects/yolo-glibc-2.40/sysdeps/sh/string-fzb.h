/* Zero byte detection; boolean.  SH4 version.
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

#ifndef STRING_FZB_H
#define STRING_FZB_H 1

#include <sys/cdefs.h>
#include <string-optype.h>

/* Determine if any bytes within X1 and X2 are equal.  */
static __always_inline _Bool
has_eq (op_t x1, op_t x2)
{
  int ret;

  /* TODO: A compiler builtin for cmp/str would be much better.  It is
     difficult to use asm goto here, because the range of bt/bf are
     quite small.  */
  asm("cmp/str %1,%2\n\t"
      "movt %0"
      : "=r" (ret) : "r" (x1), "r" (x2) : "t");

  return ret;
}

/* Determine if any byte within X is zero.  */
static __always_inline _Bool
has_zero (op_t x)
{
  return has_eq (x, 0);
}

/* Likewise, but for zeros in X1 and equal bytes between X1 and X2.  */
static __always_inline _Bool
has_zero_eq (op_t x1, op_t x2)
{
  return has_zero (x1) | has_eq (x1, x2);
}

#endif /* STRING_FZB_H */
