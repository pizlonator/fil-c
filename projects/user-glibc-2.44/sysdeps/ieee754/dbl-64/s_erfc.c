/* Correctly-rounded complementary error function for the binary64 format

Copyright (c) 2022-2025 Alexei Sibidanov, Paul Zimmermann, Tom Hubrecht and
Claude-Pierre Jeannerod.

The original version of this file was copied from the CORE-MATH
project (file src/binary64/erfc/erfc.c, revision 55e9869e).

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

/* References:
   [1] The Mathematical Function Computation Handbook, Nelson H.F. Beebe,
       Springer, 2017.
   [2] Handbook of Mathematical Functions, Abramowitz, M., and Stegun, I.,
       Dover, 1973.
   [3] The functions erf and erfc computed with arbitrary precision and
       explicit error bounds, Sylvain Chevillard, Research Report, 2009,
       https://inria.hal.science/ensl-00356709v3.
   [4]
   https://stackoverflow.com/questions/39777360/accurate-computation-of-scaled-complementary-error-function-erfcx
   [5] Chebyshev Approximation of (1 + 2 x) exp(x^2) erfc x in 0 ≤ x < Inf,
       M. M. Shepherd and J. G. Laframboise, Mathematics of Computation,
       Volume 36, No. 153, January 1981, pp. 249-253
   [6] Modern Computer Arithmetic, Richard Brent and Paul Zimmermann,
       Cambridge University Press, 2011.
*/

#include <array_length.h>
#include <math.h>
#include <stdint.h>
#include <errno.h>
#include <libm-alias-double.h>
#include "math_config.h"
#include "s_erf_common.h"
#include "s_erfc_data.h"

/* Assuming 0 <= z <= 0x1.7afb48dc96626p+2, put in h+l an accurate
   approximation of erf(z).
   Assumes z >= 2^-61, thus no underflow can occur. */
__attribute__ ((cold)) static void
cr_erf_accurate (double *h, double *l, double z)
{
  double th, tl;
  if (z < 0.125) /* z < 1/8 */
    return __cr_erf_accurate_tiny (h, l, z, false);
  double v = floor (8.0 * z);
  uint32_t i = 8.0 * z;
  z = (z - 0.0625) - 0.125 * v;
  /* now |z| <= 1/16 */
  const double *p = C2[i - 1];
  *h = p[26]; /* degree-18 */
  for (int j = 17; j > 10; j--)
    *h = fma (*h, z, p[8 + j]); /* degree j */
  *l = 0;
  for (int j = 10; j > 7; j--)
    {
      /* multiply h+l by z */
      a_mul (&th, &tl, *h, z);
      tl = fma (*l, z, tl);
      /* add p[8+j] to th + tl */
      two_sum (h, l, p[8 + j], th);
      *l += tl;
    }
  for (int j = 7; j >= 0; j--)
    {
      /* multiply h+l by z */
      a_mul (&th, &tl, *h, z);
      tl = fma (*l, z, tl);
      /* add p[2*j] + p[2*j+1] to th + tl: we use two_sum() instead of
	 fast_two_sum because for example for i=3, the coefficient of
	 degree 7 is tiny (0x1.060b78c935b8ep-13) with respect to that
	 of degree 8 (0x1.678b51a9c4b0ap-7) */
      two_sum (h, l, p[2 * j], th);
      *l += p[2 * j + 1] + tl;
    }
}

// Multiply a double with a double double : a * (bh + bl)
static inline void
s_mul (double *hi, double *lo, double a, double bh, double bl)
{
  a_mul (hi, lo, a, bh); /* exact */
  *lo = fma (a, bl, *lo);
}

// Returns (ah + al) * (bh + bl) - (al * bl)
static inline void
d_mul (double *hi, double *lo, double ah, double al, double bh, double bl)
{
  a_mul (hi, lo, ah, bh);
  *lo = fma (ah, bl, *lo);
  *lo = fma (al, bh, *lo);
}

