/* Correctly rounded hyperbolic cosine for binary64 values.

Copyright (c) 2023-2026 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary64/cosh/cosh.c, revision 5d0c89d5).

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
SOFTWARE.
*/

#include <array_length.h>
#include <math.h>
#include <libm-alias-finite.h>
#include <libm-alias-double.h>
#include <math-svid-compat.h>
#include <ddcoremath.h>
#include "e_cosh_data.h"
#include "e_coshsinh_data.h"
#include "math_config.h"

#ifndef SECTION
#  define SECTION
#endif

static __attribute__ ((noinline)) double as_cosh_database (double, double);

static inline double
polydd_cosh (double xh, double xl, int n, const double c[][2], double *l)
{
  int i = n - 1;
  double ch, cl, e;
  ch = fasttwosum (c[i][0], *l, &cl);
  cl += c[i][1];
  while (--i >= 0)
    {
      ch = muldd2 (xh, xl, ch, cl, &cl);
      ch = fasttwosum (c[i][0], ch, &e);
      cl = (cl + c[i][1]) + e;
    }
  *l = cl;
  return ch;
}

static double __attribute__ ((cold, noinline))
as_exp_accurate (double x, double t, double th, double tl, double *l)
{
  static const double ch[][2] =
    {
      { 0x1p+0, 0x1.6c16bd194535dp-94 },
      { 0x1p-1, -0x1.8259d904fd34fp-93 },
      { 0x1.5555555555555p-3, 0x1.53e93e9f26e62p-57 }
    };
  const double l2h = 0x1.62e42ffp-13, l2l = 0x1.718432a1b0e26p-47,
	       l2ll = 0x1.9ff0342542fc3p-102;
  double dx = x - l2h * t, dxl = l2l * t, dxll = l2ll * t + fma (l2l, t, -dxl);
  double dxh = dx + dxl;
  dxl = ((dx - dxh) + dxl) + dxll;
  double fl = dxh
	      * (0x1.5555555555555p-5
		 + dxh * (0x1.11111113e93e9p-7 + dxh * 0x1.6c16c169400a7p-10));
  double fh = polydd_cosh (dxh, dxl, 3, ch, &fl);
  fh = muldd2 (dxh, dxl, fh, fl, &fl);
  fh = muldd2 (th, tl, fh, fl, &fl);
  double zh = th + fh, zl = (th - zh) + fh;
  double uh = zh + tl, ul = ((zh - uh) + tl) + zl;
  double vh = uh + fl, vl = ((uh - vh) + fl) + ul;
  *l = vl;
  return vh;
}

static double __attribute__ ((noinline))
as_cosh_zero (double x)
{
  double x2 = x * x, x2l = fma (x, x, -x2);
  double y2 = x2 * (CL[0] + x2 * (CL[1] + x2 * (CL[2] + x2 * (CL[3]))));
  double y1 = polydd (x2, x2l, 4, CH, &y2);
  y1 = muldd2 (y1, y2, x2, x2l, &y2);
  double y0 = fasttwosum (1.0, y1, &y1);
  y1 = fasttwosum (y1, y2, &y2);
  uint64_t t = asuint64 (y1);
  if (__glibc_unlikely (!(t & MANTISSA_MASK)))
    {
      uint64_t w = asuint64 (y2);
      if ((w ^ t) >> 63)
	t--;
      else
	t++;
      y1 = asdouble (t);
    }
  if (__glibc_unlikely ((t & MANTISSA_MASK) == MANTISSA_MASK))
    return as_cosh_database (x, y0 + y1);
  return y0 + y1;
}

static __attribute__ ((noinline)) double
as_cosh_database (double x, double f)
{
  int a = 0, b = array_length (DB) - 1, m = (a + b) / 2;
  double ax = fabs (x);
  while (a <= b)
    {
      if (DB[m][0] < ax)
	a = m + 1;
      else if (DB[m][0] == ax)
	{
	  f = DB[m][1] + DB[m][2];
	  break;
	}
      else
	b = m - 1;
      m = (a + b) / 2;
    }
  return f;
}

