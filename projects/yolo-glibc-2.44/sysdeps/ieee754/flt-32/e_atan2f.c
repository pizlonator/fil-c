/* Correctly-rounded arctangent function of two binary32 values.

Copyright (c) 2022-2024 Alexei Sibidanov and Paul Zimmermann.

The original version of this file was copied from the CORE-MATH
project (file src/binary32/atan2/atan2f.c, revision 9b28a4a).

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

static inline double
muldd (double xh, double xl, double ch, double cl, double *l)
{
  double ahlh = ch * xl;
  double alhh = cl * xh;
  double ahhh = ch * xh;
  double ahhl = fma (ch, xh, -ahhh);
  ahhl += alhh + ahlh;
  ch = ahhh + ahhl;
  *l = (ahhh - ch) + ahhl;
  return ch;
}

static double
polydd (double xh, double xl, int n, const double c[][2], double *l)
{
  int i = n - 1;
  double ch = c[i][0];
  double cl = c[i][1];
  while (--i >= 0)
    {
      ch = muldd (xh, xl, ch, cl, &cl);
      double th = ch + c[i][0];
      double tl = (c[i][0] - th) + ch;
      ch = th;
      cl += tl + c[i][1];
    }
  *l = cl;
  return ch;
}

/* for y/x tiny, use Taylor approximation z - z^3/3 where z=y/x */
static float
cr_atan2f_tiny (float y, float x)
{
  double dy = y;
  double dx = x;
  double z = dy / dx;
  double e = fma (-z, x, y);
  /* z * x + e = y thus y/x = z + e/x */
  static const double c = -0x1.5555555555555p-2; /* -1/3 rounded to nearest */
  double zz = z * z;
  double cz = c * z;
  e = e / dx + cz * zz;
  uint64_t t = asuint64 (z);
  if ((t & UINT64_C(0xfffffff)) == 0) /* boundary case */
    {
      /* If z and e are of same sign (resp. of different signs), we increase
	 (resp. decrease) the significant of t by 1 to avoid a double-rounding
	 issue when rounding t to binary32. */
      if (z * e > 0)
	t += 1;
      else
	t -= 1;
    }
  double r = asdouble (t);
  float rf = r;
  if (__glibc_unlikely (rf == 0.0f))
    return __math_uflowf (t >> 63);
  return rf;
}

