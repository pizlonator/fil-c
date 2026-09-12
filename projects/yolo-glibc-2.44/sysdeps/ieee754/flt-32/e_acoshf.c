/* Correctly-rounded inverse hyperbolic cosine function for binary32 format.

Copyright (c) 2023-2024 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/acosh/acoshf.c, revision d0b9ddd).

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

#include <math.h>
#include <stdint.h>
#include <libm-alias-finite.h>
#include <libm-alias-float.h>
#include <math-svid-compat.h>
#include "math_config.h"
#include "s_asincoshf_data.h"

static __attribute__ ((noinline)) float
as_special (float x)
{
  uint32_t t = asuint (x);
  if (t == 0x3f800000u)
    return 0.0f;
  if ((t << 1) > 0xff000000u)
    return x + x; /* nan */
  if (t == 0x7f800000u)
    return x; /* inf */
  return __math_invalidf (x);
}

float
__acoshf (float x)
{
  uint32_t t = asuint (x);
  if (__glibc_unlikely (t <= 0x3f800000u))
    return as_special (x);
  else if (__glibc_unlikely (t <= 0x3f99db23u))   /* x <= 0x1.33b646p+0 */
    {
      float zf = x - 1.0f;
      double z = zf;
      double a = sqrt (2.0 * z);
      static const double c[] =
         {
	   -0x1.555555555491ep-4,  0x1.333333319c2p-6,
	   -0x1.6db6da26e5e0ep-8,  0x1.f1c698b7100cep-10,
	   -0x1.6e7ca9e152dc2p-11, 0x1.1b4cf187158bdp-12,
	   -0x1.b5137f11a88e5p-14, 0x1.056c6c57152b2p-15
	 };
      double z2 = z * z;
      double z4 = z2 * z2;
      double f = ((c[0] + z * c[1]) + z2 * (c[2] + z * c[3]))
		 + z4 * ((c[4] + z * c[5]) + z2 * (c[6] + z * c[7]));
      double r = a + (a * z) * f;
      return r;
    }
  else if (__glibc_likely (t < 0x7f800000u))
    {
      double xd = x;
      double x2 = xd * xd;
      uint64_t tp = asuint64 (xd + sqrt (x2 - 1));
      uint64_t m = tp & (~UINT64_C(0) >> 12);
      int j = (m + (UINT64_C(1) << (52 - 8))) >> (52 - 7);
      int e = (tp >> 52) - 0x3ff;
      double w = asdouble (m | UINT64_C(0x3ff) << 52);
      double z = w * IX[j] - 1.0;
      double z2 = z * z;
      double r = ((LIX[128] * e + LIX[j]) + z * C[0])
		  + z2 * (C[1] + z * C[2]);
      if (__glibc_unlikely (((asuint64 (r) + 259000) & INT64_C(0xfffffff))
			    < 260000)) /* accurate path */
	{
	  z2 = z * z;
	  double c0 = CP[0] + z * CP[1];
	  double c2 = CP[2] + z * CP[3];
	  double c4 = CP[4] + z * CP[5];
	  c0 += z2 * (c2 + z2 * c4);
	  const double ln2l = 0x1.7f7d1cf79abcap-20;
	  const double ln2h = 0x1.62e4p-1;
	  double Lh = ln2h * e;
	  double Ll = ln2l * e;
	  r = (z * c0 + (Ll + LIX[j])) + Lh;
	  if (__glibc_unlikely ((asuint64 (r) & UINT64_C(0xfffffff)) == 0))
	    {
	      double h = (z * c0 + (Ll + LIX[j])) + (Lh - r);
	      r = r + 64.0 * h;
	    }
	}
      return r;
    }
  else
    return as_special (x);
}
strong_alias (__acoshf, __ieee754_acoshf)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __acoshf, acoshf, GLIBC_2_43);
libm_alias_float_other (__acosh, acosh)
#else
libm_alias_float (__acosh, acosh)
#endif
libm_alias_finite (__ieee754_acoshf, __acoshf)