// Add a + (bh + bl), assuming |a| >= |bh|
static inline void
fast_sum (double *hi, double *lo, double a, double bh, double bl)
{
  fast_two_sum (hi, lo, a, bh);
  /* |(a+bh)-(hi+lo)| <= 2^-105 |hi| and |lo| < ulp(hi) */
  *lo += bl;
  /* |(a+bh+bl)-(hi+lo)| <= 2^-105 |hi| + ulp(lo),
     where |lo| <= ulp(hi) + |bl|. */
}

// Approximation for the fast path of exp(z) for z=zh+zl,
// with |z| < 0.000130273 < 2^-12.88 and |zl| < 2^-42.6
// (assuming x^y does not overflow or underflow)
static inline void
q_1 (double *hi, double *lo, double zh, double zl)
{
  double z = zh + zl;
  double q = fma (Q_1[4], zh, Q_1[3]);

  q = fma (q, z, Q_1[2]);

  fast_two_sum (hi, lo, Q_1[1], q * z);

  d_mul (hi, lo, zh, zl, *hi, *lo);

  fast_sum (hi, lo, Q_1[0], *hi, *lo);
}

/*
  Approximation of exp(x), where x = xh + xl

  exp(x) is approximated by hi + lo.

  For the error analysis, we only consider the case where x^y does not
  overflow or underflow. We get:

  (hi + lo) / exp(xh + xl) = 1 + eps with |eps| < 2^-74.139

  Assumes |xl/xh| < 2^-23.89 and |xl| < 2^-14.3486.

  At output, we also have 0.99985 < hi+lo < 1.99995 and |lo/hi| < 2^-41.4.
*/

static inline void
exp_1 (double *hi, double *lo, double xh, double xl)
{

#define INVLOG2 0x1.71547652b82fep+12 /* |INVLOG2-2^12/log(2)| < 2^-43.4 */
  double k = roundeven_finite (xh * INVLOG2);

  double kh, kl;
#define LOG2H 0x1.62e42fefa39efp-13
#define LOG2L 0x1.abc9e3b39803fp-68
  s_mul (&kh, &kl, k, LOG2H, LOG2L);

  double yh, yl;
  fast_two_sum (&yh, &yl, xh - kh, xl);
  yl -= kl;

  int64_t K = k; /* Note: k is an integer, this is just a conversion. */
  int64_t M = (K >> 12) + 0x3ff;
  int64_t i2 = (K >> 6) & 0x3f;
  int64_t i1 = K & 0x3f;

  double t1h = T1[i2][0], t1l = T1[i2][1], t2h = T2[i1][0], t2l = T2[i1][1];
  d_mul (hi, lo, t2h, t2l, t1h, t1l);

  double qh, ql;
  q_1 (&qh, &ql, yh, yl);

  d_mul (hi, lo, *hi, *lo, qh, ql);

  double df = asdouble (M << 52);
  *hi *= df;
  *lo *= df;
}

/* put in 2^e*(h+l) an approximation of exp(xh+xl) for -742 <= xh+xl <= -2.92,
   with target accuracy 104 bits */