float
__atan2f (float y, float x)
{
  static const double cn[] =
    {
      0x1p+0,               0x1.40e0698f94c35p+1, 0x1.248c5da347f0dp+1,
      0x1.d873386572976p-1, 0x1.46fa40b20f1dp-3,  0x1.33f5e041eed0fp-7,
      0x1.546bbf28667c5p-14
    };
  static const double cd[] =
    {
      0x1p+0,               0x1.6b8b143a3f6dap+1, 0x1.8421201d18ed5p+1,
      0x1.8221d086914ebp+0, 0x1.670657e3a07bap-2, 0x1.0f4951fd1e72dp-5,
      0x1.b3874b8798286p-11
    };
  static const double m[] = { 0, 1 };
#define pi 0x1.921fb54442d18p+1
#define pi2 0x1.921fb54442d18p+0
#define pi2l 0x1.1a62633145c07p-54
  static const double off[] = { 0.0f, pi2, pi, pi2, -0.0f, -pi2, -pi, -pi2 };
  static const double offl[] =
    {
      0.0f, pi2l, 2 * pi2l, pi2l, -0.0f, -pi2l, -2 * pi2l, -pi2l
    };
  static const double sgn[] = { 1, -1 };
  uint32_t ux = asuint (x);
  uint32_t uy = asuint (y);
  uint32_t ax = ux & (~0u >> 1);
  uint32_t ay = uy & (~0u >> 1);
  if (__glibc_unlikely (ay >= (0xff << 23) || ax >= (0xff << 23)))
    {
      /* we use x+y below so that the invalid exception is set
	 for (x,y) = (qnan,snan) or (snan,qnan) */
      if (ay > (0xff << 23))
	return x + y; /* nan */
      if (ax > (0xff << 23))
	return x + y; /* nan */
      bool yinf = ay == (0xff << 23);
      bool xinf = ax == (0xff << 23);
      if (yinf & xinf)
	{
	  if (ux >> 31)
	    return 0x1.2d97c7f3321d2p+1 * sgn[uy >> 31]; /* +/-3pi/4 */
	  else
	    return 0x1.921fb54442d18p-1 * sgn[uy >> 31]; /* +/-pi/4 */
	}
      if (xinf)
	{
	  if (ux >> 31)
	    return pi * sgn[uy >> 31];
	  else
	    return 0.0 * sgn[uy >> 31];
	}
      if (yinf)
	return pi2 * sgn[uy >> 31];
    }
  if (__glibc_unlikely (ay == 0))
    {
      if (__glibc_unlikely (!ax))
	{
	  uint32_t i = (uy >> 31) * 4 + (ux >> 31) * 2;
	  if (ux >> 31)
	    return off[i] + offl[i];
	  else
	    return off[i];
	}
      if (!(ux >> 31))
	return 0.0 * sgn[uy >> 31];
    }
  uint32_t gt = ay > ax;
  uint32_t i = (uy >> 31) * 4 + (ux >> 31) * 2 + gt;

  double zx = x;
  double zy = y;
  double z = (m[gt] * zx + m[1 - gt] * zy) / (m[gt] * zy + m[1 - gt] * zx);
  /* z = x/y if |y| > |x|, and z = y/x otherwise */
  double r;
  int d = (int) ax - (int) ay;
  if (__glibc_likely (d < (27 << 23) && d > (-(27 << 23))))
    {
      double z2 = z * z, z4 = z2 * z2, z8 = z4 * z4;
      /* z2 cannot underflow, since for |y|=0x1p-149 and |x|=0x1.fffffep+127
	 we get |z| > 2^-277 thus z2 > 2^-554, but z4 and z8 might underflow,
	 which might give spurious underflow exceptions. */
      double cn0 = cn[0] + z2 * cn[1];
      double cn2 = cn[2] + z2 * cn[3];
      double cn4 = cn[4] + z2 * cn[5];
      double cn6 = cn[6];
      cn0 += z4 * cn2;
      cn4 += z4 * cn6;
      cn0 += z8 * cn4;
      double cd0 = cd[0] + z2 * cd[1];
      double cd2 = cd[2] + z2 * cd[3];
      double cd4 = cd[4] + z2 * cd[5];
      double cd6 = cd[6];
      cd0 += z4 * cd2;
      cd4 += z4 * cd6;
      cd0 += z8 * cd4;
      r = cn0 / cd0;
    }
  else
    r = 1;
  z *= sgn[gt];
  r = z * r + off[i];
  if (__glibc_unlikely (((asuint64 (r) + 8) & 0xfffffff) <= 16))
    {
      /* check tiny y/x */
      if (ay < ax && ((ax - ay) >> 23 >= 25))
	return cr_atan2f_tiny (y, x);
      double zh;
      double zl;
      if (gt == 0)
	{
	  zh = zy / zx;
	  zl = fma (zh, -zx, zy) / zx;
	}
      else
	{
	  zh = zx / zy;
	  zl = fma (zh, -zy, zx) / zy;
	}
      double z2l;
      double z2h = muldd (zh, zl, zh, zl, &z2l);
      static const double c[32][2] =
	{
	  {  0x1p+0,                -0x1.8c1dac5492248p-87 },
	  { -0x1.5555555555555p-2,  -0x1.55553bf3a2abep-56 },
	  {  0x1.999999999999ap-3,  -0x1.99deed1ec9071p-57 },
	  { -0x1.2492492492492p-3,  -0x1.fd99c8d18269ap-58 },
	  {  0x1.c71c71c71c717p-4,  -0x1.651eee4c4d9dp-61 },
	  { -0x1.745d1745d1649p-4,  -0x1.632683d6c44a6p-58 },
	  {  0x1.3b13b13b11c63p-4,   0x1.bf69c1f8af41dp-58 },
	  { -0x1.11111110e6338p-4,   0x1.3c3e431e8bb68p-61 },
	  {  0x1.e1e1e1dc45c4ap-5,  -0x1.be2db05c77bbfp-59 },
	  { -0x1.af286b8164b4fp-5,   0x1.a4673491f0942p-61 },
	  {  0x1.86185e9ad4846p-5,   0x1.e12e32d79fceep-59 },
	  { -0x1.642c6d5161faep-5,   0x1.3ce76c1ca03fp-59 },
	  {  0x1.47ad6f277e5bfp-5,  -0x1.abd8d85bdb714p-60 },
	  { -0x1.2f64a2ee8896dp-5,   0x1.ef87d4b615323p-61 },
	  {  0x1.1a6a2b31741b5p-5,   0x1.a5d9d973547eep-62 },
	  { -0x1.07fbdad65e0a6p-5,  -0x1.65ac07f5d35f4p-61 },
	  {  0x1.ee9932a9a5f8bp-6,   0x1.f8b9623f6f55ap-61 },
	  { -0x1.ce8b5b9584dc6p-6,   0x1.fe5af96e8ea2dp-61 },
	  {  0x1.ac9cb288087b7p-6,  -0x1.450cdfceaf5cap-60 },
	  { -0x1.84b025351f3e6p-6,   0x1.579561b0d73dap-61 },
	  {  0x1.52f5b8ecdd52bp-6,   0x1.036bd2c6fba47p-60 },
	  { -0x1.163a8c44909dcp-6,   0x1.18f735ffb9f16p-60 },
	  {  0x1.a400dce3eea6fp-7,  -0x1.c90569c0c1b5cp-61 },
	  { -0x1.1caa78ae6db3ap-7,  -0x1.4c60f8161ea09p-61 },
	  {  0x1.52672453c0731p-8,   0x1.834efb598c338p-62 },
	  { -0x1.5850c5be137cfp-9,  -0x1.445fc150ca7f5p-63 },
	  {  0x1.23eb98d22e1cap-10, -0x1.388fbaf1d783p-64 },
	  { -0x1.8f4e974a40741p-12,  0x1.271198a97da34p-66 },
	  {  0x1.a5cf2e9cf76e5p-14, -0x1.887eb4a63b665p-68 },
	  { -0x1.420c270719e32p-16,  0x1.efd595b27888bp-71 },
	  {  0x1.3ba2d69b51677p-19, -0x1.4fb06829cdfc7p-73 },
	  { -0x1.29b7e6f676385p-23, -0x1.a783b6de718fbp-77 }
	};
      double pl;
      double ph = polydd (z2h, z2l, 32, c, &pl);
      zh *= sgn[gt];
      zl *= sgn[gt];
      ph = muldd (zh, zl, ph, pl, &pl);
      double sh = ph + off[i];
      double sl = ((off[i] - sh) + ph) + pl + offl[i];
      float rf = sh;
      double th = rf;
      double dh = sh - th;
      double tm = dh + sl;
      uint64_t tth = asuint64 (th);
      if (th + th * 0x1p-60 == th - th * 0x1p-60)
	{
	  tth &= UINT64_C(0x7ff) << 52;
	  tth -= UINT64_C(24) << 52;
	  if (fabs (tm) > asdouble (tth))
	    tm *= 1.25;
	  else
	    tm *= 0.75;
	}
      r = th + tm;
    }
  float rf = r;
  if (__glibc_unlikely (rf == 0.0f))
    return __math_uflowf (asuint (rf) >> 1);
  return rf;
}
strong_alias (__atan2f, __ieee754_atan2f)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __atan2f, atan2f, GLIBC_2_43);
libm_alias_float_other (__atan2, atan2)
#else
libm_alias_float (__atan2, atan2)
#endif
libm_alias_finite (__ieee754_atan2f, __atan2f)
