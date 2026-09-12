/* Implementation of the gamma function for binary32.

Copyright (c) 2023-2026 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/tgamma/tgammaf.c, revision 8ea8ea35).

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

/* Changes with respect to the original CORE-MATH code:
   - usage of math_narrow_eval to deal with underflow/overflow
 */

#include <array_length.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stddef.h>
#include <libm-alias-float.h>
#include <math-svid-compat.h>
#include <libm-alias-finite.h>
#include <math-narrow-eval.h>
#include "math_config.h"

float
__tgammaf (float x)
{
  /* List of exceptional cases. Each entry contains the 32-bit encoding u of x,
     a binary32 approximation f of gamma(x), and a correction term df.  */
  static const struct
  {
    uint32_t u;
    float f, df;
  } tb[] = {
    { 0x27de86a9u,  0x1.268266p+47f,  0x1p22f },  /* x = 0x1.bd0d52p-48 */
    { 0x27e05475u,  0x1.242422p+47f,  0x1p22f },  /* x = 0x1.c0a8eap-48 */
    { 0xb63befb3u, -0x1.5cb6e4p+18f,  0x1p-7f },  /* x = -0x1.77df66p-19 */
    { 0x3c7bb570u,  0x1.021d9p+6f,    0x1p-19f }, /* x = 0x1.f76aep-7 */
    { 0x41e886d1u,  0x1.33136ap+98f,  0x1p73f },  /* x = 0x1.d10da2p+4 */
    { 0xc067d177u,  0x1.f6850cp-3f,   0x1p-28f }, /* x = -0x1.cfa2eep+1 */
    { 0xbd99da31u, -0x1.befe66p+3,   -0x1p-22f }, /* x = -0x1.33b462p-4 */
    { 0xbf54c45au, -0x1.a6b4ecp+2,    0x1p-23f }, /* x = -0x1.a988b4p-1 */
    { 0x41ee77feu,  0x1.d3631cp+101, -0x1p-76f }, /* x = 0x1.dceffcp+4 */
    { 0x3f843a64u,  0x1.f6c638p-1,    0x1p-26f }, /* x = 0x1.0874c8p+0 */
  };

  uint32_t t = asuint (x);
  uint32_t ax = t << 1;
  if (__glibc_unlikely (ax >= (0xffu << 24)))
    { /* x=NaN or +/-Inf */
      if (ax == (0xffu << 24))
	{ /* x=+/-Inf */
	  if (t >> 31) /* x=-Inf */
	    return __math_invalidf (x);
	  return x; /* x=+Inf */
	}
      return x + x; /* x=NaN, where x+x ensures the "Invalid operation"
		       exception is set if x is sNaN */
    }
  double z = x;
  if (__glibc_unlikely (ax < 0x6d000000u))
    { /* |x| < 0x1p-18 */
      volatile double d = (0x1.fa658c23b1578p-1 - 0x1.d0a118f324b63p-1 * z)
	* z - 0x1.2788cfc6fb619p-1;
      double f = 1.0 / z + d;
      float r = f;
      uint64_t rt = asuint64 (f);
      if (((rt + 2) & 0xfffffff) < 4)
	{
	  for (unsigned i = 0; i < array_length (tb); i++)
	    if (t == tb[i].u)
	      return tb[i].f + tb[i].df;
	}
      if (isinf (r))
	return __math_erangef (r);
      return r;
    }
  float fx = floorf (x);
  if (__glibc_unlikely (x >= 0x1.18522p+5f))
    return __math_oflowf (0);
  /* compute k only after the overflow check, otherwise the case to integer
     might overflow */
  int k;
  if (__glibc_unlikely (x <= -0x1p+31f))
    k = INT32_MIN;
  else
    k = fx;

  if (__glibc_unlikely (fx == x))
    { /* x is integer */
      if (x < 0.0f)
	return __math_invalidf (0.0f);
      double t0 = 1, x0 = 1;
      for (int i = 1; i < k; i++, x0 += 1.0)
	t0 *= x0;
      return t0;
    }
  if (__glibc_unlikely (x < -42.0f))
    /* negative non-integer */
    /* For x < -42, x non-integer, |gamma(x)| < 2^-151.  */
    /* Underflows always happens */
    return __math_uflowf (k & 1);
  /* The array c[] stores a degree-15 polynomial approximation for
     gamma(x).  */
  static const double c[] =
    {
       0x1.c9a76be577123p+0,   0x1.8f2754ddcf90dp+0,  0x1.0d1191949419bp+0,
       0x1.e1f42cf0ae4a1p-2,   0x1.82b358a3ab638p-3,  0x1.e1f2b30cd907bp-5,
       0x1.240f6d4071bd8p-6,   0x1.1522c9f3cd012p-8,  0x1.1fd0051a0525bp-10,
       0x1.9808a8b96c37ep-13,  0x1.b3f78e01152b5p-15, 0x1.49c85a7e1fd04p-18,
       0x1.471ca49184475p-19, -0x1.368f0b7ed9e36p-23, 0x1.882222f9049efp-23,
      -0x1.a69ed2042842cp-25
   };

  double m = z - 0x1.7p+1;
  double i = roundeven_finite (m);
  double step = copysign (1.0, i);
  double d = m - i, d2 = d * d, d4 = d2 * d2, d8 = d4 * d4;
  double f = (c[0] + d * c[1]) + d2 * (c[2] + d * c[3])
	     + d4 * ((c[4] + d * c[5]) + d2 * (c[6] + d * c[7]))
	     + d8 * ((c[8] + d * c[9]) + d2 * (c[10] + d * c[11])
		     + d4 * ((c[12] + d * c[13]) + d2 * (c[14] + d * c[15])));
  int jm = fabs (i);
  double w = 1;
  if (jm)
    {
      z -= 0.5 + step * 0.5;
      w = z;
      for (int j = jm - 1; j; j--)
	{
	  z -= step;
	  w *= z;
	}
    }
  if (i <= -0.5)
    w = 1 / w;
  f *= w;
  uint64_t rt = asuint64 (f);
  float r = f;
  if (__glibc_unlikely (r == 0))
    return __math_uflowf (rt >> 63);
  /* Deal with exceptional cases.  */
  if (__glibc_unlikely (((rt + 2) & 0xfffffff) < 8))
    {
      for (unsigned j = 0; j < array_length (tb); j++)
	if (t == tb[j].u)
	  return tb[j].f + tb[j].df;
    }
  return r;
}

#if LIBM_SVID_COMPAT
versioned_symbol (libm, __tgammaf, tgammaf, GLIBC_2_43);
libm_alias_float_other (__tgamma, tgamma)
#else
libm_alias_float (__tgamma, tgamma)
#endif
libm_alias_finite (__tgammaf, __gammaf_r)
