/* Correctly-rounded inverse hyperbolic tangent function.  Binary64 version.

Copyright (c) 2023-2026 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary64/atanh/atanh.c, revision 532e37dc).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

#include <array_length.h>
#include <stdint.h>
#include <math.h>
#include <libm-alias-finite.h>
#include "math_config.h"
#include "s_atanh_data.h"
#include "ddcoremath.h"

static double __attribute__ ((noinline)) as_atanh_refine (double, double,
							  double, double);
static double __attribute__ ((noinline))
as_atanh_zero (double x)
{
  double x2 = x * x, x2l = fma (x, x, -x2);
  double y2
      = x2
	* (CL[0] + x2 * (CL[1] + x2 * (CL[2] + x2 * (CL[3] + x2 * (CL[4])))));
  double y1 = polydd3 (x2, x2l, 13, CH, &y2);
  y1 = mulddd3 (y1, y2, x, &y2);
  y1 = muldd_acc2 (x2, x2l, y1, y2, &y2);
  double y0 = fasttwosum (x, y1, &y1);
  y1 = fasttwosum (y1, y2, &y2);
  /* We have 22 failures (only for RNDN, 11 up to sign) if we disable this
     check, all with 53 or 54 identical bits after the round bit.
     For example x=0x1.cc7092205cfafp-10 we have y0=0x1.cc70b12857108p-10
     and y1=-0x1p-63.  */
  uint64_t t = asuint64 (y1);
  if (__glibc_unlikely (!(t & (~UINT64_C(0) >> 12))))
    {
      uint64_t w = asuint64 (y2);
      if ((w ^ t) >> 63)
	t--;
      else
	t++;
      y1 = asdouble (t);
    }
  return y0 + y1;
}

#ifndef SECTION
#  define SECTION
#endif


SECTION
double
__ieee754_atanh (double x)
{
  double ax = fabs (x);
  uint64_t aix = asuint64 (ax);
  if (__glibc_unlikely (aix >= UINT64_C(0x3ff0000000000000)))
    { // |x| >= 1
      if (aix == UINT64_C(0x3ff0000000000000))
	// |x| = 1
	return __math_divzero (x < 0.0);
      if (aix > UINT64_C(0x7ff0000000000000))
	return x + x; // nan
      return __math_invalid (x);
    }

  if (__glibc_unlikely (aix < UINT64_C(0x3fd0000000000000)))
    { // |x| < 0x1p-2
      // atanh(x) rounds to x to nearest for |x| < 0x1.d12ed0af1a27fp-27
      if (__glibc_unlikely (aix < UINT64_C(0x3e4d12ed0af1a27f)))
	{
	  // |x| < 0x1.d12ed0af1a27fp-27
	  /* We have underflow exactly when 0 < |x| < 2^-1022:
	     for RNDU, atanh(2^-1022-2^-1074) would round to 2^-1022-2^-1075
	     with unbounded exponent range */
	  return __math_check_uflow_zero_lt (x, 0x1p-1022,
					     fma (x, 0x1p-55, x));
	}
      /* checked exhautively this branch (with and without FMA):
       * for 0x1.d12ed0af1a27fp-27 <= x < 2^-24
       */
      double x2 = x * x;
      static const double c[] = { 0x1.999999999999ap-3, 0x1.2492492492244p-3,
				  0x1.c71c71c79715fp-4, 0x1.745d16f777723p-4,
				  0x1.3b13ca4174634p-4, 0x1.110c9724989bdp-4,
				  0x1.e2d17608a5b2ep-5, 0x1.a0b56308cba0bp-5,
				  0x1.fb6341208ad2ep-5 };
      double dx2 = fma (x, x, -x2);
      double x4 = x2 * x2, x3 = x2 * x, x8 = x4 * x4;
      double dx3 = fma (x2, x, -x3) + dx2 * x;
      double p
	  = (c[0] + x2 * c[1]) + x4 * (c[2] + x2 * c[3])
	    + x8 * ((c[4] + x2 * c[5]) + x4 * (c[6] + x2 * c[7]) + x8 * c[8]);
      double t = fma (x2, p, 0x1.5555555555555p-56);
      double pl, ph = fasttwosum (0x1.5555555555555p-2, t, &pl);
      ph = muldd2 (ph, pl, x3, dx3, &pl);
      double tl;
      ph = fasttwosum (x, ph, &tl);
      pl += tl;
      double eps = x * (x4 * 0x1.dp-53 + 0x1p-103);
      double lb = ph + (pl - eps), ub = ph + (pl + eps);
      if (__glibc_likely (lb == ub))
	return lb;
      return as_atanh_zero (x);
    }

  double pl, ph = fasttwosum (1, ax, &pl), ql, qh = fasttwosub (1, ax, &ql),
	     iqh = 1 / qh, th = ph * iqh,
	     tl = fma (ph, iqh, -th)
		  + (pl + ph * (fma (-qh, iqh, 1) - ql * iqh)) * iqh;

  static const double c[]
      = { -0x1p+0, 0x1.555555555553p+0, -0x1.fffffffffffap+0,
	  0x1.99999e33a6366p+1, -0x1.555559ef9525fp+2 };

  uint64_t t = asuint64 (th);
  int ex = t >> 52, e = ex - 0x3ff;
  t &= ~UINT64_C(0) >> 12;
  double ed = e;
  uint64_t i = t >> (52 - 5);
  int64_t d = t & (~UINT64_C(0) >> 17);
  uint64_t j = (t + ((uint64_t) B[i].c0 << 33)
		+ ((int64_t) B[i].c1 * (d >> 16))) >> (52 - 10);
  t |= UINT64_C(0x3ff) << 52;
  int i1 = j >> 5, i2 = j & 0x1f;
  double tf = asdouble (t);
  double r = (0.5 * R1[i1]) * R2[i2], dx = fma (r, tf, -0.5),
	 dx2 = dx * dx, rx = r * tf, dxl = fma (r, tf, -rx);
  double f
      = dx2 * ((c[0] + dx * c[1]) + dx2 * (c[2] + dx * c[3] + dx2 * c[4]));
  const double l2h = 0x1.62e42fefa3ap-2, l2l = -0x1.0ca86c3898dp-50;
  double lh = (L1[i1][1] + L2[i2][1]) + l2h * ed, ll;
  lh = fasttwosum (lh, rx - 0.5, &ll);
  ll += l2l * ed + (L1[i1][0] + L2[i2][0]) + dxl + 0.5 * tl / th;
  ll += f;
  lh *= copysign (1, x);
  ll *= copysign (1, x);
  double eps = 38e-24 + dx2 * 0x1p-49;
  double lb = lh + (ll - eps), ub = lh + (ll + eps);
  if (__glibc_likely (lb == ub))
    return lb;
  th = fasttwosum (th, tl, &tl);
  return as_atanh_refine (x, th, tl,
			  0x1.71547652b82fep+1 * fabs (lh + ll));
}
#ifndef __ieee754_atanh
libm_alias_finite (__ieee754_atanh, __atanh)
#endif

