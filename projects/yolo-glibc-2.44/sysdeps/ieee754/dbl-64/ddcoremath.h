/* Double-double common routines used in correctly rounded implementations.

Copyright (c) 2023-2025 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary64/acosh/acosh.c, revision 887cab6f).

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

#ifndef _DD_COREMATH_H
#define _DD_COREMATH_H

/* References:
   [1] Tight and rigourous error bounds for basic building blocks of
       double-word arithmetic, by Mioara Joldeş, Jean-Michel Muller,
       and Valentina Popescu, ACM Transactions on Mathematical Software,
       44(2), 2017.
   [2] Formalization of double-word arithmetic, and comments on ”Tight and
       rigorous error bounds for basic building blocks of double-word
       arithmetic”, Jean-Michel Muller, Laurence Rideau,
       https://hal.science/hal-02972245v2, 2021.
*/

static inline double
fasttwosum (double x, double y, double *e)
{
  double s = x + y, z = s - x;
  *e = y - z;
  return s;
}

static inline double
fasttwosub (double x, double y, double *e)
{
  double s = x - y, z = x - s;
  *e = z - y;
  return s;
}

static inline double
twosum (double x, double y, double *e)
{
  if (__glibc_likely (fabs (x) > fabs (y)))
    return fasttwosum (x, y, e);
  else
    return fasttwosum (y, x, e);
}

static inline double
fastsum (double xh, double xl, double yh, double yl, double *e)
{
  double sl, sh = fasttwosum (xh, yh, &sl);
  *e = (xl + yl) + sl;
  return sh;
}

static inline double
sumdd (double xh, double xl, double yh, double yl, double *e)
{
  double sl, sh;
  if (__glibc_likely (fabs (xh) > fabs (yh)))
    sh = fasttwosum (xh, yh, &sl);
  else
    sh = fasttwosum (yh, xh, &sl);
  *e = (xl + yl) + sl;
  return sh;
}

static inline double
adddd (double xh, double xl, double ch, double cl, double *l)
{
  double s = xh + ch, d = s - xh;
  *l = ((ch - d) + (xh + (d - s))) + (xl + cl);
  return s;
}

/* This function returns twosum (xh, ch) + (xl + cl).  */
static inline double
adddd2 (double xh, double xl, double ch, double cl, double *l)
{
  double s = twosum (xh, ch, l);
  *l += xl + cl;
  return s;
}

/* This function implements Algorithm 10 (DWTimesDW1) from [1]
   Its relative error (for round-to-nearest ties-to-even) is bounded by 5u^2
   (Theorem 2.6 of [2]), where u = 2^-53 for double precision,
   assuming xh = RN(xh + xl), which implies |xl| <= 1/2 ulp(xh),
   and similarly for ch, cl. */
static inline double
muldd_acc (double xh, double xl, double ch, double cl, double *l)
{
  double ahlh = ch * xl, alhh = cl * xh, ahhh = ch * xh,
	 ahhl = fma (ch, xh, -ahhh);
  ahhl += alhh + ahlh;
  ch = ahhh + ahhl;
  *l = (ahhh - ch) + ahhl;
  return ch;
}

/* This function implements Algorithm 10 (DWTimesDW1) from [1]
   Its relative error (for round-to-nearest ties-to-even) is bounded by 7u^2
   (Theorem 5.1 of [1]), where u = 2^-53 for double precision,
   assuming xh = RN(xh + xl), which implies |xl| <= 1/2 ulp(xh),
   and similarly for ch, cl.
   Note: in revision 085972b, we replaced the last two lines
   ch = ahhh + ahhl and *l = (ahhh - ch) + ahhl by fasttwosum (ahhh, ahhl, l).
   Indeed, these last two lines did emulate a FastTwoSum.
   However, they did emulate another variant of fasttwosum, with
   z = x - s and e = z + y.
   Note that these two variants differ when the fasttwosum condition
   |x| >= |y| is not satisfied.
   Take for example with precision 3 and rounding upwards, x=-7 and
   y=28. Then s = RU(x + y) = 24. With the first variant, z = RU(s-x) = 32
   and e = RU(y - z) = -4, thus s + e = 20. With the second variant,
   z = RU(x-s) = -28 and e = RU(z + y) = 0, thus s + e = 24. In this case,
   the first variant is closer to the sum x + y = 21.
   Still with precision 3 and rounding upwards, now take x=7 and
   y=-28. Then s = RU(x + y) = -20. With the first variant, z = RU(s-x) = -24
   and e = RU(y - z) = -4, thus s + e = -28. With the second variant,
   z = RU(x-s) = 28 and e = RU(z + y) = 0, thus s + e = -20. In this case,
   the second variant is closer to the sum x + y = -21.
*/
static inline double
muldd_acc2 (double xh, double xl, double ch, double cl, double *l)
{
 double ahlh = ch * xl, alhh = cl * xh, ahhh = ch * xh,
	ahhl = fma (ch, xh, -ahhh);
 ahhl += alhh + ahlh;
 return fasttwosum (ahhh, ahhl, l);
}

