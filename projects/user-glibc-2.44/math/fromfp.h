/* Round to integer type (C23 version).  Common helper functions.
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
#include <float.h>
#include <math.h>
#include <math-barriers.h>
#include <stdbool.h>
#include <stdint.h>

/* The including file should have defined UNSIGNED to 0 (signed return
   type) or 1 (unsigned return type), INEXACT to 0 (no inexact
   exceptions) or 1 (raise inexact exceptions), FLOAT to the floating
   type for the function being defined, PREFIX to the prefix of
   <float.h> macros for that type and SUFFIX to the suffix of <math.h>
   functions for that type.  */

#define M_CONCATX(X, Y) X ## Y
#define M_CONCAT(X, Y) M_CONCATX (X, Y)
#define M_SUF(F) M_CONCAT (F, SUFFIX)
#define M_MAX_EXP M_CONCAT (PREFIX, MAX_EXP)

/* Adjust WIDTH into a range sufficient to cover all possible finite
   results from calls to this function.  */

static unsigned int
fromfp_adjust_width (unsigned int width)
{
  if (width > (UNSIGNED ? M_MAX_EXP : M_MAX_EXP + 1))
    return UNSIGNED ? M_MAX_EXP : M_MAX_EXP + 1;
  return width;
}

/* Return the maximum unbiased exponent for a rounded result (negative
   if NEGATIVE is set) that might be in range for a call to a fromfp
   function with width WIDTH (greater than 0, and not exceeding that
   returned by fromfp_adjust_width).  The result may still be out of
   range in the case of negative arguments.  */

static int
fromfp_max_exponent (bool negative, int width)
{
  if (UNSIGNED)
    return negative ? -1 : width - 1;
  else
    return negative ? width - 1 : width - 2;
}

/* Return the result of rounding an argument X in the rounding
   direction ROUND.  */

static FLOAT
fromfp_round (FLOAT x, int rm)
{
  switch (rm)
    {
    case FP_INT_UPWARD:
      return M_SUF (ceil) (x);

    case FP_INT_DOWNWARD:
      return M_SUF (floor) (x);

    case FP_INT_TOWARDZERO:
    default:
      /* Unknown rounding directions are defined to mean unspecified
	 rounding; treat this as truncation.  */
      return M_SUF (trunc) (x);

    case FP_INT_TONEARESTFROMZERO:
      return M_SUF (round) (x);

    case FP_INT_TONEAREST:
      return M_SUF (roundeven) (x);
    }
}

/* Handle a domain error for a call to a fromfp function.  */

static FLOAT
fromfp_domain_error (void)
{
  __set_errno (EDOM);
  return (FLOAT) 0 / (FLOAT) 0;
}

/* X has rounded to RX, which is within range.  Return RX, with
   "inexact" raised if appropriate.  */

static FLOAT
fromfp_return (FLOAT x, FLOAT rx)
{
  if (INEXACT && rx != x)
    {
      /* There is no need for this to use the specific floating-point
	 type for which this header is included (and indeed 1 +
	 LDBL_MIN may not raise "inexact" for IBM long double).  */
      float force_inexact = 1.0f + FLT_MIN;
      math_force_eval (force_inexact);
    }
  return rx;
}
