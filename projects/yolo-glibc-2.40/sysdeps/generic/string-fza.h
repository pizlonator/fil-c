/* Basic zero byte detection.  Generic C version.
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

#ifndef _STRING_FZA_H
#define _STRING_FZA_H 1

#include <string-misc.h>
#include <string-optype.h>

/* The function return a byte mask.  */
typedef op_t find_t;

/* This function returns non-zero if any byte in X is zero.
   More specifically, at least one bit set within the least significant
   byte that was zero; other bytes within the word are indeterminate.  */
static __always_inline find_t
find_zero_low (op_t x)
{
  /* This expression comes from
       https://graphics.stanford.edu/~seander/bithacks.html#ZeroInWord
     Subtracting 1 sets 0x80 in a byte that was 0; anding ~x clears
     0x80 in a byte that was >= 128; anding 0x80 isolates that test bit.  */
  op_t lsb = repeat_bytes (0x01);
  op_t msb = repeat_bytes (0x80);
  return (x - lsb) & ~x & msb;
}

/* This function returns at least one bit set within every byte of X that
   is zero.  The result is exact in that, unlike find_zero_low, all bytes
   are determinate.  This is usually used for finding the index of the
   most significant byte that was zero.  */
static __always_inline find_t
find_zero_all (op_t x)
{
  /* For each byte, find not-zero by
     (0) And 0x7f so that we cannot carry between bytes,
     (1) Add 0x7f so that non-zero carries into 0x80,
     (2) Or in the original byte (which might have had 0x80 set).
     Then invert and mask such that 0x80 is set iff that byte was zero.  */
  op_t m = repeat_bytes (0x7f);
  return ~(((x & m) + m) | x | m);
}

/* With similar caveats, identify bytes that are equal between X1 and X2.  */
static __always_inline find_t
find_eq_low (op_t x1, op_t x2)
{
  return find_zero_low (x1 ^ x2);
}

static __always_inline find_t
find_eq_all (op_t x1, op_t x2)
{
  return find_zero_all (x1 ^ x2);
}

/* With similar caveats, identify zero bytes in X1 and bytes that are
   equal between in X1 and X2.  */
static __always_inline find_t
find_zero_eq_low (op_t x1, op_t x2)
{
  return find_zero_low (x1) | find_zero_low (x1 ^ x2);
}

static __always_inline find_t
find_zero_eq_all (op_t x1, op_t x2)
{
  return find_zero_all (x1) | find_zero_all (x1 ^ x2);
}

/* With similar caveats, identify zero bytes in X1 and bytes that are
   not equal between in X1 and X2.  */
static __always_inline find_t
find_zero_ne_all (op_t x1, op_t x2)
{
  op_t m = repeat_bytes (0x7f);
  op_t eq = x1 ^ x2;
  op_t nz1 = ((x1 & m) + m) | x1;
  op_t ne2 = ((eq & m) + m) | eq;
  return (ne2 | ~nz1) & ~m;
}

#endif /* _STRING_FZA_H */
