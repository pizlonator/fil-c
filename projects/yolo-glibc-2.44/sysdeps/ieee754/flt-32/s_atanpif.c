/* Correctly-rounded half-revolution arc-tangent of binary32 value.

Copyright (c) 2022-2025 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/atanpi/atanpif.c, revision e02000e).

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
#include <libm-alias-float.h>
#include "math_config.h"

float
__atanpif (float x)
{
  uint32_t t = asuint (x);
  int32_t e = (t >> 23) & 0xff;
  bool gt = e >= 127;
  if (__glibc_unlikely (e > 127 + 24))
    {
      float f = copysignf (0.5f, x);
      if (__glibc_unlikely (e == 0xff))
	{
	  if (t << 9)
	    return x + x; /* nan */
	  return f;	  /* inf */
	}
      /* Warning: 0x1.45f306p-2f / x underflows for |x| >= 0x1.45f306p+124 */
      if (fabsf (x) >= 0x1.45f306p+124f)
	return f - copysignf (0x1p-26f, x);
      else
	return f - 0x1.45f306p-2f / x;
    }
  double z = x;
  if (__glibc_unlikely (e < 127 - 13))
    {
      double sx = z * 0x1.45f306dc9c883p-2;
      if (__glibc_unlikely (e < 127 - 25))
	{
	  float rsx = sx;
	  if (x != 0 && rsx == 0)
	    __set_errno (ERANGE);
	  return rsx;
	}
      return sx - (0x1.5555555555555p-2 * sx) * (z * z);
    }
  uint32_t ax = t & (~0u >> 1);
  if (__glibc_unlikely (ax == 0x3fa267ddu))
    return copysignf (0x1.267004p-2f, x) - copysignf (0x1p-55f, x);
  if (__glibc_unlikely (ax == 0x3f693531u))
    return copysignf (0x1.e1a662p-3f, x) + copysignf (0x1p-28f, x);
  if (__glibc_unlikely (ax == 0x3f800000u))
    return copysignf (0x1p-2f, x);
  if (gt)
    z = 1 / z;
  double z2 = z * z;
  double z4 = z2 * z2;
  double z8 = z4 * z4;
  static const double cn[] =
    {
      0x1.45f306dc9c882p-2, 0x1.733b561bc23d5p-1, 0x1.28d9805bdfbf2p-1,
      0x1.8c3ba966ae287p-3, 0x1.94a7f81ee634bp-6, 0x1.a6bbf6127a6dfp-11
    };
  static const double cd[] =
    {
      0x1p+0,               0x1.4e3b3ecc2518fp+1, 0x1.3ef4a360ff063p+1,
      0x1.0f1dc55bad551p+0, 0x1.8da0fecc018a4p-3, 0x1.8fa87803776bfp-7,
      0x1.dadf2ca0acb43p-14
    };
  double cn0 = cn[0] + z2 * cn[1];
  double cn2 = cn[2] + z2 * cn[3];
  double cn4 = cn[4] + z2 * cn[5];
  cn0 += z4 * cn2;
  cn0 += z8 * cn4;
  cn0 *= z;
  double cd0 = cd[0] + z2 * cd[1];
  double cd2 = cd[2] + z2 * cd[3];
  double cd4 = cd[4] + z2 * cd[5];
  double cd6 = cd[6];
  cd0 += z4 * cd2;
  cd4 += z4 * cd6;
  cd0 += z8 * cd4;
  double r = cn0 / cd0;
  if (gt)
    r = copysign (0.5, z) - r;
  return r;
}
libm_alias_float (__atanpi, atanpi)
