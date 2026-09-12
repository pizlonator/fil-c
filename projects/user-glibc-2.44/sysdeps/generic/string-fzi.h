/* Zero byte detection; indexes.  Generic C version.
   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
#include <string-bitops.h>
#include <stdbit.h>

static __always_inline unsigned int
ctzb (find_t c)
{
#if HAVE_BITOPTS_WORKING
  return stdc_trailing_zeros (c) / CHAR_BIT;
#else
  if (sizeof (find_t) <= 4)
    return (((c & -c) >> 7) * 0x00010203) >> 24;
  else
    return (((c & -c) >> 7) * 0x0001020304050607UL) >> 56;
#endif
}

static __always_inline unsigned int
clzb (find_t c)
{
#if HAVE_BITOPTS_WORKING
  return stdc_leading_zeros (c) / CHAR_BIT;
#else
#  if ULONG_MAX == 0xFFFFFFFFUL
  c |= c >> 8;
  c |= c >> 16;
  return ((c >> 7) * 0x30f0f0f0) >> 28;
#  else
  c |= c >> 8;
  c |= c >> 16;
  c |= c >> 32;
  return ((c >> 7) * 0x70f0f0f0f0f0f0f0UL) >> 60;
#  endif
#endif
}

/* A subroutine for the index_zero functions.  Given a test word C, return
   the (memory order) index of the first byte (in memory order) that is
   non-zero.  */
static __always_inline unsigned int
index_first (find_t c)
{
  unsigned int r;
  if (__BYTE_ORDER == __LITTLE_ENDIAN)
    r = ctzb (c);
  else
    r = clzb (c);
  return r;
}

/* Similarly, but return the (memory order) index of the last byte that is
   non-zero.  */
static __always_inline unsigned int
index_last (find_t c)
{
  unsigned int r;
  if (__BYTE_ORDER == __LITTLE_ENDIAN)
    r = clzb (c);
  else
    r = ctzb (c);
  return sizeof (find_t) - 1 - r;
}

#endif /* STRING_FZI_H */
