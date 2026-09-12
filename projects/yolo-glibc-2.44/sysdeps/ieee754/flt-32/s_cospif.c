/* Correctly-rounded cosine of binary32 value for angles in half-revolutions

Copyright (c) 2022-2025 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (src/binary32/cospi/cospif.c, revision bbfabd99).

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
#include "s_sincospif_data.h"
#include "math_config.h"

float
__cospif (float x)
{
  uint32_t ix = asuint (x);
  int32_t e = (ix >> 23) & 0xff;
  if (__glibc_unlikely (e == 0xff))
    {
      if (!(ix << 9))
        return __math_invalidf (x);
      return x + x; /* nan */
    }
  int32_t m = (ix & ~0u >> 9) | 1 << 23;
  int32_t s = 143 - e;
  int32_t p = e - 112;
  if (__glibc_unlikely (p < 0)) /* |x| < 2^-15  */
    {
      uint32_t ax = ix & (~0u>>1);
      /* Warning: -0x1.3bd3ccp+2f * x underflows for |x| < 0x1.9f03p-129 */
      if (ax >= 0x19f030u)
	return fmaf (-0x1.3bd3ccp+2f * x, x, 1.0f);
      else /* |x| < 0x1.9f03p-129 */
	return fmaf (-x, x, 1.0f);
    }
  if (__glibc_unlikely (p > 31))
    {
      if (__glibc_unlikely (p > 63))
	return 1.0f;
      int32_t iq = (uint32_t)m << (p - 32);
      return S[(iq + 32) & 127];
    }
  int32_t k = (uint32_t)m << p;
  if (__glibc_unlikely (k == 0))
    {
      int32_t iq = m >> (32 - p);
      return S[(iq + 32) & 127];
    }
  double z = k;
  double z2 = z * z;
  double fs = SN[0] + z2 * (SN[1] + z2 * SN[2]);
  double fc = CN[0] + z2 * (CN[1] + z2 * CN[2]);
  uint32_t iq = m >> s;
  iq = (iq + 1) >> 1;
  uint32_t is = iq & 127, ic = (iq + 32) & 127;
  double ts = S[ic], tc = S[is];
  double r = ts + (ts * z2) * fc - (tc * z) * fs;
  return r;
}
libm_alias_float (__cospi, cospi)