static void
exp_accurate (double *h, double *l, int *e, double xh, double xl)
{
  double th, tl, yh, yl;
  /* first reduce argument: xh + xl ~ k*log(2) + yh + yl */
#define INVLOG2acc 0x1.71547652b82fep+0 // approximates 1/log(2)
  int k = roundeven_finite (xh * INVLOG2acc);
  /* since |xh| <= 742, |k| <= round(742/log(2)) = 1070 */
  /* subtract k*log(2), where LOG2H+LOG2L approximates log(2) */
#define LOG2Hacc 0x1.62e42fefa39efp-1
  /* we approximate LOG2Lacc ~ log(2) - LOG2H with 38 bits, so that
     k*LOG2Lacc is exact (k has at most 11 bits) */
#define LOG2Lacc 0x1.abc9e3b398p-56
#define LOG2tiny 0x1.f97b57a079a19p-103
  yh = fma (-k, LOG2Hacc, xh);
  /* since |xh+xl| >= 2.92 we have |k| >= 4;
  (|k|-1/2)*log(2) <= |x| <= (|k|+1/2)*log(2) thus
  1-1/(2|k|) <= |x/(k*log(2))| <= 1+1/(2|k|) thus by Sterbenz theorem
  yh is exact too */
  two_sum (&th, &tl, -(double) k * LOG2Lacc, xl);
  fast_two_sum (&yh, &yl, yh, th);
  yl = fma (-k, LOG2tiny, yl + tl);
  /* now yh+yl approximates xh + xl - k*log(2), and we approximate p(yh+yl)
     in h + l */
  /* Since |xh| <= 742, we assume |xl| <= ulp(742) = 2^-43. Then since
     |k| <= round(742/log(2)) = 1070, |yl| <= 1070*LOG2L + 2^-42 < 2^-42.7.
     Since |yh| <= log(2)/2, the contribution of yl is negligible as long
     as |i*p[i]*yh^(i-1)*yl| < 2^-104, which holds for i >= 16.
     Thus for coefficients of degree 16 or more, we don't take yl into account.
  */
  *h = E2[19 + 8]; // degree 19
  for (int i = 18; i >= 16; i--)
    *h = fma (*h, yh, E2[i + 8]); // degree i
  /* degree 15: h*(yh+yl)+E2[15 + 8] */
  a_mul (&th, &tl, *h, yh);
  tl = fma (*h, yl, tl);
  fast_two_sum (h, l, E2[15 + 8], th);
  *l += tl;
  for (int i = 14; i >= 8; i--)
    {
      /* degree i: (h+l)*(yh+yl)+E2[i+8] */
      a_mul (&th, &tl, *h, yh);
      tl = fma (*h, yl, tl);
      tl = fma (*l, yh, tl);
      fast_two_sum (h, l, E2[i + 8], th);
      *l += tl;
    }
  for (int i = 7; i >= 0; i--)
    {
      /* degree i: (h+l)*(yh+yl)+E2[2i]+E2[2i+1] */
      a_mul (&th, &tl, *h, yh);
      tl = fma (*h, yl, tl);
      tl = fma (*l, yh, tl);
      fast_two_sum (h, l, E2[2 * i], th);
      *l += tl + E2[2 * i + 1];
    }
  /* we have to multiply h,l by 2^k */
  *e = k;
}

/* Fast path for 0x1.713786d9c7c09p+1 < x < 0x1.b39dc41e48bfdp+4,
   using the asymptotic formula erfc(x) = exp(-x^2) * p(1/x), where
   p(x) is computed with Sollya (files asympt[0-5].sollya).
   Return a bound on the absolute error. */