static __attribute__ ((noinline)) double
as_atanh_database (double x, double f)
{
  static const double db[][3] = {
    { 0x1.2dbb7b1c91363p-2, 0x1.36f33d51c264dp-2, 0x1p-56 },
    { 0x1.c493dc899e4a5p-2, 0x1.e611aa58ab608p-2, -0x1p-56 },
  };
  double ax = fabs (x), sgn = copysign (1, x);
  int a = 0, b = array_length (db) - 1, m = (a + b) / 2;
  while (a <= b)
    { // binary search
      if (db[m][0] < ax)
	a = m + 1;
      else if (db[m][0] == ax)
	{
	  f = sgn * db[m][1] + sgn * db[m][2];
	  break;
	}
      else
	b = m - 1;
      m = (a + b) / 2;
    }
  return f;
}

static double
as_atanh_refine (double x, double zh, double zl, double a)
{
  static const double cl[3]
      = { -0x1p-3, 0x1.9999999a0754fp-4, -0x1.55555555c3157p-4 };
  uint64_t t = asuint64 (zh);
  int ex = t >> 52, e = ex - 0x3ff;
  t &= ~UINT64_C (0) >> 12;
  t |= UINT64_C (0x3ff) << 52;
  double ed = e;
  uint64_t v = asuint64 (a - ed + 0x1.00008p+0);
  uint64_t i = (v - (UINT64_C(0x3ff) << 52)) >> (52 - 16);
  int i1 = i >> 12, i2 = (i >> 8) & 0xf, i3 = (i >> 4) & 0xf, i4 = i & 0xf;
  const double l20 = 0x1.62e42fefa3ap-2, l21 = -0x1.0ca86c3898dp-50,
	       l22 = 0x1.f97b57a079ap-104;
  double el2 = l22 * ed, el1 = l21 * ed, el0 = l20 * ed;
  double L[3];
  L[0] = LL[0][i1][0] + LL[1][i2][0] + (LL[2][i3][0] + LL[3][i4][0]) + el0;
  L[1] = LL[0][i1][1] + LL[1][i2][1] + (LL[2][i3][1] + LL[3][i4][1]);
  L[2] = LL[0][i1][2] + LL[1][i2][2] + (LL[2][i3][2] + LL[3][i4][2]);
  double t12 = T1[i1] * T2[i2], t34 = T3[i3] * T4[i4];
  double th = t12 * t34, tl = fma (t12, t34, -th);
  double tf = asdouble (t);
  double dh = th * tf, dl = fma (th, tf, -dh);
  double sh = tl * tf, sl = fma (tl, tf, -sh);
  double xl, xh = fasttwosum (dh - 1, dl, &xl);
  t = asuint64 (zl);
  t -= (int64_t) e << 52;
  xl += th * asdouble (t);
  xh = adddd (xh, xl, sh, sl, &xl);
  sl = xh * (cl[0] + xh * (cl[1] + xh * cl[2]));
  sh = polydd (xh, xl, 3, CH2, &sl);
  sh = muldd_acc (xh, xl, sh, sl, &sl);
  sh = adddd (sh, sl, el1, el2, &sl);
  sh = adddd (sh, sl, L[1], L[2], &sl);
  double v2, v0 = fasttwosum (L[0], sh, &v2), v1 = fasttwosum (v2, sl, &v2);
  t = asuint64 (v1);
  if (__glibc_unlikely (!(t & (~UINT64_C(0) >> 12))))
    {
      uint64_t w = asuint64 (v2);
      if ((w ^ t) >> 63)
	t--;
      else
	t++;
      v1 = asdouble (t);
    }
  uint64_t t0 = asuint64 (v0);
  uint64_t er = ((t + 1) & (~UINT64_C(0) >> 12)),
	   de = ((t0 >> 52) & 0x7ff) - ((t >> 52) & 0x7ff);
  v0 *= copysign (1, x);
  v1 *= copysign (1, x);
  double res = v0 + v1;
  if (__glibc_unlikely (de > 104 || er < 3))
    return as_atanh_database (x, res);
  return res;
}
