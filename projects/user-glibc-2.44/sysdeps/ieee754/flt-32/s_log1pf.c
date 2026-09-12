/* Correctly-rounded biased argument natural logarithm function for binary32
   value.

Copyright (c) 2023, 2024  Alexei Sibidanov.

This file is part of the CORE-MATH project
project (file src/binary32/log1p/log1pf.c revision 24ef43a1).

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
#include <libm-alias-float.h>
#include "math_config.h"

static __attribute__ ((noinline)) float
as_special (float x)
{
  uint32_t t = asuint (x);
  if (t == 0xbf800000u)
    return __math_divzerof (1);
  if (t == 0x7f800000u)
    return x; /* +inf */
  uint32_t ax = t << 1;
  if (ax > 0xff000000u)
    return x + x; /* nan */
  return __math_invalidf (0.0f);
}

float
__log1pf (float x)
{
  /* the reciprocal 1/(1+(j+0.5)/32) is rounded to 23 bits */
  static const double x0[] =
    { 0x1.f81f80p-1, 0x1.e9131cp-1, 0x1.dae608p-1, 0x1.cd8568p-1,
      0x1.c0e070p-1, 0x1.b4e81cp-1, 0x1.a98ef8p-1, 0x1.9ec8e8p-1,
      0x1.948b10p-1, 0x1.8acb90p-1, 0x1.818180p-1, 0x1.78a4c8p-1,
      0x1.702e04p-1, 0x1.681680p-1, 0x1.605818p-1, 0x1.58ed24p-1,
      0x1.51d080p-1, 0x1.4afd6cp-1, 0x1.446f88p-1, 0x1.3e22ccp-1,
      0x1.381380p-1, 0x1.323e34p-1, 0x1.2c9fb4p-1, 0x1.27350cp-1,
      0x1.21fb78p-1, 0x1.1cf06cp-1, 0x1.181180p-1, 0x1.135c80p-1,
      0x1.0ecf58p-1, 0x1.0a6810p-1, 0x1.0624dcp-1, 0x1.020408p-1
    };

  /* the logarithm of the reciprocal is offset by 0x1.7654p-37 so
     log1p_fast(x) - log1p(x) > 0 */
  static const double lix[] = {
      0x1.fc0b0b1599ce4p-7, 0x1.77457a64a42abp-5, 0x1.341d74627847dp-4,
      0x1.a926d8a568810p-4, 0x1.0d77e8cd667aap-3, 0x1.44d2b38d15679p-3,
      0x1.7ab886a16b2b3p-3, 0x1.af3c9b686996dp-3, 0x1.e27075e30cc37p-3,
      0x1.0a3250a767d98p-2, 0x1.229423bd2662ep-2, 0x1.3a64c596c3292p-2,
      0x1.51aadd530e505p-2, 0x1.686c85e9e0177p-2, 0x1.7eaf7df859caep-2,
      0x1.94793ee2403b3p-2, 0x1.a9cec5a9cf512p-2, 0x1.beb4d3baa086fp-2,
      0x1.d32fe2a03d8b5p-2, 0x1.e744257d97431p-2, 0x1.faf58cf7bdfe7p-2,
      0x1.0723e6d1e5599p-1, 0x1.109f3b52ec2f3p-1, 0x1.19ee6a7693fc5p-1,
      0x1.23130d9c03597p-1, 0x1.2c0e9cc4604f1p-1, 0x1.34e28bd9e5837p-1,
      0x1.3d9028a72cd5fp-1, 0x1.4618b9c1dd52dp-1, 0x1.4e7d825b8d20bp-1,
      0x1.56bf9fab56a02p-1, 0x1.5ee02ab258ccap-1
  };
  static const double b[] =
    {
      0x1p+0,
      -0x1p-1,
      0x1.5555555556f6bp-2,
      -0x1.00000000029b9p-2,
      0x1.9999988d176e4p-3,
      -0x1.55555418889a7p-3,
      0x1.24adeca50e2bcp-3,
      -0x1.001ba33bf57cfp-3
    };
  static const double c[] =
    {
      0x1.ffffffe1eac82p-1, -0x1.ffffff7da1724p-2, 0x1.5564d8fa59d0cp-2,
      -0x1.001219d3dba2ap-2
    };

  double z = x;
  uint32_t ux = asuint (x);
  if (__glibc_unlikely (ux >= 0xbf800000u))
    return as_special (x); // x<=-1, x=-inf, x=-nan
  uint32_t ax = ux & (~0u >> 1);
  if (__glibc_unlikely (ax >= 0x7f800000u))
    return as_special (x); // x=+inf, x=+nan
  if (__glibc_likely (ax < 0x3c880000u))
    { // |x| < 0x1.1p-6
      if (__glibc_unlikely (ax < 0x33000000u))
	{ // |x| < 0x1p-25
	  if (!ax)
	    return x;
	  return fmaf (x, -x, x);
	}
      double z2 = z * z, z4 = z2 * z2;
      double f = z2
		 * ((b[1] + z * b[2]) + z2 * (b[3] + z * b[4])
		    + z4 * (b[5] + z * (b[6] + z * b[7])));
      double r = z + f;
      if (__glibc_unlikely (asuint64 (r) & 0xfffffffll) == 0)
	r += 0x1p14 * (f + (z - r));
      return r;
    }
  else
    {
      uint64_t tp = asuint64 (z + 1.0);
      int e = (tp >> 52) - 0x3ff;
      uint64_t m52 = tp & (~0ull >> 12);
      unsigned j = (tp >> (52 - 5)) & 31;
      double xd = asdouble (m52 | UINT64_C (0x3ff) << 52);
      z = xd * x0[j] - 1; // z is exact for x<0x1.0cp+30
      const double ln2 = 0x1.62e42fefa39efp-1;
      double z2 = z * z,
	     r = (ln2 * e + lix[j])
		 + z * ((c[0] + z * c[1]) + z2 * (c[2] + z * c[3]));
      const double eps = 2.1555e-11;
      float ub = r, lb = r - eps;
      if (__glibc_unlikely (ub != lb))
	{
	  double z4 = z2 * z2,
		 f = z2
		     * ((b[1] + z * b[2]) + z2 * (b[3] + z * b[4])
			+ z4 * (b[5] + z * (b[6] + z * b[7])));
	  double lj = lix[j] - 0x1.7654p-37; // subtract the offset
	  const double ln2l = 0x1.7f7d1cf79abcap-20, ln2h = 0x1.62e4p-1;
	  double Lh = ln2h * e, Ll = ln2l * e;
	  Ll += z;
	  double rh = Lh + lj, rl = ((Lh - rh) + lj) + (Ll + f);
	  float fh = rh + rl;
	  double Fl = (rh - (double) fh) + rl;
	  float fl = Fl, tfl = fl * 2.0f;
	  if ((fh + tfl) - fh == tfl)
	    fl += copysignf (0.5f, (float) (Fl - (double) fl)) * fabsf (fl);
	  ub = fh + fl;
	}
      return ub;
    }
}
libm_alias_float (__log1p, log1p)
strong_alias (__log1pf, __logp1f)
libm_alias_float (__logp1, logp1)