static double
erfc_asympt_fast (double *h, double *l, double x)
{
  /* for x >= 0x1.9db1bb14e15cap+4, erfc(x) < 2^-970, and we might encounter
     underflow issues in the computation of l, thus we delegate this case
     to the accurate path */
  if (x >= 0x1.9db1bb14e15cap+4)
    {
      *h = 0;
      *l = 0;
      return 1.0;
    }

  /* first approximate exp(-x^2): */
  double eh, el, uh, ul;
  a_mul (&uh, &ul, x, x);
  exp_1 (&eh, &el, -uh, -ul);
  /* the assumptions from exp_1 are satisfied:
     * a_mul ensures |ul| <= ulp(uh), thus |ul/uh| <= 2^-52
     * since |x| < 0x1.9db1bb14e15cap+4 we have
       |ul| < ulp(0x1.9db1bb14e15cap+4^2) = 2^-43 */
  /* eh+el approximates exp(-x^2) with maximal relative error 2^-74.139 */

  /* compute 1/x as double-double */
  double yh, yl;
  yh = 1.0 / x;
  /* Assume 1 <= x < 2, then 0.5 <= yh <= 1,
     and yh = 1/x + eps with |eps| <= 2^-53. */
  /* Newton's iteration for 1/x is y -> y + y*(1-x*y) */
  yl = yh * fma (-x, yh, 1.0);
  /* x*yh-1 = x*(1/x+eps)-1 = x*eps
     with |x*eps| <= 2^-52, thus the error on the FMA is bounded by
     ulp(2^-52.1) = 2^-105.
     Now |yl| <= |yh| * 2^-52 <= 2^-52, thus the rounding error on
     yh * fma (-x, yh, 1.0) is bounded also by ulp(2^-52.1) = 2^-105.
     From [6], Lemma 3.7, if yl was computed exactly, then yh+yl would differ
     from 1/x by at most yh^2/theta^3*(1/x-yh)^2 for some theta in [yh,1/x]
     or [1/x,yh].
     Since yh, 1/x <= 1, this gives eps^2 <= 2^-106.
     Adding the rounding errors, we have:
     |yh + yl - 1/x| <= 2^-105 + 2^-105 + 2^-106 < 2^-103.67.
     For the relative error, since |yh| >= 1/2, this gives:
     |yh + yl - 1/x| < 2^-102.67 * |yh+yl|
  */

  /* look for the right interval for yh */
  static const double threshold[]
      = { 0x1.d5p-4, 0x1.59da6ca291ba6p-3, 0x1.bcp-3, 0x1.0cp-2, 0x1.38p-2,
	  0x1.63p-2 };
  int i;
  for (i = 0; yh > threshold[i]; i++)
    ;

  const double *p = T[i];
  a_mul (&uh, &ul, yh, yh); // exact
  /* Since |yh| <= 1, we have |uh| <= 1 and |ul| <= 2^-53. */
  ul = fma (2.0 * yh, yl, ul);
  /* uh+ul approximates (yh+yl)^2, with absolute error bounded by
     ulp(ul) + yl^2, where ulp(ul) is the maximal rounding error in
     the FMA, and yl^2 is the neglected term.
     Since |ul| <= 2^-53, ulp(ul) <= 2^-105, and since |yl| <= 2^-52,
     this yields |uh + ul - yh^2| <= 2^-105 + 2^-104 < 2^-103.41.
     For the relative error, since |(yh+yl)^2| >= 1/4:
     |uh + ul - yh^2| < 2^-101.41 * |uh+ul|.
     And relatively to 1/x^2:
     yh + yl = 1/x * (1 + eps1)       with |eps1| < 2^-102.67
     uh + ul = (yh+yl)^2 * (1 + eps2) with |eps2| < 2^-101.41
     This yields:
     |uh + ul - 1/x| < 2^-100.90 * |uh+ul|.
  */

  /* evaluate p(uh+ul) */
  double zh, zl;
  zh = p[12];		    // degree 23
  zh = fma (zh, uh, p[11]); // degree 21
  zh = fma (zh, uh, p[10]); // degree 19
  /* degree 17: zh*(uh+ul)+p[i] */
  s_mul (h, l, zh, uh, ul);
  fast_two_sum (&zh, &zl, p[9], *h);
  zl += *l;

  for (int j = 15; j >= 3; j -= 2)
    {
      d_mul (h, l, zh, zl, uh, ul);
      fast_two_sum (&zh, &zl, p[(j + 1) / 2], *h);
      zl += *l;
    }
  /* degree 1: (zh+zl)*(uh+ul)+p[0]+p[1] */
  d_mul (h, l, zh, zl, uh, ul);
  fast_two_sum (&zh, &zl, p[0], *h);
  zl += *l + p[1];
  /* multiply by yh+yl */
  d_mul (&uh, &ul, zh, zl, yh, yl);
  /* now uh+ul approximates p(1/x) */
  /* now multiply (uh+ul)*(eh+el) */
  d_mul (h, l, uh, ul, eh, el);
  /* Write y = 1/x.  We have the following errors:
     * the maximal mathematical error is:
       |erfc(x)*exp(x^2) - p(y)| < 2^-71.804 * |p(y)| (for i=3) thus
       |erfc(x) - exp(-x^2)*p(y)| < 2^-71.804 * |exp(-x^2)*p(y)|
     * the error in approximating exp(-x^2) by eh+el:
       |eh + el - exp(-x^2)| < 2^-74.139 * |eh + el|
     * the fact that we evaluate p on yh+yl instead of 1/x
       this error is bounded by |p'| * |yh+yl - 1/x|, where
       |yh+yl - 1/x| < 2^-102.67 * |yh+yl|, and the relative
       error is bounded by |p'/p| * |yh+yl - 1/x|.
       Since the maximal value of |p'/p| is bounded by 27.2 (for i=0),
       this yields 27.2 * 2^-102.67 < 2^-97.9
     * the rounding errors when evaluating p on yh+yl: this error is bounded
       (relatively) by 2^-67.184 (for i=5), see analyze_erfc_asympt_fast()
       in erfc.sage
     * the rounding error in (uh+ul)*(eh+el): we assume this error is bounded
       by 2^-80 (relatively)
     This yields a global relative bound of:
     (1+2^-71.804)*(1+2^-74.139)*(1+2^-97.9)*(1+2^-67.184)*(1+2^-80)-1
     < 2^-67.115
  */
  // avoid a spurious underflow in 0x1.d9p-68 * h
  if (*h >= 0x1.151b9a3fdd5c9p-955)
    return 0x1.d9p-68 * *h; /* 2^-67.115 < 0x1.d9p-68 */
  else
    return 0x1p-1022; // this overestimates 0x1.d9p-68 * h
}

