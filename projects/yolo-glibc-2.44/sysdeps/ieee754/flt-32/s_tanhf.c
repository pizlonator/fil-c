/* Correctly-rounded hyperbolic tangent function for binary32 value.

Copyright (c) 2022-2024 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/tanh/tanhf.c, revision b1ecd83).

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

float
__tanhf (float x)
{
  double z = x;
  uint32_t ux = asuint (x);
  int e = (ux >> 23) & 0xff;
  if (__glibc_unlikely (e == 0xff))
    {
      if (ux << 9)
	return x + x; /* x = nan */
      static const float ir[] = { 1.0f, -1.0f };
      return ir[ux >> 31]; /* x = +-inf */
    }
  if (__glibc_unlikely (e < 115))
    {
      if (__glibc_unlikely (e < 102))
	{
	  if (__glibc_unlikely ((ux << 1) == 0))
	    return x;
	  return fmaf (-x, fabsf (x), x);
	}
      float x2 = x * x;
      return fmaf (x, -0x1.555556p-2f * x2, x);
    }
  if ((ux << 1) > (0x41102cb3u << 1))
    return copysignf (1.0f, x) - copysignf (0x1p-25f, x);
  double z2 = z * z;
  double z4 = z2 * z2;
  double z8 = z4 * z4;
  static const double cn[] =
    {
      0x1p+0,                0x1.30877b8b72d33p-3,  0x1.694aa09ae9e5ep-8,
      0x1.4101377abb729p-14, 0x1.e0392b1db0018p-22, 0x1.2533756e546f7p-30,
      0x1.d62e5abe6ae8ap-41, 0x1.b06be534182dep-54
    };
  static const double cd[] =
    {
      0x1p+0,                0x1.ed99131b0ebeap-2,  0x1.0d27ed6c95a69p-5,
      0x1.7cbdaca0e9fccp-11, 0x1.b4e60b892578ep-18, 0x1.a6f707c5c71abp-26,
      0x1.35a8b6e2cd94cp-35, 0x1.ca8230677aa01p-47
    };
  double n0 = cn[0] + z2 * cn[1];
  double n2 = cn[2] + z2 * cn[3];
  double n4 = cn[4] + z2 * cn[5];
  double n6 = cn[6] + z2 * cn[7];
  n0 += z4 * n2;
  n4 += z4 * n6;
  n0 += z8 * n4;
  double d0 = cd[0] + z2 * cd[1];
  double d2 = cd[2] + z2 * cd[3];
  double d4 = cd[4] + z2 * cd[5];
  double d6 = cd[6] + z2 * cd[7];
  d0 += z4 * d2;
  d4 += z4 * d6;
  d0 += z8 * d4;
  double r = z * n0 / d0;
  return r;
}
libm_alias_float (__tanh, tanh)
