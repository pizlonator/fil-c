/* Correctly rounded hyperbolic tangent function for binary64 values.

Copyright (c) 2023-2026 Alexei Sibidanov, Cyprien Peignier, Paul Zimmermann.

Alexei Sibidanov designed the original algorithm, while Cyprien Peignier and
Paul Zimmermann extended the fma formula for |x0| <= 0x1.d12ed0af1a27fp-27,
and improved the minimax polynomial for x0 <= |x| < 0.25.

The original version of this file was copied from the CORE-MATH
project (file src/binary64/tanh/tanh.c, revision cf237fa0).

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
#include <stdint.h>
#include <libm-alias-double.h>
#include <ddcoremath.h>
#include "math_config.h"
#include "e_coshsinh_data.h"
#include "e_tanh_data.h"

static __attribute__((noinline)) double as_tanh_database(double, double);

/* At input, *l is the approximation of the upper part of the polynomial
   (evaluated with double arithmetic only).  */
static inline double
polydd_tanh (double xh, double xl, int n, const double c[][2], double *l)
{
  int i = n - 1;
  double t;
  double ch = fasttwosum (c[i][0], *l, &t);
  double cl = t + c[i][1];
  /* ch + cl ~= c[i][0] + c[i][1] + *l  */
  while (--i >= 0)
    {
      double tl;
      ch = muldd_acc2 (xh, xl, ch, cl, &cl);
      ch = fasttwosum (c[i][0], ch, &tl);
      cl = (cl + c[i][1]) + tl;
    }
  *l = cl;
  return ch;
}

static double __attribute__ ((noinline))
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
  double dx = x - l2h * t, dxl = l2l * t,
	 dxll = l2ll * t + fma (l2l, t, -dxl);
  double dxh = dx + dxl;
  dxl = ((dx - dxh) + dxl) + dxll;
  double fl = dxh
	      * (0x1.5555555555555p-5
		 + dxh * (0x1.11111113e93e9p-7 + dxh * 0x1.6c16c169400a7p-10));
  double fh = polydd_tanh (dxh, dxl, array_length (ch), ch, &fl);
  fh = muldd_acc2 (dxh, dxl, fh, fl, &fl);
  fh = muldd_acc2 (th, tl, fh, fl, &fl);
  double zh = th + fh, zl = (th - zh) + fh;
  double uh = zh + tl, ul = ((zh - uh) + tl) + zl;
  double vh = uh + fl, vl = ((uh - vh) + fl) + ul;
  *l = vl;
  return vh;
}

/* Assumes |x| < 0.25.  */
static double __attribute__ ((noinline))
as_tanh_zero (double x)
{
  double x2 = x * x, x2l = fma (x, x, -x2);
  double y2
      = x2 * (CL[0] + x2 * (CL[1] + x2 * (CL[2] + x2 * (CL[3] + x2 * CL[4]))));
  double y1 = polydd_tanh (x2, x2l, array_length (CH), CH, &y2);
  y1 = mulddd_acc (y1, y2, x, &y2);
  y1 = muldd_acc2 (y1, y2, x2, x2l, &y2);
  double y0 = fasttwosum (x, y1, &y1);
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
      if (__glibc_unlikely (fabs (x) == 0x1.ac343b179fec4p-3))
	return as_tanh_database (x, y0 + y1);
    }
  return y0 + y1;
}

static __attribute__ ((noinline)) double
as_tanh_database (double x, double f)
{
  int a = 0, b = array_length (DB) - 1, m = (a + b) / 2;
  double ax = fabs (x);
  while (a <= b)
    {
      if (DB[m][0] < ax)
	a = m + 1;
      else if (DB[m][0] == ax)
	{
	  f = copysign (1, x) * DB[m][1]
	      + copysign (1, x) * DB[m][2];
	  break;
	}
      else
	b = m - 1;
      m = (a + b) / 2;
    }
  return f;
}

#ifndef SECTION
# define SECTION
#endif