/* given -0x1.7744f8f74e94bp2 < x < 0x1.b39dc41e48bfdp+4,
   put in h+l a double-double approximation of erfc(x),
   with *absolute* error bounded by err (the returned value) */
static double
cr_erfc_fast (double *h, double *l, double x)
{
  /* on a i7-8700 with gcc 12.2.0, for x in [-5,0], the average reciprocal
     throughput is about 44 cycles */
  if (x < 0) // erfc(x) = 1 - erf(x) = 1 + erf(-x)
    {
      double err = __cr_erf_fast (h, l, -x);
      /* h+l approximates erf(-x), with relative error bounded by err,
	 where err <= 0x1.78p-69 */
      err = err * *h; /* convert into absolute error */
      double t;
      fast_two_sum (h, &t, 1.0, *h);
      // since h <= 2, the fast_two_sum() error is bounded by 2^-105*h <=
      // 2^-104
      *l = t + *l;
      /* After the fast_two_sum() call, we have |t| <= ulp(h) <= ulp(2) = 2^-51
	 thus assuming |l| <= 2^-51 after the cr_erf_fast() call,
	 we have |t| <= 2^-50 here, thus the rounding
	 error on t -= *l is bounded by ulp(2^-50) = 2^-102.
	 The absolute error is thus bounded by err + 2^-104 + 2^-102
	 = err + 0x1.4p-102.
	 The maximal value of err here is for |x| < 0.0625, where cr_erf_fast()
	 returns 0x1.78p-69, and h=1/2, yielding err = 0x1.78p-70 here.
	 Adding 0x1.4p-102 is thus exact. */
      return err + 0x1.4p-102;
    }
    // now 0 <= x < 0x1.b39dc41e48bfdp+4
#define THRESHOLD1 0x1.713786d9c7c09p+1
  /* on a i7-8700 with gcc 12.2.0, for x in [0,THRESHOLD1],
     the average reciprocal throughput is about 59 cycles */
  else if (x <= THRESHOLD1)
    {
      double err = __cr_erf_fast (h, l, x);
      /* h+l approximates erf(x), with relative error bounded by err,
	 where err <= 0x1.78p-69 */
      err = err * *h; /* convert into absolute error */
      double t;
      fast_two_sum (h, &t, 1.0, -*h);
      *l = t - *l;
      /* for x >= 0x1.e861fbb24c00ap-2, erf(x) >= 1/2, thus 1-h is exact
	 by Sterbenz theorem, thus t = 0 in fast_two_sum(), and we have t = -l
	 here, thus the absolute error is err */
      if (x >= 0x1.e861fbb24c00ap-2)
	return err;
      /* for x < 0x1.e861fbb24c00ap-2, the error in fast_two_sum() is bounded
	 by 2^-105*h, and since h <= 1/2, this yields 2^-106.
	 After the fast_two_sum() call, we have |t| <= ulp(h) <= ulp(1/2) =
	 2^-53 thus assuming |l| <= 2^-53 after the cr_erf_fast() call, we have
	 |t| <= 2^-52 here, thus the rounding error on t -= *l is bounded by
	 ulp(2^-52) = 2^-104. The absolute error is thus bounded by err +
	 2^-106 + 2^-104 = err + 0x1.4p-104. The maximal value of err here is
	 for x < 0.0625, where cr_erf_fast() returns 0x1.78p-69, and h=1/2,
	 yielding err = 0x1.78p-70 here. Adding 0x1.4p-104 is thus exact. */
      return err + 0x1.4p-104;
    }
  /* Now THRESHOLD1 < x < 0x1.b39dc41e48bfdp+4 thus erfc(x) < 0.000046. */
  /* on a i7-8700 with gcc 12.2.0, for x in [THRESHOLD1,+5.0],
     the average reciprocal throughput is about 111 cycles
     (among which 20 cycles for exp_1) */
  return erfc_asympt_fast (h, l, x);
}

