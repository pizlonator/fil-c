/* Euclidean distance function.  Double/Binary64 version.
   Copyright (C) 2021-2026 Free Software Foundation, Inc.
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

/* The implementation uses a correction based on 'An Improved Algorithm for
   hypot(a,b)' by Carlos F. Borges [1] usingthe MyHypot3 with the following
   changes:

   - Handle qNaN and sNaN.
   - Tune the 'widely varying operands' to avoid spurious underflow
     due the multiplication and fix the return value for upwards
     rounding mode.
   - Handle required underflow exception for subnormal results.

   The expected ULP is ~0.792 or ~0.948 if FMA is used.  For FMA, the
   correction is not used and the error of sqrt (x^2 + y^2) is below 1 ULP
   if x^2 + y^2 is computed with less than 0.707 ULP error.  If |x| >= |2y|,
   fma (x, x, y^2) has ~0.625 ULP.  If |x| < |2y|, fma (|2x|, |y|, (x - y)^2)
   has ~0.625 ULP.

   [1] https://arxiv.org/pdf/1904.09481.pdf  */

#include <errno.h>
#include <math.h>
#include <math_private.h>
#include <math-underflow.h>
#include <math-narrow-eval.h>
#include <math-svid-compat.h>
#include <libm-alias-finite.h>
#include <libm-alias-double.h>
#include "math_config.h"

#define SCALE     0x1p-600
#define LARGE_VAL 0x1p+511
#define TINY_VAL  0x1p-459
#define EPS       0x1p-54

static inline double
handle_errno (double r)
{
  if (isinf (r))
    __set_errno (ERANGE);
  return r;
}

/* Hypot kernel. The inputs must be adjusted so that ax >= ay >= 0
   and squaring ax, ay and (ax - ay) does not overflow or underflow.  */
static inline double
kernel (double ax, double ay)
{
  double t1, t2;
#ifdef __FP_FAST_FMA
  t1 = ay + ay;
  t2 = ax - ay;

  if (t1 >= ax)
    return sqrt (fma (t1, ax, t2 * t2));
  else
    return sqrt (fma (ax, ax, ay * ay));

#else
  double h = sqrt (ax * ax + ay * ay);
  if (h <= 2.0 * ay)
    {
      double delta = h - ay;
      t1 = ax * (2.0 * delta - ax);
      t2 = (delta - 2.0 * (ax - ay)) * delta;
    }
  else
    {
      double delta = h - ax;
      t1 = 2.0 * delta * (ax - 2.0 * ay);
      t2 = (4.0 * delta - ay) * ay + delta * delta;
    }

  h -= (t1 + t2) / (2.0 * h);
  return h;
#endif
}

double
__hypot (double x, double y)
{
  if (!isfinite(x) || !isfinite(y))
    {
      if ((isinf (x) || isinf (y))
	  && !issignaling (x) && !issignaling (y))
	return INFINITY;
      return x + y;
    }

  x = fabs (x);
  y = fabs (y);

  double ax = USE_FMAX_BUILTIN ? fmax (x, y) : (x < y ? y : x);
  double ay = USE_FMIN_BUILTIN ? fmin (x, y) : (x < y ? x : y);

  /* If ax is huge, scale both inputs down.  */
  if (__glibc_unlikely (ax > LARGE_VAL))
    {
      if (__glibc_unlikely (ay <= ax * EPS))
	return handle_errno (math_narrow_eval (ax + ay));

      return handle_errno (math_narrow_eval (kernel (ax * SCALE, ay * SCALE)
					     / SCALE));
    }

  /* If ay is tiny, scale both inputs up.  */
  if (__glibc_unlikely (ay < TINY_VAL))
    {
      if (__glibc_unlikely (ax >= ay / EPS))
	return math_narrow_eval (ax + ay);

      ax = math_narrow_eval (kernel (ax / SCALE, ay / SCALE) * SCALE);
      math_check_force_underflow_nonneg (ax);
      return ax;
    }

  /* Common case: ax is not huge and ay is not tiny.  */
  if (__glibc_unlikely (ay <= ax * EPS))
    return ax + ay;

  return kernel (ax, ay);
}
strong_alias (__hypot, __ieee754_hypot)
libm_alias_finite (__ieee754_hypot, __hypot)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __hypot, hypot, GLIBC_2_35);
libm_alias_double_other (__hypot, hypot)
#else
libm_alias_double (__hypot, hypot)
#endif
