/* Round to integer type (C23 version).  ldbl-96 version.
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
#include <libm-alias-ldouble.h>
#include <stdbool.h>
#include <stdint.h>
#include <shlib-compat.h>

#define FLOAT long double
#define PREFIX LDBL_
#define SUFFIX l
#define BIAS 0x3fff
#define MANT_DIG 64

#include <fromfp.h>

long double
FUNC (long double x, int round, unsigned int width)
{
  width = fromfp_adjust_width (width);
  long double rx = fromfp_round (x, round);
  if (width == 0 || !isfinite (rx))
    return fromfp_domain_error ();
  uint16_t se;
  uint32_t hx, lx;
  GET_LDOUBLE_WORDS (se, hx, lx, rx);
  bool negative = (se & 0x8000) != 0;
  int exponent = se & 0x7fff;
  exponent -= BIAS;
  int max_exponent = fromfp_max_exponent (negative, width);
  uint64_t ix = (((uint64_t) hx) << 32) | lx;
  if (exponent > max_exponent
      || (!UNSIGNED
	  && negative
	  && exponent == max_exponent
	  && ix != 0x8000000000000000ULL))
    return fromfp_domain_error ();
  return fromfp_return (x, rx);
}