/* assume 0x1.b59ffb450828cp+0 < x < 0x1.b39dc41e48bfdp+4
   thus 1.70 < x < 27.3 */
static double
erfc_asympt_accurate (double x)
{
  for (int i = 0; i < array_length (EXCEPTIONS_ERFC); i++)
    if (x == EXCEPTIONS_ERFC[i][0])
      return EXCEPTIONS_ERFC[i][1] + EXCEPTIONS_ERFC[i][2];

  /* subnormal exceptions */
  if (x == 0x1.a8f7bfbd15495p+4)
    return fma (0x1p-1074, -0.25, 0x1.99ef5883f656cp-1024);

  double h, l;
  /* first approximate exp(-x^2) */
  double eh, el, uh, ul;
  a_mul (&uh, &ul, x, x);
  int e;
  exp_accurate (&eh, &el, &e, -uh, -ul);
  /* eh+el approximates exp(-x^2), where 2.92 < x^2 < 742 */

  /* compute 1/x as double-double */
  double yh, yl;
  yh = 1.0 / x;
  /* Newton's iteration for 1/x is y -> y + y*(1-x*y) */
  yl = yh * fma (-x, yh, 1.0);
  // yh+yl approximates 1/x
  static const double threshold[]
      = { 0x1.45p-4, 0x1.e0p-4, 0x1.3fp-3, 0x1.95p-3, 0x1.f5p-3,
	  0x1.31p-2, 0x1.71p-2, 0x1.bcp-2, 0x1.0bp-1, 0x1.3p-1 };
  int i;
  for (i = 0; yh > threshold[i]; i++)
    ;
  // 0 <= i <= 9
  const double *p = Tacc[i];
  /* now evaluate p(yh + yl) */
  a_mul (&uh, &ul, yh, yh);
  ul = fma (2.0 * yh, yl, ul);
  /* uh+ul approximates 1/x^2 */
  double zh, zl;
  /* the polynomial p has degree 29+2i, and its coefficient of largest
     degree is p[14+6+i] */
  zh = p[14 + 6 + i];
  zl = 0;
  for (int j = 27 + 2 * i; j >= 13; j -= 2)
    {
      /* degree j: (zh+zl)*(uh+ul)+p[(j-1)/2+6]] */
      a_mul (&h, &l, zh, uh);
      l = fma (zh, ul, l);
      l = fma (zl, uh, l);
      two_sum (&zh, &zl, p[(j - 1) / 2 + 6], h);
      zl += l;
    }
  for (int j = 11; j >= 1; j -= 2)
    /* degree j: (zh+zl)*(uh+ul)+p[j-1]+p[j] */
    {
      a_mul (&h, &l, zh, uh);
      l = fma (zh, ul, l);
      l = fma (zl, uh, l);
      two_sum (&zh, &zl, p[j - 1], h);
      zl += l + p[j];
    }
  /* multiply by yh+yl */
  a_mul (&uh, &ul, zh, yh);
  ul = fma (zh, yl, ul);
  ul = fma (zl, yh, ul);
  /* now uh+ul approximates p(1/x), i.e., erfc(x)*exp(x^2) */
  /* now multiply (uh+ul)*(eh+el), after normalizing uh+ul to reduce the
     number of exceptional cases */
  fast_two_sum (&uh, &ul, uh, ul);
  a_mul (&h, &l, uh, eh);
  l = fma (uh, el, l);
  l = fma (ul, eh, l);
  /* multiply by 2^e */
  double res = __ldexp (h + l, e);
  if (res < 0x1p-1022)
    {
      /* for erfc(x) in the subnormal range, we have to perform a special
	 rounding */
      double corr = h - __ldexp (res, -e);
      corr += l;
      /* add corr*2^e */
      res += __ldexp (corr, e);
    }
  return res;
}

