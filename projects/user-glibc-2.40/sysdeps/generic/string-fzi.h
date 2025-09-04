/* Zero byte detection; indexes.  Generic C version.
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

#include <limits.h>
#include <endian.h>
#include <string-fza.h>

static __always_inline int
clz (find_t c)
{
  if (sizeof (find_t) == sizeof (unsigned long))
    return __builtin_clzl (c);
  else
    return __builtin_clzll (c);
}

static __always_inline int
ctz (find_t c)
{
  if (sizeof (find_t) == sizeof (unsigned long))
    return __builtin_ctzl (c);
  else
    return __builtin_ctzll (c);
}

/* A subroutine for the index_zero functions.  Given a test word C, return
   the (memory order) index of the first byte (in memory order) that is
   non-zero.  */
static __always_inline unsigned int
index_first (find_t c)
{
  int r;
  if (__BYTE_ORDER == __LITTLE_ENDIAN)
    r = ctz (c);
  else
    r = clz (c);
  return r / CHAR_BIT;
}

/* Similarly, but return the (memory order) index of the last byte that is
   non-zero.  */
static __always_inline unsigned int
index_last (find_t c)
{
  int r;
  if (__BYTE_ORDER == __LITTLE_ENDIAN)
    r = clz (c);
  else
    r = ctz (c);
  return sizeof (find_t) - 1 - (r / CHAR_BIT);
}

#endif /* STRING_FZI_H */
