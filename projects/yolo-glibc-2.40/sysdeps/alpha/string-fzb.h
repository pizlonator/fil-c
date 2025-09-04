/* Zero byte detection; boolean.  Alpha version.
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

#ifndef _STRING_FZB_H
#define _STRING_FZB_H 1

#include <sys/cdefs.h>
#include <string-optype.h>

/* Note that since CMPBGE creates a bit mask rather than a byte mask,
   we cannot simply provide a target-specific string-fza.h.  */

/* Determine if any byte within X is zero.  This is a pure boolean test.  */

static __always_inline _Bool
has_zero (op_t x)
{
  return __builtin_alpha_cmpbge (0, x) != 0;
}

/* Likewise, but for byte equality between X1 and X2.  */

static __always_inline _Bool
has_eq (op_t x1, op_t x2)
{
  return has_zero (x1 ^ x2);
}

/* Likewise, but for zeros in X1 and equal bytes between X1 and X2.  */

static __always_inline _Bool
has_zero_eq (op_t x1, op_t x2)
{
  return has_zero (x1) | has_eq (x1, x2);
}

#endif /* _STRING_FZB_H */
