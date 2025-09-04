/* Zero byte detection; basics.  ARM version.
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

/* The functions return a byte mask.  */
typedef op_t find_t;

/* This function returns at least one bit set within every byte
   of X that is zero.  */
static __always_inline find_t
find_zero_all (op_t x)
{
  /* Use unsigned saturated subtraction from 1 in each byte.
     That leaves 1 for every byte that was zero.  */
  op_t ones = repeat_bytes (0x01);
  op_t ret;
  asm ("uqsub8 %0,%1,%2" : "=r"(ret) : "r"(ones), "r"(x));
  return ret;
}

/* Identify bytes that are equal between X1 and X2.  */
static __always_inline find_t
find_eq_all (op_t x1, op_t x2)
{
  return find_zero_all (x1 ^ x2);
}

/* Identify zero bytes in X1 or equality between X1 and X2.  */
static __always_inline find_t
find_zero_eq_all (op_t x1, op_t x2)
{
  return find_zero_all (x1) | find_zero_all (x1 ^ x2);
}

/* Identify zero bytes in X1 or inequality between X1 and X2.  */
static __always_inline find_t
find_zero_ne_all (op_t x1, op_t x2)
{
  /* Make use of the fact that we'll already have ONES in a register.  */
  op_t ones = repeat_bytes (0x01);
  return find_zero_all (x1) | (find_zero_all (x1 ^ x2) ^ ones);
}

/* Define the "inexact" versions in terms of the exact versions.  */
#define find_zero_low		find_zero_all
#define find_eq_low		find_eq_all
#define find_zero_eq_low	find_zero_eq_all

#endif /* _STRING_FZA_H */
