/* Correctly-rounded sine of binary32 value for angles in half-revolutions

Copyright (c) 2022-2025 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (src/binary32/sinpi/sinpif.c, revision bbfabd99.

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
#include <errno.h>
#include <fenv.h>
#include <libm-alias-float.h>
#include "s_sincospif_data.h"
#include "math_config.h"

float
__sinpif (float x)
{
  uint32_t ix = asuint (x);
  int32_t e = (ix >> 23) & 0xff;
  if (__glibc_unlikely (e == 0xff))
    {
      if (!(ix << 9))
        return __math_invalidf (x);
      return x + x; /* nan */
    }
  int32_t m = (ix & ~0u >> 9) | 1 << 23, sgn = ix;
  sgn >>= 31;
  m = (m ^ sgn) - sgn;
  int32_t s = 143 - e;
  if (__glibc_unlikely (s < 0))
    {
      if (__glibc_unlikely (s < -6))
	return copysignf (0.0f, x);
      int32_t iq = (uint32_t)m << (-s - 1);
      iq &= 127;
      if (iq == 0 || iq == 64)
	return copysignf (0.0f, x);
      return S[iq];
    }
  else if (__glibc_unlikely (s > 30))
    {
      double z = x, z2 = z * z;
      return z * (0x1.921fb54442d18p+1 + z2 * (-0x1.4abbce625be53p+2));
    }
  int32_t si = 25 - s;
  if (__glibc_unlikely (si >= 0 && ((uint32_t)m << si) == 0))
    return copysignf (0.0f, x);

  int32_t k = (uint32_t)m << (31 - s);
  double z = k, z2 = z * z;
  double fs = SN[0] + z2 * (SN[1] + z2 * SN[2]);
  double fc = CN[0] + z2 * (CN[1] + z2 * CN[2]);
  uint32_t iq = m >> s;
  iq = (iq + 1) >> 1;
  uint32_t is = iq & 127, ic = (iq + 32) & 127;
  double ts = S[is], tc = S[ic];
  double r = ts + (ts * z2) * fc + (tc * z) * fs;
  return r;
}
libm_alias_float (__sinpi, sinpi)
