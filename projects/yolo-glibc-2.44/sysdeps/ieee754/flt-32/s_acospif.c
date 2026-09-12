/* Correctly-rounded half-revolution arc-cosine function for binary32 value.

Copyright (c) 2022-2025 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/acospi/acospif.c, revision 1a6a9ab).

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
#include "s_asincospif_data.h"

float
__acospif (float x)
{
  float ax = fabsf (x);
  double az = ax;
  double z = x;
  uint32_t t = asuint (x);
  int e = (t >> 23) & 0xff;
  if (__glibc_unlikely (e >= 127))
    {
      if (x == 1.0f)
	return 0.0f;
      if (x == -1.0f)
	return 1.0f;
      if (e == 0xff && (t << 9))
	return x + x; /* nan */
      return __math_edomf ((x - x) / (x - x)); /* nan */
    }
  int s = 146 - e;
  int i = 0;
  if (__glibc_likely (s < 32))
    i = ((t & (~0u >> 9)) | 1 << 23) >> s;
  const double *c = CH[i];
  double z2 = z * z;
  double z4 = z2 * z2;
  if (__glibc_unlikely (i == 0))
    {
      double c0 = c[0] + z2 * c[1];
      double c2 = c[2] + z2 * c[3];
      double c4 = c[4] + z2 * c[5];
      double c6 = c[6] + z2 * c[7];
      c0 += c2 * z4;
      c4 += c6 * z4;
      /* For |x| <= 0x1.0fd288p-127, c0 += c4*(z4*z4) would raise a spurious
	 underflow exception, we use an FMA instead, where c4 * z4 does not
	 underflow. */
#ifndef __FP_FAST_FMA
      if (__glibc_likely (ax > 0x1.0fd288p-127))
	c0 = (c4 * z4) * z4 + c0;
      else
#endif
	c0 = fma (c4 * z4, z4, c0);
      return 0.5 - z * c0;
    }
  else
    {
      double f = sqrt (1 - az);
      double c0 = c[0] + az * c[1];
      double c2 = c[2] + az * c[3];
      double c4 = c[4] + az * c[5];
      double c6 = c[6] + az * c[7];
      c0 += c2 * z2;
      c4 += c6 * z2;
      c0 += c4 * z4;
      static const double o[] = { 0, 1 };
      double r = o[t >> 31] + c0 * copysign (f, x);
      return r;
    }
}
libm_alias_float (__acospi, acospi)
