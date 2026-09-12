/* Correctly-rounded inverse hyperbolic sine function for binary32 format.

Copyright (c) 2023-2024 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/asinh/asinhf.c, revision e8774b3).

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
#include <libm-alias-float.h>
#include "math_config.h"
#include "s_asincoshf_data.h"

float
__asinhf (float x)
{
  uint32_t t = asuint (x);
  t &= ~0u >> 1;
  double xs = x;
  if (__glibc_unlikely (t <= 0x3e815667u))
    {
      if (__glibc_unlikely (t <= 0x39ddb3d7u))
	{
	  if (__glibc_unlikely (t == 0))
	    return x;
	  return fmaf (x, -0x1p-25f, x);
	}
      static const double c[] =
	{
	   0x1.5555555555553p-3, -0x1.3333333330e9dp-4,  0x1.6db6db67cb37ap-5,
	  -0x1.f1c71699375dp-6,   0x1.6e8a374c39ff9p-6, -0x1.1c1e98f9d01e1p-6,
	   0x1.c277e96d84026p-7, -0x1.329ff5faf02abp-7
	};
      double x2 = xs * xs;
      double x4 = x2 * x2;
      double  x8 = x4 * x4;
      double f = x2 * (((c[0] + x2 * c[1]) + x4 * (c[2] + x2 * c[3]))
		       + x8 * ((c[4] + x2 * c[5]) + x4 * (c[6] + x2 * c[7])));
      double r = xs - xs * f;
      return r;
    }
  else
    {
      if (__glibc_unlikely (t >= 0x7f800000u))
	return x + x; /* +-inf or nan */
      double xd = fabs (xs);
      double x2 = xd * xd;
      uint64_t tp = asuint64 (xd + sqrt (x2 + 1));
      uint64_t m = tp & (~UINT64_C(0) >> 12);
      int j = (m + (UINT64_C(1) << (52 - 8))) >> (52 - 7);
      int e = (tp >> 52) - 0x3ff;
      double w = asdouble (m | UINT64_C(0x3ff) << 52);
      double z = w * IX[j] - 1.0;
      double z2 = z * z;
      double r = ((LIX[128] * e + LIX[j]) + z * C[0]) + z2 * (C[1] + z * C[2]);
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
	  if (__glibc_unlikely ((asuint64 (r) & INT64_C(0xfffffff)) == 0))
	    {
	      double h = (z * c0 + (Ll + LIX[j])) + (Lh - r);
	      r = r + 64 * h;
	    }
	}
      return copysign (r, xs);
    }
}
libm_alias_float (__asinh, asinh)
