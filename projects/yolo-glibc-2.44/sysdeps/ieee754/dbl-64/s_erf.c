/* Correctly-rounded error function for binary64 value.

Copyright (c) 2023-2025 Paul Zimmermann

The original version of this file was copied from the CORE-MATH
project (file src/binary64/erf/erf.c, revision 384ed01d).

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
#include <errno.h>
#include <libm-alias-double.h>
#include "math_config.h"
#include "s_erf_common.h"

/* CH+CL is a double-double approximation of 2/sqrt(pi) to nearest */
static const double CH = 0x1.20dd750429b6dp+0;
static const double CL = 0x1.1ae3a914fed8p-56;

/* Assuming 0 <= z <= 0x1.7afb48dc96626p+2, put in h+l an accurate
   approximation of erf(z).
   Assumes z >= 2^-61, thus no underflow can occur. */
static void
cr_erf_accurate (double *h, double *l, double z)
{
  for (int i = 0; i < EXCEPTIONS_LEN; i++)
    if (z == EXCEPTIONS[i][0])
      {
	*h = EXCEPTIONS[i][1];
	*l = EXCEPTIONS[i][2];
	return;
      }
  double th, tl;
  /* we split [0,0x1.7afb48dc96626p+2] into intervals i/8 <= z < (i+1)/8,
     and for each interval, we use a minimax polynomial:
     * for i=0 (0 <= z < 1/8) we use a polynomial evaluated at zero,
       since if we evaluate in the middle 1/16, we will get bad accuracy
       for tiny z, and moreover z-1/16 might not be exact
     * for 1 <= i <= 47, we use a polynomial evaluated in the middle of
       the interval, namely i/8+1/16
  */
  if (z < 0.125) /* z < 1/8 */
    return __cr_erf_accurate_tiny (h, l, z, true);
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

double
__erf (double x)
{
  double z = fabs (x);
  uint64_t t = asuint64 (z);
  uint64_t ux = t;
  /* erf(x) rounds to +/-1 for RNDN for |x| > 0x1.7afb48dc96626p+2 */
  // |x| > 0x1.7afb48dc96626p+2
  if (__glibc_unlikely (ux > UINT64_C(0x4017afb48dc96626)))
    {
      double os = copysign (1.0, x);
#define MASK UINT64_C(0x7ff0000000000000) // encoding of +Inf
      if (ux > MASK)
	return x + x; /* NaN */
      if (ux == MASK)
	return os; /* +/-Inf */
      return os - 0x1p-54 * os;
    }
  double h, l, err;
  /* now |x| <= 0x1.7afb48dc96626p+2 */
  if (__glibc_unlikely (z < 0x1p-61))
    {
      /* for x=-0 the code below returns +0 which is wrong */
      if (x == 0)
	return x;
      /* tiny x: erf(x) ~ 2/sqrt(pi) * x + O(x^3), where the ratio of the
	 O(x^3) term to the main term is in x^2/3, thus less than 2^-123 */
      double y = CH * x; /* tentative result */
      /* scale x by 2^106 to get out the subnormal range */
      double sx = x * 0x1p106;
      a_mul (&h, &l, CH, sx);
      l = fma (CL, sx, l);
      /* now compute the residual h + l - y */
      l += h
	   - y * 0x1p106; /* h-y*2^106 is exact since h and y are very close */
      double res = fma (l, 0x1p-106, y);
      return res;
    }
  /* now z >= 2^-61 */
  err = __cr_erf_fast (&h, &l, z);
  uint64_t u = asuint64 (h), v = asuint64 (l);
  t = asuint64 (x);
  u ^= t & SIGN_MASK;
  v ^= t & SIGN_MASK;
  double uf = asdouble (u);
  double vf = asdouble (v);
  double left = uf + fma (err, -uf, vf);
  double right = uf + fma (err, uf, vf);
  if (__glibc_likely (left == right))
    return left;

  cr_erf_accurate (&h, &l, z);

  return (x >= 0) ? h + l : (-h) + (-l);
}
libm_alias_double (__erf, erf)
