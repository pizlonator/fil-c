/* Correctly-rounded tangent of binary32 value.

Copyright (c) 2022-2024 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/tan/tanf.c, revision 59d21d7).

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
#include <stdint.h>
#include <libm-alias-float.h>
#include "math_config.h"
#include <math_uint128.h>

/* argument reduction
   for |z| < 2^28, return r such that 2/pi*x = q + r  */
static inline double
rltl (float z, int *q)
{
  double x = z;
  double idl = -0x1.b1bbead603d8bp-32 * x;
  double idh = 0x1.45f306ep-1 * x;
  double id = roundeven_finite (idh);
  *q = (int64_t) id;
  return (idh - id) + idl;
}

/* argument reduction
   same as rltl, but for |x| >= 2^28  */
static double __attribute__ ((noinline))
rbig (uint32_t u, int *q)
{
  static const uint64_t ipi[] =
    {
      0xfe5163abdebbc562, 0xdb6295993c439041,
      0xfc2757d1f534ddc0, 0xa2f9836e4e441529
    };
  int e = (u >> 23) & 0xff, i;
  uint64_t m = (u & (~0u >> 9)) | 1 << 23;
  u128 p0 = u128_mul (u128_from_u64 (m), u128_from_u64 (ipi[0]));
  u128 p1 = u128_mul (u128_from_u64 (m), u128_from_u64 (ipi[1]));
  p1 = u128_add (p1, u128_rshift (p0, 64));
  u128 p2 = u128_mul (u128_from_u64 (m), u128_from_u64 (ipi[2]));
  p2 = u128_add (p2, u128_rshift (p1, 64));
  u128 p3 = u128_mul (u128_from_u64 (m), u128_from_u64 (ipi[3]));
  p3 = u128_add (p3, u128_rshift (p2, 64));
  uint64_t p3h = u128_high (p3);
  uint64_t p3l = u128_low (p3);
  uint64_t p2l = u128_low (p2);
  uint64_t p1l = u128_low (p1);
  int64_t a;
  int k = e - 127, s = k - 23;
  /* in ctanf(), rbig() is called in the case 127+28 <= e < 0xff
     thus 155 <= e <= 254, which yields 28 <= k <= 127 and 5 <= s <= 104 */
  if (s < 64)
    {
      i = p3h << s | p3l >> (64 - s);
      a = p3l << s | p2l >> (64 - s);
    }
  else if (s == 64)
    {
      i = p3l;
      a = p2l;
    }
  else
    { /* s > 64 */
      i = p3l << (s - 64) | p2l >> (128 - s);
      a = p2l << (s - 64) | p1l >> (128 - s);
    }
  int sgn = u;
  sgn >>= 31;
  int64_t sm = a >> 63;
  i -= sm;
  double z = (a ^ sgn) * 0x1p-64;
  i = (i ^ sgn) - sgn;
  *q = i;
  return z;
}

float
__tanf (float x)
{
  uint32_t t = asuint (x);
  int e = (t >> 23) & 0xff;
  int i;
  double z;
  if (__glibc_likely (e < 127 + 28))  /* |x| < 2^28 */
    {
      if (__glibc_unlikely (e < 115))
	{
	  if (__glibc_unlikely (e < 102))
	    return fmaf (x, fabsf (x), x);
	  float x2 = x * x;
	  return fmaf (x, 0x1.555556p-2f * x2, x);
	}
      z = rltl (x, &i);
    }
  else if (e < 0xff)
    z = rbig (t, &i);
  else
    {
      if (t << 9)
	return x + x; /* nan */
      return __math_invalidf (x);
    }
  double z2 = z * z;
  double z4 = z2 * z2;
  static const double cn[] =
    {
      0x1.921fb54442d18p+0, -0x1.fd226e573289fp-2,
      0x1.b7a60c8dac9f6p-6, -0x1.725beb40f33e5p-13
    };
  static const double cd[] =
    {
      0x1p+0,               -0x1.2395347fb829dp+0,
      0x1.2313660f29c36p-3, -0x1.9a707ab98d1c1p-9
    };
  static const double s[] = { 0, 1 };
  double n = cn[0] + z2 * cn[1];
  double n2 = cn[2] + z2 * cn[3];
  n += z4 * n2;
  double d = cd[0] + z2 * cd[1];
  double d2 = cd[2] + z2 * cd[3];
  d += z4 * d2;
  n *= z;
  double s0 = s[i & 1];
  double s1 = s[1 - (i & 1)];
  double r1 = (n * s1 - d * s0) / (n * s0 + d * s1);
  uint64_t tail = (asuint64 (r1) + 7) & (~UINT64_C(0) >> 35);
  if (__glibc_unlikely (tail <= 14))
    {
      static const struct
      {
	float arg;
	float rh;
        float rl;
      } st[] = {
	{ 0x1.143ec4p+0f,    0x1.ddf9f6p+0f, -0x1.891d24p-52f },
	{ 0x1.ada6aap+27f,   0x1.e80304p-3f,  0x1.419f46p-58f },
	{ 0x1.af61dap+48f,   0x1.60d1c8p-2f, -0x1.2d6c3ap-55f },
	{ 0x1.0088bcp+52f,   0x1.ca1edp+0f,   0x1.f6053p-53f },
	{ 0x1.f90dfcp+72f,   0x1.597f9cp-1f,  0x1.925978p-53f },
	{ 0x1.cc4e22p+85f,  -0x1.f33584p+1f,  0x1.d7254ap-51f },
	{ 0x1.a6ce12p+86f,  -0x1.c5612ep-1f, -0x1.26c33ep-53f },
	{ 0x1.6a0b76p+102f, -0x1.e42a1ep+0f, -0x1.1dc906p-52f },
      };
      uint32_t ax = t & (~0u >> 1);
      uint32_t sgn = t >> 31;
      for (int j = 0; j < array_length (st); j++)
	{
	  if (__glibc_unlikely (asuint (st[j].arg) == ax))
	    {
	      if (sgn)
		return -st[j].rh - st[j].rl;
	      else
		return st[j].rh + st[j].rl;
	    }
	}
    }
  return r1;
}
libm_alias_float (__tan, tan)
