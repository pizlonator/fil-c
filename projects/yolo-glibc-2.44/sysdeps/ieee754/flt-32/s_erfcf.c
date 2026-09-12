/* Correctly-rounded complementary error function for the binary32 format

Copyright (c) 2023, 2024 Alexei Sibidanov.

This file is part of the CORE-MATH project
project (file src/binary32/erfc/erfcf.c revision d0a2be20).

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

static const double E[] =
  {
    0x1p+0,               0x1.0163da9fb3335p+0, 0x1.02c9a3e778061p+0,
    0x1.04315e86e7f85p+0, 0x1.059b0d3158574p+0, 0x1.0706b29ddf6dep+0,
    0x1.0874518759bc8p+0, 0x1.09e3ecac6f383p+0, 0x1.0b5586cf9890fp+0,
    0x1.0cc922b7247f7p+0, 0x1.0e3ec32d3d1a2p+0, 0x1.0fb66affed31bp+0,
    0x1.11301d0125b51p+0, 0x1.12abdc06c31ccp+0, 0x1.1429aaea92dep+0,
    0x1.15a98c8a58e51p+0, 0x1.172b83c7d517bp+0, 0x1.18af9388c8deap+0,
    0x1.1a35beb6fcb75p+0, 0x1.1bbe084045cd4p+0, 0x1.1d4873168b9aap+0,
    0x1.1ed5022fcd91dp+0, 0x1.2063b88628cd6p+0, 0x1.21f49917ddc96p+0,
    0x1.2387a6e756238p+0, 0x1.251ce4fb2a63fp+0, 0x1.26b4565e27cddp+0,
    0x1.284dfe1f56381p+0, 0x1.29e9df51fdee1p+0, 0x1.2b87fd0dad99p+0,
    0x1.2d285a6e4030bp+0, 0x1.2ecafa93e2f56p+0, 0x1.306fe0a31b715p+0,
    0x1.32170fc4cd831p+0, 0x1.33c08b26416ffp+0, 0x1.356c55f929ff1p+0,
    0x1.371a7373aa9cbp+0, 0x1.38cae6d05d866p+0, 0x1.3a7db34e59ff7p+0,
    0x1.3c32dc313a8e5p+0, 0x1.3dea64c123422p+0, 0x1.3fa4504ac801cp+0,
    0x1.4160a21f72e2ap+0, 0x1.431f5d950a897p+0, 0x1.44e086061892dp+0,
    0x1.46a41ed1d0057p+0, 0x1.486a2b5c13cdp+0,  0x1.4a32af0d7d3dep+0,
    0x1.4bfdad5362a27p+0, 0x1.4dcb299fddd0dp+0, 0x1.4f9b2769d2ca7p+0,
    0x1.516daa2cf6642p+0, 0x1.5342b569d4f82p+0, 0x1.551a4ca5d920fp+0,
    0x1.56f4736b527dap+0, 0x1.58d12d497c7fdp+0, 0x1.5ab07dd485429p+0,
    0x1.5c9268a5946b7p+0, 0x1.5e76f15ad2148p+0, 0x1.605e1b976dc09p+0,
    0x1.6247eb03a5585p+0, 0x1.6434634ccc32p+0,  0x1.6623882552225p+0,
    0x1.68155d44ca973p+0, 0x1.6a09e667f3bcdp+0, 0x1.6c012750bdabfp+0,
    0x1.6dfb23c651a2fp+0, 0x1.6ff7df9519484p+0, 0x1.71f75e8ec5f74p+0,
    0x1.73f9a48a58174p+0, 0x1.75feb564267c9p+0, 0x1.780694fde5d3fp+0,
    0x1.7a11473eb0187p+0, 0x1.7c1ed0130c132p+0, 0x1.7e2f336cf4e62p+0,
    0x1.80427543e1a12p+0, 0x1.82589994cce13p+0, 0x1.8471a4623c7adp+0,
    0x1.868d99b4492edp+0, 0x1.88ac7d98a6699p+0, 0x1.8ace5422aa0dbp+0,
    0x1.8cf3216b5448cp+0, 0x1.8f1ae99157736p+0, 0x1.9145b0b91ffc6p+0,
    0x1.93737b0cdc5e5p+0, 0x1.95a44cbc8520fp+0, 0x1.97d829fde4e5p+0,
    0x1.9a0f170ca07bap+0, 0x1.9c49182a3f09p+0,  0x1.9e86319e32323p+0,
    0x1.a0c667b5de565p+0, 0x1.a309bec4a2d33p+0, 0x1.a5503b23e255dp+0,
    0x1.a799e1330b358p+0, 0x1.a9e6b5579fdbfp+0, 0x1.ac36bbfd3f37ap+0,
    0x1.ae89f995ad3adp+0, 0x1.b0e07298db666p+0, 0x1.b33a2b84f15fbp+0,
    0x1.b59728de5593ap+0, 0x1.b7f76f2fb5e47p+0, 0x1.ba5b030a1064ap+0,
    0x1.bcc1e904bc1d2p+0, 0x1.bf2c25bd71e09p+0, 0x1.c199bdd85529cp+0,
    0x1.c40ab5fffd07ap+0, 0x1.c67f12e57d14bp+0, 0x1.c8f6d9406e7b5p+0,
    0x1.cb720dcef9069p+0, 0x1.cdf0b555dc3fap+0, 0x1.d072d4a07897cp+0,
    0x1.d2f87080d89f2p+0, 0x1.d5818dcfba487p+0, 0x1.d80e316c98398p+0,
    0x1.da9e603db3285p+0, 0x1.dd321f301b46p+0,  0x1.dfc97337b9b5fp+0,
    0x1.e264614f5a129p+0, 0x1.e502ee78b3ff6p+0, 0x1.e7a51fbc74c83p+0,
    0x1.ea4afa2a490dap+0, 0x1.ecf482d8e67f1p+0, 0x1.efa1bee615a27p+0,
    0x1.f252b376bba97p+0, 0x1.f50765b6e454p+0,  0x1.f7bfdad9cbe14p+0,
    0x1.fa7c1819e90d8p+0, 0x1.fd3c22b8f71f1p+0
  };

float
__erfcf (float xf)
{
  float axf = fabsf (xf);
  double axd = axf;
  double  x2 = axd * axd;
  uint32_t t = asuint (xf);
  unsigned int at = t & (~0u >> 1);
  unsigned int sgn = t >> 31;
  int64_t i = at > 0x40051000;
  /* for x < -0x1.ea8f94p+1, erfc(x) rounds to 2 (to nearest) */
  if (__glibc_unlikely (t > 0xc07547ca))
    { /* xf < -0x1.ea8f94p+1 */
      if (__glibc_unlikely (t >= 0xff800000))
	{ /* -Inf or NaN */
	  if (t == 0xff800000)
	    return 2.0f;  /* -Inf */
	  return xf + xf; /* NaN */
	}
      return 2.0f - 0x1p-25f; /* rounds to 2 or nextbelow(2) */
    }
  /* at is the absolute value of xf
     for x >= 0x1.41bbf8p+3, erfc(x) < 2^-150, thus rounds to 0 or to 2^-149
     depending on the rounding mode */
  if (__glibc_unlikely (at >= 0x4120ddfc))
    { /* |xf| >= 0x1.41bbf8p+3 */
      if (__glibc_unlikely (at >= 0x7f800000))
	{ /* +Inf or NaN */
	  if (at == 0x7f800000)
	    return 0.0f;  /* +Inf */
	  return xf + xf; /* NaN */
	}
      __set_errno (ERANGE);
      /* 0x1p-149f * 0.25f rounds to 0 or 2^-149 depending on rounding */
      return 0x1p-149f * 0.25f;
    }
  if (__glibc_unlikely (at <= 0x3db80000))
    { /* |x| <= 0x1.7p-4 */
      if (__glibc_unlikely (t == 0xb76c9f62))
	return 0x1.00010ap+0f + 0x1p-25f; /* exceptional case */
      /* for |x| <= 0x1.c5bf88p-26. erfc(x) rounds to 1 (to nearest) */
      if (__glibc_unlikely (at <= 0x32e2dfc4))
	{ /* |x| <= 0x1.c5bf88p-26 */
	  if (__glibc_unlikely (at == 0))
	    return 1.0f;
	  static const float d[] = { -0x1p-26, 0x1p-25 };
	  return 1.0f + d[sgn];
	}
      /* around 0, erfc(x) behaves as 1 - (odd polynomial) */
      static const double c[] =
	{
	  0x1.20dd750429b6dp+0, -0x1.812746b03610bp-2, 0x1.ce2f218831d2fp-4,
	  -0x1.b82c609607dcbp-6, 0x1.553af09b8008ep-8
	};
      double f0 = (double) xf
	    * (c[0] + x2 * (c[1] + x2 * (c[2] + x2 * (c[3] + x2 * (c[4])))));
      return 1.0 - f0;
    }

  /* now -0x1.ea8f94p+1 <= x <= 0x1.41bbf8p+3, with |x| > 0x1.7p-4 */
  const double iln2 = 0x1.71547652b82fep+0;
  const double ln2h = 0x1.62e42fefap-8;
  const double ln2l = 0x1.cf79abd6f5dc8p-47;
  uint64_t jt = asuint64 (x2 * iln2 + -(1024 + 0x1p-8));
  int64_t j = (int64_t) (jt << 12) >> 48;
  double S = asdouble ((uint64_t)((j >> 7) + (0x3ff | sgn << 11)) << 52);
  static const double ch[] =
    {
      -0x1.ffffffffff333p-2, 0x1.5555555556a14p-3, -0x1.55556666659b4p-5,
      0x1.1111074cc7b22p-7
    };
  double d = (x2 + ln2h * j) + ln2l * j;
  double d2 = d * d;
  double e0 = E[j & 127];
  double f = d + d2 * ((ch[0] + d * ch[1]) + d2 * (ch[2] + d * ch[3]));
  static const double ct[][16] =
    {
      {
	 0x1.c162355429b28p-1,   0x1.d99999999999ap+1,  0x1.da951cece2b85p-2,
	-0x1.70ef6cff4bcc4p+0,   0x1.3d7f7b3d617dep+1, -0x1.9d0aa47537c51p+1,
	 0x1.9754ea9a3fcb1p+1,  -0x1.27a5453fcc015p+1,  0x1.1ef2e0531aebap+0,
	-0x1.eca090f5a1c06p-3,  -0x1.7a3cd173a063cp-4,  0x1.30fa68a68fdddp-4,
	 0x1.55ad9a326993ap-10, -0x1.07e7b0bb39fbfp-6,  0x1.2328706c0e95p-10,
	 0x1.d6aa0b7b19cfep-9
      },
      {
	 0x1.137c8983f8516p+2,   0x1.799999999999ap+1,   0x1.05b53aa241333p-3,
	-0x1.a3f53872bf87p-3,    0x1.de4c30742c9d5p-4,  -0x1.cb24bfa591986p-5,
	 0x1.666aec059ca5fp-6,  -0x1.a61250eb26b0bp-8,   0x1.2b28b7924b34dp-10,
	 0x1.41b13a9d45013p-15, -0x1.6dd5e8a273613p-14,  0x1.09ce8ea5e8da5p-16,
	 0x1.33923b4102981p-18, -0x1.1dfd161e3f984p-19, -0x1.c87618fcae3b3p-23,
	 0x1.e8a6ffa0ba2c7p-23
      }
  };
  double z = (axd - ct[i][0]) / (axd + ct[i][1]);
  double z2 = z * z, z4 = z2 * z2;
  double z8 = z4 * z4;
  const double *c = ct[i] + 3;
  double s = (((c[0] + z * c[1]) + z2 * (c[2] + z * c[3]))
	      + z4 * ((c[4] + z * c[5]) + z2 * (c[6] + z * c[7])))
    + z8 * (((c[8] + z * c[9]) + z2 * (c[10] + z * c[11])) + z4 * (c[12]));
  s = ct[i][2] + z * s;
  static const double off[] = { 0, 2 };
  double r = (S * (e0 - f * e0)) * s;
  double y = off[sgn] + r;
  return y;
}
libm_alias_float (__erfc, erfc)
