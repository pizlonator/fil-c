/* Shift unaligned word read.  Alpha version.
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

#ifndef _STRING_SHIFT_H
#define _STRING_SHIFT_H 1

#include <limits.h>
#include <stdint.h>
#include <string-fza.h>

/* Return the mask WORD shifted based on S_INT address value, to ignore
   values not presented in the aligned word read.  */
static __always_inline find_t
shift_find (find_t word, uintptr_t s)
{
  return word >> (s % sizeof (op_t));
}

/* Mask off the bits defined the the S alignment value.  */
static __always_inline find_t
shift_find_last (find_t word, uintptr_t s)
{
  s = s % sizeof (op_t);
  if (s == 0)
    return word;
  return word & ~((op_t)-1 << s);
}

#endif /* _STRING_SHIFT_H */