static double
cr_erfc_accurate (double x)
{
  double h, l, t;
  if (x < 0)
    {
      for (int i = 0; i < array_length (EXCEPTIONS_ACCURATE_ERFC); i++)
	if (x == EXCEPTIONS_ACCURATE_ERFC[i][0])
	  return EXCEPTIONS_ACCURATE_ERFC[i][1] + EXCEPTIONS_ACCURATE_ERFC[i][2];
      cr_erf_accurate (&h, &l, -x);
      fast_two_sum (&h, &t, 1.0, h);
      l = t + l;
      return h + l;
    }
  else if (x <= 0x1.b59ffb450828cp+0) // erfc(x) >= 2^-6
    {
      for (int i = 0; i < array_length (EXCEPTIONS_ACCURATE_ERFC_2); i++)
	if (x == EXCEPTIONS_ACCURATE_ERFC_2[i][0])
	  return EXCEPTIONS_ACCURATE_ERFC_2[i][1] + EXCEPTIONS_ACCURATE_ERFC_2[i][2];
      cr_erf_accurate (&h, &l, x);
      fast_two_sum (&h, &t, 1.0, -h);
      l = t - l;
      return h + l;
    }
  // now 0x1.b59ffb450828cp+0 < x < 0x1.b39dc41e48bfdp+4
  return erfc_asympt_accurate (x);
}

double
__erfc (double x)
{
  uint64_t t = asuint64 (x);
  uint64_t at = t & UINT64_C(0x7fffffffffffffff);

  if (t >= UINT64_C(0x8000000000000000)) // x = -NaN or x <= 0 (excluding +0)
    {
      // for x <= -0x1.7744f8f74e94bp2, erfc(x) rounds to 2 (to nearest)
      // x = NaN or x <= -0x1.7744f8f74e94bp2
      if (t >= UINT64_C(0xc017744f8f74e94b))
	{
	  if (t >= UINT64_C(0xfff0000000000000))
	    { // -Inf or NaN
	      if (t == UINT64_C(0xfff0000000000000))
		return 2.0; // -Inf
	      return x + x; // NaN
	    }
	  return 2.0 - 0x1p-54; // rounds to 2 or below(2)
	}

      // for -0x1.c5bf891b4ef6ap-54 <= x <= 0, erfc(x) rounds to 1 (to nearest)
      if (-0x1.c5bf891b4ef6ap-54 <= x)
	return fma (-x, 0x1p-54, 1.0);
    }

  else // x = +NaN or x >= 0 (excluding -0)
    {
      // for x >= 0x1.b39dc41e48bfdp+4, erfc(x) < 2^-1075: rounds to 0 or
      // 2^-1074
      // x = NaN or x >= 0x1.b39dc41e48bfdp+4
      if (at >= UINT64_C(0x403b39dc41e48bfd))
	{
	  if (at >= UINT64_C(0x7ff0000000000000))
	    { // +Inf or NaN
	      if (at == UINT64_C(0x7ff0000000000000))
		return 0.0; // +Inf
	      return x + x; // NaN
	    }
	  return __math_erange (0x1p-1074 * 0.25); // 0 or 2^-1074 wrt rounding
	}

      // for 0 <= x <= 0x1.c5bf891b4ef6ap-55, erfc(x) rounds to 1 (to nearest)
      if (x <= 0x1.c5bf891b4ef6ap-55)
	return fma (-x, 0x1p-54, 1.0);
    }

  /* now -0x1.7744f8f74e94bp+2 < x < -0x1.c5bf891b4ef6ap-54
     or 0x1.c5bf891b4ef6ap-55 < x < 0x1.b39dc41e48bfdp+4 */
  double h, l, err;
  err = cr_erfc_fast (&h, &l, x);

  double left = h + (l - err);
  double right = h + (l + err);
  if (left == right)
    return left;

  return cr_erfc_accurate (x);
}
libm_alias_double (__erfc, erfc)
