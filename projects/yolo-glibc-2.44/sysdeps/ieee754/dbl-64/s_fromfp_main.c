/* Round to integer type (C23 version).  dbl-64 version.
   Copyright (C) 2016-2026 Free Software Foundation, Inc.
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

#include <errno.h>
#include <fenv.h>
#include <math.h>
#include <math_private.h>
#include <libm-alias-double.h>
#include <stdbool.h>
#include <stdint.h>
#include <shlib-compat.h>

#define FLOAT double
#define PREFIX DBL_
#define SUFFIX
#define BIAS 0x3ff
#define MANT_DIG 53

#include <fromfp.h>

double
FUNC (double x, int round, unsigned int width)
{
  width = fromfp_adjust_width (width);
  double rx = fromfp_round (x, round);
  if (width == 0 || !isfinite (rx))
    return fromfp_domain_error ();
  uint64_t ix;
  EXTRACT_WORDS64 (ix, rx);
  bool negative = (ix & 0x8000000000000000ULL) != 0;
  ix &= 0x7fffffffffffffffULL;
  int exponent = ix >> (MANT_DIG - 1);
  exponent -= BIAS;
  int max_exponent = fromfp_max_exponent (negative, width);
  ix &= ((1ULL << (MANT_DIG - 1)) - 1);
  if (exponent > max_exponent
      || (!UNSIGNED && negative && exponent == max_exponent && ix != 0))
    return fromfp_domain_error ();
  return fromfp_return (x, rx);
}