static inline double
muldd2 (double xh, double xl, double ch, double cl, double *l)
{
  double ahhh = ch * xh;
  *l = (ch * xl + cl * xh) + fma (ch, xh, -ahhh);
  return ahhh;
}

static inline double
muldd3 (double xh, double xl, double yh, double yl, double *l)
{
  double ch = xh * yh, cl1 = fma (xh, yh, -ch);
  double tl0 = xl * yl;
  double tl1 = tl0 + xh * yl;
  double cl2 = tl1 + xl * yh;
  double cl3 = cl1 + cl2;
  return fasttwosum (ch, cl3, l);
}

static inline double
mulddd (double xh, double xl, double ch, double *l)
{
  double ahlh = ch * xl, ahhh = ch * xh, ahhl = fma (ch, xh, -ahhh);
  ahhl += ahlh;
  ch = ahhh + ahhl;
  *l = (ahhh - ch) + ahhl;
  return ch;
}

/* Same as mulddd, except the final normalization uses the fasttwosum
   variant discussed in the muldd_acc2 comment above.  */
static inline double
mulddd_acc (double xh, double xl, double ch, double *l)
{
  double ahlh = ch * xl, ahhh = ch * xh, ahhl = fma (ch, xh, -ahhh);
  ahhl += ahlh;
  return fasttwosum (ahhh, ahhl, l);
}

static inline double
mulddd2 (double x, double ch, double cl, double *l)
{
  double ahhh = ch * x;
  *l = cl * x + fma (ch, x, -ahhh);
  return ahhh;
}

static inline double mulddd3 (double xh, double xl, double ch, double *l)
{
  double hh = xh * ch;
  *l = fma (ch, xh, -hh) + xl * ch;
  return hh;
}

static inline double
polydd (double xh, double xl, int n, const double c[][2], double *l)
{
  int i = n - 1;
  double ch, cl;
  ch = fasttwosum (c[i][0], *l, &cl);
  cl += c[i][1];
  while (--i >= 0)
    {
      ch = muldd_acc (xh, xl, ch, cl, &cl);
      double th = ch + c[i][0], tl = (c[i][0] - th) + ch;
      ch = th;
      cl += tl + c[i][1];
    }
  *l = cl;
  return ch;
}

static inline double
polydd2 (double xh, double xl, int n, const double c[][2], double *l)
{
  int i = n - 1;
  double cl, ch = fasttwosum (c[i][0], *l, &cl);
  cl += c[i][1];
  while (--i >= 0)
    {
      ch = muldd2 (xh, xl, ch, cl, &cl);
      ch = fastsum (c[i][0], c[i][1], ch, cl, &cl);
    }
  *l = cl;
  return ch;
}

static inline double
polydd3 (double xh, double xl, int n, const double c[][2], double *l)
{
  int i = n - 1;
  double ch, cl;
  ch = fasttwosum (c[i][0], *l, &cl);
  cl += c[i][1];
  while (--i >= 0)
    {
      ch = muldd_acc2 (xh, xl, ch, cl, &cl);
      double tl, th = fasttwosum (c[i][0], ch, &tl);
      ch = th;
      cl += tl + c[i][1];
    }
  *l = cl;
  return ch;
}

static inline double
polyddd (double x, int n, const double c[][2], double *l)
{
  int i = n - 1;
  double cl, ch = fasttwosum (c[i][0], *l, &cl);
  cl += c[i][1];
  while (--i >= 0)
    {
      ch = mulddd2 (x, ch, cl, &cl);
      ch = sumdd (c[i][0], c[i][1], ch, cl, &cl);
    }
  *l = cl;
  return ch;
}

static inline double
polydddfst (double x, int n, const double c[][2], double *l)
{
  int i = n - 1;
  double cl, ch = fasttwosum (c[i][0], *l, &cl);
  cl += c[i][1];
  while (--i >= 0)
    {
      ch = mulddd2 (x, ch, cl, &cl);
      ch = fastsum (c[i][0], c[i][1], ch, cl, &cl);
    }
  *l = cl;
  return ch;
}

static inline double
polyd (double x, int n, const double c[][2])
{
  int i = n - 1;
  double ch = c[i][0];
  while (--i >= 0)
    ch = c[i][0] + x * ch;
  return ch;
}

#endif
