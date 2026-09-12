/* Copyright (C) 2012-2026 Free Software Foundation, Inc.
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

#include <math.h>
#include <math-barriers.h>
#include <math-narrow-eval.h>
#include <math-svid-compat.h>
#include <libm-alias-finite.h>
#include <libm-alias-double.h>
#include "math_config.h"

#define N (1 << EXP_TABLE_BITS)
#define IndexMask (N - 1)
#define OFlowBound 0x1.34413509f79ffp8 /* log10(DBL_MAX).  */
#define UFlowBound -0x1.5ep+8 /* -350.  */
#define SmallTop 0x3c6 /* top12(0x1p-57).  */
#define BigTop 0x407   /* top12(0x1p8).  */
#define Thresh 0x41    /* BigTop - SmallTop.  */
#define Shift __exp_data.shift
#define C(i) __exp_data.exp10_poly[i]

static double
special_case (uint64_t sbits, double tmp, uint64_t ki)
{
  double scale, y;

  if ((ki & 0x80000000) == 0)
    {
      /* The exponent of scale might have overflowed by 1.  */
      sbits -= 1ull << 52;
      scale = asdouble (sbits);
      y = 2 * (scale + scale * tmp);
      return check_oflow (y);
    }

  /* n < 0, need special care in the subnormal range.  */
  sbits += 1022ull << 52;
  scale = asdouble (sbits);
  y = scale + scale * tmp;

  if (y < 1.0)
    {
      /* Round y to the right precision before scaling it into the subnormal
	 range to avoid double rounding that can cause 0.5+E/2 ulp error where
	 E is the worst-case ulp error outside the subnormal range.  So this
	 is only useful if the goal is better than 1 ulp worst-case error.  */
      double lo = scale - y + scale * tmp;
      double hi = 1.0 + y;
      lo = 1.0 - hi + y + lo;
      y = math_narrow_eval (hi + lo) - 1.0;
      /* Avoid -0.0 with downward rounding.  */
      if (WANT_ROUNDING && y == 0.0)
	y = 0.0;
      /* The underflow exception needs to be signaled explicitly.  */
      math_force_eval (math_opt_barrier (0x1p-1022) * 0x1p-1022);
    }
  y = 0x1p-1022 * y;

  return check_uflow (y);
}

/* Double-precision 10^x approximation. Largest observed error is ~0.513 ULP.  */
double
__exp10 (double x)
{
  uint64_t ix = asuint64 (x);
  uint32_t abstop = (ix >> 52) & 0x7ff;

  if (__glibc_unlikely (abstop - SmallTop >= Thresh))
    {
      if (abstop - SmallTop >= 0x80000000)
	/* Avoid spurious underflow for tiny x.
	   Note: 0 is common input.  */
	return x + 1;
      if (abstop == 0x7ff)
	return ix == asuint64 (-INFINITY) ? 0.0 : x + 1.0;
      if (x >= OFlowBound)
	return __math_oflow (0);
      if (x < UFlowBound)
	return __math_uflow (0);

      /* Large x is special-cased below.  */
      abstop = 0;
    }

  /* Reduce x: z = x * N / log10(2), k = round(z).  */
  double z = __exp_data.invlog10_2N * x;
  double kd;
  uint64_t ki;
#if TOINT_INTRINSICS
  kd = roundtoint (z);
  ki = converttoint (z);
#else
  kd = math_narrow_eval (z + Shift);
  ki = asuint64 (kd);
  kd -= Shift;
#endif

  /* r = x - k * log10(2), r in [-0.5, 0.5].  */
  double r = x;
  r = __exp_data.neglog10_2hiN * kd + r;
  r = __exp_data.neglog10_2loN * kd + r;

  /* exp10(x) = 2^(k/N) * 2^(r/N).
     Approximate the two components separately.  */

  /* s = 2^(k/N), using lookup table.  */
  uint64_t e = ki << (52 - EXP_TABLE_BITS);
  uint64_t i = (ki & IndexMask) * 2;
  uint64_t u = __exp_data.tab[i + 1];
  uint64_t sbits = u + e;

  double tail = asdouble (__exp_data.tab[i]);

  /* 2^(r/N) ~= 1 + r * Poly(r).  */
  double r2 = r * r;
  double p = C (0) + r * C (1);
  double y = C (2) + r * C (3);
  y = y + r2 * C (4);
  y = p + r2 * y;
  y = tail + y * r;

  if (__glibc_unlikely (abstop == 0))
    return special_case (sbits, y, ki);

  /* Assemble components:
     y  = 2^(r/N) * 2^(k/N)
       ~= (y + 1) * s.  */
  double s = asdouble (sbits);
  return s * y + s;
}

strong_alias (__exp10, __ieee754_exp10)
libm_alias_finite (__ieee754_exp10, __exp10)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __exp10, exp10, GLIBC_2_39);
libm_alias_double_other (__exp10, exp10)
#else
libm_alias_double (__exp10, exp10)
#endif