SECTION
double
__cosh (double x)
{
  const double s = 0x1.71547652b82fep+12;
  double ax = fabs (x), v0 = fma (ax, s, 0x1.8000002p+26);
  uint64_t jt = asuint64 (v0);
  uint64_t v = asuint64 (v0);
  uint64_t tt = ~((1 << 26) - 1l);
  v &= tt;
  double t = asdouble (v) - 0x1.8p26;
  uint64_t aix = asuint64 (ax);
  if (__glibc_unlikely (aix < UINT64_C(0x3fc0000000000000)))
    { // |x| < 0.125
      if (__glibc_unlikely (aix
			    < UINT64_C(0x3e50000000000000))) // |x| < 0x1p-26
	return fma (ax, 0x1p-55, 1);
      /* q(x) = 1 + c0*x^2 + c1*x^4 + c2*x^6 + c3*x^8 + c4*x^10 is a degree-10
	 polynomial approximating cosh(x) on [2^-26, 0.125] such that:
	 |q(x) - cosh(x)| < 2^-67.518 * x^2.
	 This polynomial was generated with the following Sollya command:
	 d = [2^-26,0.125];
	 q=1+x^2*fpminimax((cosh(x)-1)/x^2, [|0,2,4,6,8|], [|53...|], d,
	 absolute);
      */
      static const double c[] =
	{
	  0x1p-1, 0x1.5555555555554p-5, 0x1.6c16c16c1d0cp-10,
	  0x1.a01a0075066b4p-16, 0x1.27faff8dcc1c8p-22
	};
      double x2 = x * x, x4 = x2 * x2,
	     p
	     = x2
	       * ((c[0] + x2 * c[1]) + x4 * ((c[2] + x2 * c[3]) + x4 * c[4]));
      // fails with e = x2*(0x1.c8p-52), x=0x1.0f0a7d6ea89ep-14 (rndu, no FMA)
      double e = x2 * 0x1.84p-51, lb = 1 + (p - e), ub = 1 + (p + e);
      if (lb == ub)
	return lb;
      return as_cosh_zero (x);
    }

  // treat large values apart to avoid a spurious invalid exception
  if (__glibc_unlikely (aix > UINT64_C(0x408633ce8fb9f87d)))
    {
      // |x| > 0x1.633ce8fb9f87dp+9
      if (aix > UINT64_C(0x7ff0000000000000))
	return x + x; // nan
      if (aix == UINT64_C(0x7ff0000000000000))
	return fabs (x); // inf
      return __math_oflow (0);
    }

  // now 0.125 <= |x| <= 0x1.633ce8fb9f87dp+9
  /* exhaustive tests:
     0.25 <= x < 2: done
     2 <= x < 4: nancy (gr10)
     4 <= x < 8: nancy
     8 <= x < 16: explor
  */
  int64_t il = ((uint64_t) jt << 14) >> 40, jl = -il;
  int64_t i1 = il & 0x3f, i0 = (il >> 6) & 0x3f, ie = il >> 12;
  int64_t j1 = jl & 0x3f, j0 = (jl >> 6) & 0x3f, je = jl >> 12;
  double sp = asdouble ((uint64_t) (1022 + ie) << 52),
	 sm = asdouble ((uint64_t) (1022 + je) << 52);
  double t0h = T0[i0][1], t0l = T0[i0][0];
  double t1h = T1[i1][1], t1l = T1[i1][0];
  double th = t0h * t1h, tl = t0h * t1l + t1h * t0l + fma (t0h, t1h, -th);
  const double l2h = 0x1.62e42ffp-13, l2l = 0x1.718432a1b0e26p-47;
  double dx = (ax - l2h * t) + l2l * t, dx2 = dx * dx, mx = -dx;
  static const double ch[]
      = { 0x1p+0, 0x1p-1, 0x1.5555555aaaaaep-3, 0x1.55555551c98cp-5 };
  double pp = dx * ((ch[0] + dx * ch[1]) + dx2 * (ch[2] + dx * ch[3]));
  double rh, rl;
  if (__glibc_unlikely (aix > UINT64_C(0x4014000000000000)))
    { // |x| > 5
      if (__glibc_unlikely (aix > UINT64_C(0x40425e4f7b2737fa)))
	{ // |x| >~ 36.736801
	  sp = asdouble ((1021 + ie) << 52);
	  rh = th;
	  rl = tl + th * pp;
	  double e = 0x1.1b578c96db19bp-63 * th, lb = rh + (rl - e),
		 ub = rh + (rl + e);
	  if (lb == ub)
	    return (lb * sp) * 2;

	  th = as_exp_accurate (ax, t, th, tl, &tl);
	  th = fasttwosum (th, tl, &tl);
	  uint64_t uh = asuint64 (th), ul = asuint64 (tl);
	  int64_t eh = (uh >> MANTISSA_WIDTH) & 0x7ff,
		  el = (ul >> MANTISSA_WIDTH) & 0x7ff,
		  ml = (ul + 8) & MANTISSA_MASK;
	  th += tl;
	  th *= 2;
	  th *= sp;
	  /* if the exponent difference between eh and el is larger than 103,
	     or if the last bits from ml are <= 8 in absolute value,
	     call the accurate path */
	  if (ml <= 16 || eh - el > 103)
	    return as_cosh_database (x, th);
	  return th;
	}
      double q0h = T0[j0][1], q1h = T1[j1][1], qh = q0h * q1h;
      th *= sp;
      tl *= sp;
      qh *= sm;
      double pm = mx * ((ch[0] + mx * ch[1]) + dx2 * (ch[2] + mx * ch[3]));
      double em = qh + qh * pm;
      rh = th;
      rl = (tl + em) + th * pp;

      double e = 0x1.202p-63 * rh, lb = rh + (rl - e), ub = rh + (rl + e);
      // fails with e = 0x1.afbp-64*rh and x=0x1.4173941572a71p+2 (rndz)
      if (lb == ub)
	return lb;

      th = as_exp_accurate (ax, t, th, tl, &tl);
      if (__glibc_unlikely (aix > UINT64_C(0x403f666666666666)))
	{
	  // |x| > 0x1.f666666666666p+4
	  rh = th + qh;
	  rl = ((th - rh) + qh) + tl;
	}
      else
	{
	  qh = q0h * q1h;
	  double q0l = T0[j0][0], q1l = T1[j1][0];
	  double ql = q0h * q1l + q1h * q0l + fma (q0h, q1h, -qh);
	  qh *= sm;
	  ql *= sm;
	  qh = as_exp_accurate (-ax, -t, qh, ql, &ql);
	  rh = th + qh;
	  rl = (((th - rh) + qh) + ql) + tl;
	}
    }
  else
    { // |x| <= 5
      double q0h = T0[j0][1], q0l = T0[j0][0];
      double q1h = T1[j1][1], q1l = T1[j1][0];
      double qh = q0h * q1h, ql = q0h * q1l + q1h * q0l + fma (q0h, q1h, -qh);
      th *= sp;
      tl *= sp;
      qh *= sm;
      ql *= sm;
      double pm = mx * ((ch[0] + mx * ch[1]) + dx2 * (ch[2] + mx * ch[3]));
      double fph = th, fpl = tl + th * pp;
      double fmh = qh, fml = ql + qh * pm;

      rh = fph + fmh;
      rl = ((fph - rh) + fmh) + fml + fpl;
      double e = 0x1.c0ap-62 * rh, lb = rh + (rl - e), ub = rh + (rl + e);
      // fails with e = 0x1.855p-64*rh and x=0x1.dbf464fbc8795p+0 (rndz, no
      // fma)
      if (lb == ub)
	return lb;
      th = as_exp_accurate (ax, t, th, tl, &tl);
      qh = as_exp_accurate (-ax, -t, qh, ql, &ql);
      rh = th + qh;
      rl = ((th - rh) + qh) + ql + tl;
    }
  rh = fasttwosum (rh, rl, &rl);
  uint64_t uh = asuint64 (rh), ul = asuint64 (rl);
  int64_t eh = (uh >> MANTISSA_WIDTH) & 0x7ff,
	  el = (ul >> MANTISSA_WIDTH) & 0x7ff,
	  ml = (ul + 8) & MANTISSA_MASK;
  rh += rl;
  if (__glibc_unlikely (ml <= 16 || eh - el > 103))
    return as_cosh_database (x, rh);
  return rh;
}
#ifndef __cosh
strong_alias (__cosh, __ieee754_cosh)
# if LIBM_SVID_COMPAT
versioned_symbol (libm, __cosh, cosh, GLIBC_2_44);
libm_alias_double_other (__cosh, cosh)
# else
libm_alias_double (__cosh, cosh)
# endif
libm_alias_finite (__ieee754_cosh, __cosh)
#endif
