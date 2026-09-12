/* Correctly-rounded cubic root of binary32 value.

Copyright (c) 2023, 2024 Alexei Sibidanov.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/cbrt/cbrtf.c, revision f7c7408d).

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

#include <fenv.h>
#include <libm-alias-float.h>
#include <math.h>
#include <stdint.h>
#include "math_config.h"

float
__cbrtf (float x)
{
  static const union
  {
    double d;
    uint64_t u;
  } escale[3] =
  {
    { .d = 1.0 },
    { .d = 0x1.428a2f98d728bp+0 }, /* 2^(1/3) */
    { .d = 0x1.965fea53d6e3dp+0 }, /* 2^(2/3) */
  };
  uint32_t u = asuint (x);
  uint32_t au = u << 1;
  uint32_t sgn = u >> 31;
  uint32_t e = au >> 24;
  if (__glibc_unlikely (au < 1u << 24 || au >= 0xffu << 24))
    {
      if (au >= 0xffu << 24)
	return x + x; /* inf, nan */
      if (au == 0)
	return x;		       /* +-0 */
      int nz = __builtin_clz (au) - 7; /* subnormal */
      au <<= nz;
      e -= nz - 1;
    }
  uint32_t mant = au & 0xffffff;
  e += 899;
  uint32_t et = e / 3, it = e % 3;
  uint64_t isc = escale[it].u;
  isc += (uint64_t) (et - 342) << 52;
  isc |= (uint64_t) sgn << 63;
  double cvt2 = asdouble (isc);
  static const double c[] =
    {
       0x1.2319d352ea5d5p-1,  0x1.67ad8ee258d1ap-1, -0x1.9342edf9cbad9p-2,
       0x1.b6388fc510a75p-3, -0x1.6002455599e2fp-4,  0x1.7b096936192c4p-6,
      -0x1.e5577187e8bf8p-9,  0x1.169ef81d6c34ep-12
    };
  double z = asdouble ((uint64_t) mant << 28 | UINT64_C(0x3ff) << 52);
  double r0 = -0x1.9931c6c2d19d1p-6 / z;
  double z2 = z * z;
  double z4 = z2 * z2;
  double f = ((c[0] + z * c[1]) + z2 * (c[2] + z * c[3]))
	     + z4 * ((c[4] + z * c[5]) + z2 * (c[6] + z * c[7])) + r0;
  double r = f * cvt2;
  float ub = r;
  float lb = r - cvt2 * 1.4182e-9;
  if (__glibc_likely (ub == lb))
    return ub;
  const double u0 = -0x1.ab16ec65d138fp+3;
  double h = f * f * f - z;
  f -= (f * r0 * u0) * h;
  r = f * cvt2;
  uint64_t cvt1 = asuint64 (r);
  ub = r;
  int64_t m0 = cvt1 << 19;
  int64_t  m1 = m0 >> 63;
  if (__glibc_unlikely ((m0 ^ m1) < (UINT64_C(1) << 31)))
    {
      cvt1 = (cvt1 + (UINT64_C(1) << 31)) & UINT64_C(0xffffffff00000000);
      ub = asdouble (cvt1);
    }
  return ub;
}
libm_alias_float (__cbrt, cbrt)