SECTION
double
__tanh (double x)
{
  double ax = fabs (x);
  uint64_t aix = asuint64 (ax);
  /* for |x| >= 0x1.30fc1931f09cap+4, tanh(x) rounds to +1 or -1 to nearest,
     this avoid a spurious overflow in the computation of v0 below */
  if (__glibc_unlikely (aix >= UINT64_C(0x40330fc1931f09ca)))
    {
      if (aix > UINT64_C(0x7ff0000000000000))
	return x + x; /* nan */
      double f = copysign (1.0, x);
      if (aix == UINT64_C(0x7ff0000000000000))
	return f;
      double df = copysign (0x1p-55, x);
      return f - df;
    }
  const double s = -0x1.71547652b82fep+13;
  double v0 = fma (ax, s, 0x1.8000004p+25);
  uint64_t jt = asuint64 (v0);
  uint64_t v = jt;
  uint64_t tt = ~((UINT64_C(1) << 27) - INT64_C(1));
  v &= tt;
  double t = asdouble (v) - 0x1.8p25;
  int64_t i1 = (jt >> 27) & 0x3f, i0 = (jt >> 33) & 0x3f,
	  ie = (int64_t) (jt << 13) >> MANTISSA_WIDTH;
  const double sp = asdouble ((uint64_t) (1023 + ie) << MANTISSA_WIDTH);
  static const double ch[]
      = { 0x1p+1, 0x1p+1, 0x1.55555557e54ffp+0, 0x1.55555553a12f4p-1 };
  double t0h = T0[i0][1], t1h = T1[i1][1], th = t0h * t1h, tl;
  if (aix < UINT64_C(0x400d76c8b4395810))
    { /* |x| ~< 3.683 */
      if (__glibc_unlikely (aix < UINT64_C(0x3fd0000000000000)))
	{ /* |x| < 0x1p-2 */
	  if (__glibc_unlikely (aix <= UINT64_C(0x3e4d12ed0af1a27f)))
	    { /* |x| <= 0x1.d12ed0af1a27fp-27 */
	      if (__glibc_unlikely (!aix))
		return x;
	      /* We have underflow when 0 < |x| < 2^-1022 or when |x| =
		 2^-1022 and rounding towards zero.  */
	      return fma (x, -0x1p-55, x);
	    }
	  static const double c[] =
	    {
	      -0x1.5555555555555p-2,  0x1.1111111110f33p-3,
	      -0x1.ba1ba1b9b8ea6p-5,  0x1.664f4838e0a43p-6,
	      -0x1.226e17d1bc09bp-7,  0x1.d6c64dfba2565p-9,
	      -0x1.7bdd094d327afp-10, 0x1.1535ad0c31d0ep-11
	    };
	  double x2 = x * x, x3 = x2 * x, x4 = x2 * x2, x8 = x4 * x4;
	  double p1 = (c[4] + x2 * c[5]) + x4 * (c[6] + x2 * c[7]);
	  double p0 = (c[0] + x2 * c[1]) + x4 * (c[2] + x2 * c[3]);
	  p0 += x8 * p1;
	  p0 *= x3;
	  double rl, rh = fasttwosum (x, p0, &rl);
	  /* The branch 0x1.d12ed0af1a27fp-27 <= x < 0x1p-26 was checked
	     exhaustively (with and without fma contraction) with revision
	     1820535, with the error bound e = x3*0x1.4dp-52.  It fails with
	     0x1.4cp-52 and x=0x1.27a0e7f47f0fap-4 (rndz, no fma contraction).
	     The interval [0x1p-3, 0x1.00cp-3] was checked exhaustively with
	     rndz and without fma contraction, with error bound
	     e = x3*0x1.80p-52: no failure.  The interval
	     [0x1.015891c9eaef8p-3, 0x1.019891c9eaef8p-3] was checked
	     exhaustively with rndz and without fma contraction, with error
	     bound e = x3*0x1.80p-52: no failure.  */
	  double e = x3 * 0x1.ap-52, lb = rh + (rl - e), ub = rh + (rl + e);
	  if (lb == ub)
	    return lb;
	  return as_tanh_zero (x);
	}

      double t0l = T0[i0][0], t1l = T1[i1][0];
      tl = t0h * t1l + t1h * t0l + fma (t0h, t1h, -th);
      th *= sp;
      tl *= sp;
      const double l2h = -0x1.62e42ffp-14, l2l = -0x1.718432a1b0e26p-48;
      double dx = (l2h * t - ax) - l2l * t, dx2 = dx * dx;
      double p = dx * ((ch[0] + dx * ch[1]) + dx2 * (ch[2] + dx * ch[3]));
      double rh = th, rl = tl + rh * p;
      rh = fasttwosum (rh, rl, &rl);

      double ph = rh, pl = rl;
      double qh = rh, ql = rl, qd;
      qh = fasttwosum (1, qh, &qd);
      ql += qd;

      double rqh = 1 / qh, rql = (ql * rqh + fma (rqh, qh, -1)) * -rqh;
      ph = muldd_acc2 (ph, pl, rqh, rql, &pl);

      /* This branch was tested exhaustively with/without fma contraction.
	 During this search, a failure was found with the original error
	 bound (e = rh*0x1p-62) and x=0x1.a0112a16e9318p+1 (rndu, no fma
	 contraction).  */
      double e = rh * 0x1.0bp-62;
      rh = fasttwosub (0.5, ph, &rl);
      rl -= pl;
      rh *= copysign (2, x);
      rl *= copysign (2, x);
      double lb = rh + (rl - e), ub = rh + (rl + e);
      if (lb == ub)
	return lb;
    }
  else
    { /* 3.683 ~< |x| < 0x1.30fc1931f09cap+4 */
      static const double l2 = -0x1.62e42fefa39efp-14;
      double dx = fma (l2, t, -ax), dx2 = dx * dx;
      double p = dx * ((ch[0] + dx * ch[1]) + dx2 * (ch[2] + dx * ch[3]));
      double rh = th * sp;
      rh += (p + ((2 * 0x1.3p-55) * ax)) * rh;
      /* This branch was tested exhaustively with/without fma contraction.
	 During this search, the largest 9-bit value of e for which it fails
	 was found to be e = rh*0x1.fap-50 with x=0x1.09cc2de69e78cp+2
	 (rndu, with/without fma contraction).  Thus the bound below can be
	 reduced to rh*0x1.fbp-50.  */
      double e = rh * 0x1.1p-49;
      rh = (2 * rh) / (1 + rh);
      double one = copysign (1, x);
      rh = copysign (rh, x);
      double lb = one - (rh + e), ub = one - (rh - e);
      if (lb == ub)
	return lb;

      double t0l = T0[i0][0], t1l = T1[i1][0];
      tl = t0h * t1l + t1h * t0l + fma (t0h, t1h, -th);
      th *= sp;
      tl *= sp;
    }
  double rl, rh = as_exp_accurate (-2 * ax, t, th, tl, &rl);
  double qd, qh = fasttwosum (1, rh, &qd), ql = rl + qd;
  qh = fasttwosum (qh, ql, &ql);
  double rqh = 1 / qh, rql = (ql * rqh + fma (rqh, qh, -1)) * -rqh;
  double pl, ph = muldd_acc2 (rh, rl, rqh, rql, &pl);
  rh = fasttwosub (0.5, ph, &rl);
  rl -= pl;
  rh = fasttwosum (rh, rl, &rl);
  double res = copysign (2, x) * rh + copysign (2, x) * rl;
  uint64_t lu = asuint64 (rl);
  if (__glibc_unlikely (((lu + 32) & MANTISSA_MASK) < 65))
    return as_tanh_database (x, res);
  return res;
}
libm_alias_double (__tanh, tanh)
