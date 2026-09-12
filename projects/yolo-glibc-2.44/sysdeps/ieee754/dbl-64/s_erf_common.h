/* Common definitions for erf/erc implementation.

Copyright (c) 2023-2025 Paul Zimmermann

This file is part of the CORE-MATH project
(https://core-math.gitlabpages.inria.fr/).

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

#ifndef _S_ERF_COMMON_H
#define _S_ERF_COMMON_H

#include "s_erf_data.h"

/* Add a + b, such that *hi + *lo approximates a + b.
   Assumes |a| >= |b|.
   For rounding to nearest we have hi + lo = a + b exactly.
   For directed rounding, we have
   (a) hi + lo = a + b exactly when the exponent difference between a and b
       is at most 53 (the binary64 precision)
   (b) otherwise |(a+b)-(hi+lo)| <= 2^-105 min(|a+b|,|hi|)
       (see https://hal.inria.fr/hal-03798376)
   We also have |lo| < ulp(hi). */
static inline void
fast_two_sum (double *hi, double *lo, double a, double b)
{
  double e;

  *hi = a + b;
  e = *hi - a; /* exact */
  *lo = b - e; /* exact */
}

/* Reference: https://hal.science/hal-01351529v3/document */
static inline void
two_sum (double *hi, double *lo, double a, double b)
{
  *hi = a + b;
  double aa = *hi - b;
  double bb = *hi - aa;
  double da = a - aa;
  double db = b - bb;
  *lo = da + db;
}

// Multiply exactly a and b, such that *hi + *lo = a * b.
static inline void
a_mul (double *hi, double *lo, double a, double b)
{
  *hi = a * b;
  *lo = fma (a, b, -*hi);
}

/* Assuming 0 <= z <= 0x1.7afb48dc96626p+2, put in h+l an approximation
   of erf(z). Return err the maximal relative error:
   |(h + l)/erf(z) - 1| < err*|h+l| */
double __cr_erf_fast (double *h, double *l, double z) attribute_hidden;

/* for |z| < 1/8, assuming z >= 2^-61, thus no underflow can occur */
void __cr_erf_accurate_tiny (double *h, double *l, double z, bool exceptions)
     attribute_hidden;

#endif
