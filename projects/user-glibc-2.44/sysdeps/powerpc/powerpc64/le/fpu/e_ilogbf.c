/* Get integer exponent of a floating-point value.
   Copyright (C) 1999-2026 Free Software Foundation, Inc.
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

#include <limits.h>
#include <math.h>
#include <stdbit.h>
#include "sysdeps/ieee754/flt-32/math_config.h"

int
__ieee754_ilogbf (float x)
{
  uint32_t ux = asuint (x);
  int ex = (ux & ~SIGN_MASK) >> MANTISSA_WIDTH;
  if (ex == 0) /* zero or subnormal */
    {
      /* Clear sign and exponent.  */
      ux <<= 1 + EXPONENT_WIDTH;
      if (ux == 0)
	return FP_ILOGB0;
      /* sbunormal */
      return -127 - stdc_leading_zeros (ux);
    }
  if (ex == EXPONENT_MASK >> MANTISSA_WIDTH) /* NaN or Inf */
    return ux << (1 + EXPONENT_WIDTH) ? FP_ILOGBNAN : INT_MAX;
  return ex - 127;
}
