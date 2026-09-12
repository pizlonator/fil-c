/* Correctly-rounded hyperbolic sine function for binary32 value.

Copyright (c) 2022-2024 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/sinh/sinhf.c, revision bbfabd99).

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

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <libm-alias-finite.h>
#include <libm-alias-float.h>
#include <math-svid-compat.h>
#include <math-narrow-eval.h>
#include "math_config.h"
#include "e_sincoshf_data.h"

float
__sinhf (float x)
{
  static const struct
  {
    uint32_t uarg;
    float rh, rl;
  } st = { 0x74250bfeu, 0x1.250bfep-11, 0x1p-36 };
  const double iln2 = 0x1.71547652b82fep+5;
  double z = x;
  uint32_t ux = asuint (x) << 1;
  if (__glibc_unlikely (ux > 0x8565a9f8u))
    { /* |x| >~ 89.4 */
      if (ux >= 0xff000000u)
	return x + x;;
      return __math_oflowf (x < 0);
    }
  if (__glibc_unlikely (ux < 0x7c000000u))
    { /* |x| < 0.125 */
      if (__glibc_unlikely (ux <= 0x74250bfeu))
	{					   /* |x| <= 0x1.250bfep-11 */
	  if (__glibc_unlikely (ux < 0x66000000u)) /* |x| < 0x1p-24 */
	    return fmaf (x, fabsf (x), x);
	  if (__glibc_unlikely (st.uarg == ux))
	    {
	      float sgn = copysignf (1.0f, x);
	      return sgn * st.rh + sgn * st.rl;
	    }
	  return (x * 0x1.555556p-3f) * (x * x) + x;
	}
      static const double cp[] =
	{
	  0x1.5555555555555p-3,  0x1.11111111146e1p-7,
	  0x1.a01a00930dda6p-13, 0x1.71f92198aa6e9p-19
	};
      double z2 = z * z;
      double z4 = z2 * z2;
      return z + (z2 * z)
	* ((cp[0] + z2 * cp[1]) + z4 * (cp[2] + z2 * (cp[3])));
    }
  double a = iln2 * z;
  double ia = roundeven_finite (a);
  double h = a - ia;
  double h2 = h * h;
  int64_t jp = asuint64 (ia + 0x1.8p52);
  int64_t jm = -jp;
  double sp = asdouble (TB[jp & 31] + ((uint64_t)(jp >> 5) << 52));
  double sm = asdouble (TB[jm & 31] + ((uint64_t)(jm >> 5) << 52));
  double te = C[0] + h2 * C[2];
  double to = (C[1] + h2 * C[3]);
  double rp = sp * (te + h * to);
  double rm = sm * (te - h * to);
  double r = rp - rm;
  float ub = r;
  float lb = r - 1.52e-10 * r;
  if (__glibc_unlikely (ub != lb))
    {
      const double iln2h = 0x1.7154765p+5;
      const double iln2l = 0x1.5c17f0bbbe88p-26;
      h = (iln2h * z - ia) + iln2l * z;
      h2 = h * h;
      te = CH[0] + h2 * CH[2] + (h2 * h2) * (CH[4] + h2 * CH[6]);
      to = CH[1] + h2 * (CH[3] + h2 * CH[5]);
      r = sp * (te + h * to) - sm * (te - h * to);
      ub = r;
    }
  return ub;
}
strong_alias (__sinhf, __ieee754_sinhf)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __sinhf, sinhf, GLIBC_2_43);
libm_alias_float_other (__sinh, sinh)
#else
libm_alias_float (__sinh, sinh)
#endif
libm_alias_finite (__ieee754_sinhf, __sinhf)
