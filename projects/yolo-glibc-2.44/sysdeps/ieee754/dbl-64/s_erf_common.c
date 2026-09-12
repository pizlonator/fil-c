/* Common definitions for erf/erc implementation.

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

#include <stdint.h>
#include <math.h>
#include <stddef.h>
#include <stdbool.h>
#include "s_erf_common.h"

double
__cr_erf_fast (double *h, double *l, double z)
{
  double th, tl;
  /* we split [0,0x1.7afb48dc96626p+2] into intervals i/16 <= z < (i+1)/16,
     and for each interval, we use a minimax polynomial:
     * for i=0 (0 <= z < 1/16) we use a polynomial evaluated at zero,
       since if we evaluate in the middle 1/32, we will get bad accuracy
       for tiny z, and moreover z-1/32 might not be exact
     * for 1 <= i <= 94, we use a polynomial evaluated in the middle of
       the interval, namely i/16+1/32
  */
  if (z < 0.0625) /* z < 1/16 */
    {
      double z2h, z2l, z4;
      a_mul (&z2h, &z2l, z, z);
      z4 = z2h * z2h;
      double c9 = fma (C0[7], z2h, C0[6]);
      double c5 = fma (C0[5], z2h, C0[4]);
      c5 = fma (c9, z4, c5);
      /* compute C0[2] + C0[3] + z2h*c5 */
      a_mul (&th, &tl, z2h, c5);
      fast_two_sum (h, l, C0[2], th);
      *l += tl + C0[3];
      /* compute C0[0] + C0[1] + (z2h + z2l)*(h + l) */
      double h_copy = *h;
      a_mul (&th, &tl, z2h, *h);
      tl += fma (z2h, *l, C0[1]);
      fast_two_sum (h, l, C0[0], th);
      *l += fma (z2l, h_copy, tl);
      /* multiply (h,l) by z */
      a_mul (h, &tl, *h, z);
      *l = fma (*l, z, tl);
      return 0x1.78p-69; /* err < 2.48658249618372e-21, cf Analyze0() */
    }
  double v = floor (16.0 * z);
  uint32_t i = 16.0 * z;
  /* i/16 <= z < (i+1)/16 */
  /* For 0.0625 0 <= z <= 0x1.7afb48dc96626p+2, z - 0.03125 is exact:
     (1) either z - 0.03125 is in the same binade as z, then 0.03125 is
	 an integer multiple of ulp(z), so is z - 0.03125
     (2) if z - 0.03125 is in a smaller binade, both z and 0.03125 are
	 integer multiple of the ulp() of that smaller binade.
     Also, subtracting 0.0625 * v is exact. */
  z = (z - 0.03125) - 0.0625 * v;
  /* now |z| <= 1/32 */
  const double *c = C[i - 1];
  double z2 = z * z, z4 = z2 * z2;
  /* the degree-10 coefficient is c[12] */
  double c9 = fma (c[12], z, c[11]);
  double c7 = fma (c[10], z, c[9]);
  double c5 = fma (c[8], z, c[7]);
  double c3h, c3l;
  /* c3h, c3l <- c[5] + z*c[6] */
  fast_two_sum (&c3h, &c3l, c[5], z * c[6]);
  c7 = fma (c9, z2, c7);
  /* c3h, c3l <- c3h, c3l + c5*z2 */
  fast_two_sum (&c3h, &tl, c3h, c5 * z2);
  c3l += tl;
  /* c3h, c3l <- c3h, c3l + c7*z4 */
  fast_two_sum (&c3h, &tl, c3h, c7 * z4);
  c3l += tl;
  /* c2h, c2l <- c[4] + z*(c3h + c3l) */
  double c2h, c2l;
  a_mul (&th, &tl, z, c3h);
  fast_two_sum (&c2h, &c2l, c[4], th);
  c2l += fma (z, c3l, tl);
  /* compute c[2] + c[3] + z*(c2h + c2l) */
  a_mul (&th, &tl, z, c2h);
  fast_two_sum (h, l, c[2], th);
  *l += tl + fma (z, c2l, c[3]);
  /* compute c[0] + c[1] + z*(h + l) */
  a_mul (&th, &tl, z, *h);
  tl = fma (z, *l, tl); /* tl += z*l */
  fast_two_sum (h, l, c[0], th);
  *l += tl + c[1];
  return 0x1.11p-69; /* err < 1.80414390200020e-21, cf analyze_p(1)
			(larger values of i yield smaller error bounds) */
}

void
__cr_erf_accurate_tiny (double *h, double *l, double z, bool exceptions)
{
  if (exceptions)
    {
      uint64_t i, j, k;
      /* use dichotomy */
      for (i = 0, j = EXCEPTIONS_TINY_LEN; i + 1 < j;)
	{
	  k = (i + j) / 2;
	  if (EXCEPTIONS_TINY[k][0] <= z)
	    i = k;
	  else
	    j = k;
	}
      /* Either i=0 and z < exceptions[i+1][0],
	 or i=n-1 and exceptions[i][0] <= z
	 or 0 < i < n-1 and exceptions[i][0] <= z < exceptions[i+1][0].
	 In all cases z can only equal exceptions[i][0]. */
      if (z == EXCEPTIONS_TINY[i][0])
	{
	  *h = EXCEPTIONS_TINY[i][1];
	  *l = EXCEPTIONS_TINY[i][2];
	  return;
	}
    }

  double z2 = z * z, th, tl;
  *h = P[21 / 2 + 4]; /* degree 21 */
  for (int a = 19; a > 11; a -= 2)
    *h = fma (*h, z2, P[a / 2 + 4]); /* degree j */
  *l = 0;
  for (int a = 11; a > 7; a -= 2)
    {
      /* multiply h+l by z^2 */
      a_mul (&th, &tl, *h, z);
      tl = fma (*l, z, tl);
      a_mul (h, l, th, z);
      *l = fma (tl, z, *l);
      /* add p[j] to h + l */
      fast_two_sum (h, &tl, P[a / 2 + 4], *h);
      *l += tl;
    }
  for (int a = 7; a >= 1; a -= 2)
    {
      /* multiply h+l by z^2 */
      a_mul (&th, &tl, *h, z);
      tl = fma (*l, z, tl);
      a_mul (h, l, th, z);
      *l = fma (tl, z, *l);
      fast_two_sum (h, &tl, P[a - 1], *h);
      *l += P[a] + tl;
    }
  /* multiply by z */
  a_mul (h, &tl, *h, z);
  *l = fma (*l, z, tl);
  return;
}
