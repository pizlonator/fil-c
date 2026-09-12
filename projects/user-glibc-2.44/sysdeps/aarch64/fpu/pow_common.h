/* Common helper functions for double-precision pow variants.

   Copyright (C) 2026 Free Software Foundation, Inc.
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

#ifndef _AARCH64_GLIBC_POW_COMMON_H
#define _AARCH64_GLIBC_POW_COMMON_H

#include <math.h>
#include <stdint.h>

#include "math_config.h"

/* Top 12 bits of a double (sign and exponent bits).  */
static inline uint32_t
top12 (double x)
{
  return asuint64 (x) >> 52;
}

/* Returns 0 if not int, 1 if odd int, 2 if even int.  The argument is
   the bit representation of a non-zero finite floating-point value.  */
static inline int
checkint (uint64_t iy)
{
  int e = iy >> 52 & 0x7ff;
  if (e < 0x3ff)
    return 0;
  if (e > 0x3ff + 52)
    return 2;
  if (iy & ((1ULL << (0x3ff + 52 - e)) - 1))
    return 0;
  if (iy & (1ULL << (0x3ff + 52 - e)))
    return 1;
  return 2;
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline int
zeroinfnan (uint64_t i)
{
  return 2 * i - 1 >= 2 * asuint64 (INFINITY) - 1;
}

#endif
