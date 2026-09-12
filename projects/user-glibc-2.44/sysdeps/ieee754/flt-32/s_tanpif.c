/* Correctly-rounded tangent of binary32 value for angles in half-revolutions

Copyright (c) 2022-2025 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (src/binary32/tanpi/tanpif.c, revision 3bbf907).

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
#include <errno.h>
#include <libm-alias-float.h>
#include "math_config.h"

float
__tanpif (float x)
{
  uint32_t ix = asuint (x);
  uint32_t e = ix & (0xff << 23);
  if (__glibc_unlikely (e > (150 << 23))) /* |x| > 2^23 */
    {
      if (e == (0xff << 23)) /* x = nan or inf */
	{
	  if (!(ix << 9)) /* x = inf */
	    return __math_invalidf (x);
	  return x + x; /* x = nan */
	}
      return copysign (0.0f, x);
    }
  float x4 = 4.0f * x;
  float nx4 = roundevenf_finite (x4);
  float dx4 = x4 - nx4;
  float ni = roundevenf_finite (x);
  float zf = x - ni;
  if (__glibc_unlikely (dx4 == 0.0f)) /* 4*x integer */
    {
      int k = x4;
      if (k & 1)
	return copysignf (1.0f, zf); /* x = 1/4 mod 1/2 */
      k &= 7;
      if (k == 0)
	return copysignf (0.0f, x); /* x = 0 mod 2 */
      if (k == 4)
	return -copysignf (0.0f, x); /*  x = 1 mod 2 */
      __set_errno (ERANGE);
      if (k == 2)
	return 1.0f / 0.0f; /* x = 1/2 mod 2 */
      if (k == 6)
	return -1.0f / 0.0f; /* x = -1/2 mod 2 */
    }
  ix = asuint (zf);
  uint32_t a = ix & (~0u >> 1);
  /* x=0x1.267004p-2 is not correctly rounded for RNDZ/RNDD by the code below */
  if (__glibc_unlikely (a == 0x3e933802u))
    return copysignf (0x1.44cfbap+0f, zf) + copysignf (0x1p-25f, zf);
  /* x=-0x1.e4cd0ap-14 is not correctly rounded for RNDU by the code below */
  if (__glibc_unlikely (a == 0x38f26685u))
    return copysignf (0x1.7cc304p-12, zf) + copysignf (0x1p-37f, zf);

  double z = zf, z2 = z * z;

  static const double cn[] = { 0x1.921fb54442d19p-1, -0x1.1f458b3e1f8d6p-2,
			       0x1.68a34bd0b8f6ap-6, -0x1.e4866f7a25f99p-13 };
  static const double cd[] = { 0x1p+0, -0x1.4b4b98d2df3a7p-1,
			       0x1.8e9926d2bb901p-4, -0x1.a6f77fd847eep-9 };
  double z4
      = z2 * z2,
      r = (z - z * z2) * ((cn[0] + z2 * cn[1]) + z4 * (cn[2] + z2 * cn[3]))
	  / (((cd[0] + z2 * cd[1]) + z4 * (cd[2] + z2 * cd[3])) * (0.25 - z2));
  return r;
}
libm_alias_float (__tanpi, tanpi)
