/* Correctly-rounded logarithm of the absolute value of the gamma function
   for binary32 value.

Copyright (c) 2023-2026 Alexei Sibidanov.

This file is part of the CORE-MATH project
project (file src/binary32/lgamma/lgammaf.c, revision 8ea8ea35).

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

/* Changes with respect to the original CORE-MATH code:
   - removed the dealing with errno
     (this is done in the wrapper math/w_lgammaf_compat2.c).
   - usage of math_narrow_eval to deal with underflow/overflow.
   - deal with signamp.  */

#include <array_length.h>
#include <stdint.h>
#include <math.h>
#include <libm-alias-finite.h>
#include <limits.h>
#include <libm-alias-float.h>
#include <math-svid-compat.h>
#include <math-narrow-eval.h>
#include "math_config.h"

static double
as_r7 (double x, const double *c)
{
  return (((x - c[0]) * (x - c[1])) * ((x - c[2]) * (x - c[3])))
	 * (((x - c[4]) * (x - c[5])) * ((x - c[6])));
}

static double
as_r8 (double x, const double *c)
{
  return (((x - c[0]) * (x - c[1])) * ((x - c[2]) * (x - c[3])))
	 * (((x - c[4]) * (x - c[5])) * ((x - c[6]) * (x - c[7])));
}

static double
as_sinpi (double x)
{
  static const double c[] =
    {
       0x1p+2,                -0x1.de9e64df22ea4p+1,   0x1.472be122401f8p+0,
      -0x1.d4fcd82df91bp-3,    0x1.9f05c97e0aab2p-6,  -0x1.f3091c427b611p-10,
       0x1.b22c9bfdca547p-14, -0x1.15484325ef569p-18
    };
  x -= 0.5;
  double x2 = x * x, x4 = x2 * x2, x8 = x4 * x4;
  return (0.25 - x2)
	 * ((c[0] + x2 * c[1]) + x4 * (c[2] + x2 * c[3])
	    + x8 * ((c[4] + x2 * c[5]) + x4 * (c[6] + x2 * c[7])));
}

static double
as_ln (double x)
{
  uint64_t t = asuint64 (x);
  int e = (t >> 52) - 0x3ff;
  static const double c[] =
    {
       0x1.fffffffffff24p-1, -0x1.ffffffffd1d67p-2,  0x1.55555537802dep-2,
      -0x1.ffffeca81b866p-3,  0x1.999611761d772p-3, -0x1.54f3e581b61bfp-3,
       0x1.1e642b4cb5143p-3, -0x1.9115a5af1e1edp-4
    };
  static const double il[] =
    {
      0x1.59caeec280116p-57, 0x1.f0a30c01162aap-5, 0x1.e27076e2af2ebp-4,
      0x1.5ff3070a793d6p-3,  0x1.c8ff7c79a9a2p-3,  0x1.1675cababa60fp-2,
      0x1.4618bc21c5ec2p-2,  0x1.739d7f6bbd007p-2, 0x1.9f323ecbf984dp-2,
      0x1.c8ff7c79a9a21p-2,  0x1.f128f5faf06ecp-2, 0x1.0be72e4252a83p-1,
      0x1.1e85f5e7040d1p-1,  0x1.307d7334f10bep-1, 0x1.41d8fe84672afp-1,
      0x1.52a2d265bc5abp-1
    };
  static const double ix[] =
    {
      0x1p+0,                0x1.e1e1e1e1e1e1ep-1, 0x1.c71c71c71c71cp-1,
      0x1.af286bca1af28p-1,  0x1.999999999999ap-1, 0x1.8618618618618p-1,
      0x1.745d1745d1746p-1,  0x1.642c8590b2164p-1, 0x1.5555555555555p-1,
      0x1.47ae147ae147bp-1,  0x1.3b13b13b13b14p-1, 0x1.2f684bda12f68p-1,
      0x1.2492492492492p-1,  0x1.1a7b9611a7b96p-1, 0x1.1111111111111p-1,
      0x1.0842108421084p-1
    };
  int i = (t >> 48) & 0xf;
  t = (t & (~UINT64_C(0) >> 12)) | (INT64_C(0x3ff) << 52);
  double z = ix[i] * asdouble (t) - 1;
  double z2 = z * z, z4 = z2 * z2;
  return e * 0x1.62e42fefa39efp-1 + il[i]
	 + z * ((c[0] + z * c[1]) + z2 * (c[2] + z * c[3])
		+ z4 * ((c[4] + z * c[5]) + z2 * (c[6] + z * c[7])));
}

float
__lgammaf_r (float x, int *signgamp)
{
  static const struct
  {
    float x;
    float f;
    float df;
  } tb[] = {
    /* NB: the entries should be sorted by the asuint (x) value.  */
    { 0x1.ecf3fep-73f,   0x1.8f8e5ap+5f,   -0x1p-20f },
    { 0x1.108a5ap-66f,   0x1.6d7b18p+5f,   -0x1p-20f },
    { 0x1.a68bbcp-42f,   0x1.c9c6e8p+4f,    0x1p-21f },
    { 0x1.ddfd06p-12f,   0x1.ec5ba8p+2f,   -0x1p-23f },
    { 0x1.f8a754p-9f,    0x1.63acc2p+2f,    0x1p-23f },
    { 0x1.8d16b2p+5f,    0x1.1e4b4ep+7f,    0x1p-18f },
    { 0x1.359e0ep+10f,   0x1.d9ad02p+12f,  -0x1p-13f },
    { 0x1.a82a2cp+13f,   0x1.c38036p+16f,   0x1p-9f },
    { 0x1.62c646p+14f,   0x1.9075bep+17f,  -0x1p-8f },
    { 0x1.7f298p+31f,    0x1.f44946p+35f,  -0x1p+10f },
    { 0x1.a45ea4p+33f,   0x1.25dcbcp+38f,  -0x1p+13f },
    { 0x1.f9413ep+76f,   0x1.9d5ab4p+82f,  -0x1p+57f },
    { 0x1.dcbbaap+99f,   0x1.fc5772p+105f,  0x1p+80f },
    { 0x1.58ace8p+112f,  0x1.9e4f66p+118f, -0x1p+93f },
    { 0x1.87bdfp+115f,   0x1.e465aep+121f,  0x1p+96f },
    { -0x1.25cb66p-123f, 0x1.547a44p+6f,   -0x1p-19f },
    { -0x1.ecf3fep-73f,  0x1.8f8e5ap+5f,   -0x1p-20f },
    { -0x1.108a5ap-66f,  0x1.6d7b18p+5f,   -0x1p-20f },
    { -0x1.f51c8ep-49f,  0x1.0a572ap+5f,   -0x1p-20f },
    { -0x1.d85bfep-43f,  0x1.d31592p+4f,   -0x1p-21f },
    { -0x1.437e74p-40f,  0x1.b7dec2p+4f,   -0x1p-21f },
    { -0x1.ade594p-30f,  0x1.446ab2p+4f,   -0x1p-21f },
    { -0x1.c2f04p-30f,   0x1.43a6f6p+4f,    0x1p-21f },
    { -0x1.580c1ep+1f,  -0x1.5787c6p-4f,    0x1p-29f },
    { -0x1.69d628p+3f,  -0x1.0eac2ap+4f,   -0x1p-21f },
    { -0x1.627346p+7f,  -0x1.73235ep+9f,   -0x1p-16f },
    { -0x1.efc2a2p+14f, -0x1.222dbcp+18f,  -0x1p-7f }
  };

  float fx = floor (x);
  float ax = fabsf (x);
  uint32_t t = asuint (ax);
  if (__glibc_unlikely (t >= (0xffu << 23)))
    {
      *signgamp = 1;
      if (t == (0xffu << 23))
	return INFINITY;
      return x + x; /* nan */
    }
  if (__glibc_unlikely (fx == x))
    { /* x integer */
      if (x <= 0.0f)
	{
	  *signgamp = asuint (x) >> 31 ? -1 : 1;
	  return __math_divzerof (0);
	}
      if (x == 1.0f || x == 2.0f)
	{
	  *signgamp = 1;
	  return 0.0f;
	}
    }

  /* Check the value of fx to avoid a spurious invalid exception.
     Note that for a binary32 |x| >= 2^23, x is necessarily an integer,
     and we already dealed with negative integers, thus now:
     -2^23 < x < +Inf and x is not a negative integer nor 0, 1, 2. */
  if (__glibc_likely (fx >= 0))
    *signgamp = 1;
  else
    /* gamma(x) is negative in (-2n-1,-2n), thus when fx is odd.  */
    *signgamp = 1 - ((((int) fx) & 1) << 1);

  double z = ax, f;
  double s = x;
  if (__glibc_unlikely (ax < 0x1.52p-1f))
    {
      static const double rn[] =
	{
	  -0x1.505bdf4b65acp+4,  -0x1.51c80eb47e068p+2,
	   0x1.0000000007cb8p+0, -0x1.4ac529250a1fcp+1,
	  -0x1.a8c99dbe1621ap+0, -0x1.4abdcc74115eap+0,
	  -0x1.1b87fe5a5b923p+0, -0x1.05b8a4d47ff64p+0
	};
      const double c0 = 0x1.0fc0fad268c4dp+2;
      static const double rd[] =
	{
	  -0x1.4db2cfe9a5265p+5, -0x1.062e99d1c4f27p+3,
	  -0x1.c81bc2ecf25f6p+1, -0x1.108e55c10091bp+1,
	  -0x1.7dd25af0b83d4p+0, -0x1.36bf1880125fcp+0,
	  -0x1.1379fc8023d9cp+0, -0x1.03712e41525d2p+0
	};
      f = (c0 * s) * as_r8 (s, rn) / as_r8 (s, rd) - as_ln (z);
    }
  else
    { /* |x| >= 0x1.52p-1 */
      if (ax > 0x1.afc1ap+1f)
	{
	  if (__glibc_unlikely (x >= 0x1.895f1cp+121f))
	    {
	      /* for x=0x1.895f1cp+121, lgamma(x) < 2^128, thus there is no
		 overflow for rounding towards zero or downwards.
		 The following expression overflows for x > 0x1.895f1cp+121
		 or x = 0x1.895f1cp+121 and rounding to nearest or away,
		 and does not overflow for x = 0x1.895f1cp+121 and rounding
		 towards zero */
	      float r = fmaf (x, 0x1.4d3398p+6f, 0x1.10f35ep+103f);
	      if (x > 0x1.895f1cp+121f
		  || (x == 0x1.895f1cp+121f
		      && math_narrow_eval (x * 5.0f) >= 0x1.ebb6e4p+123))
		return __math_oflowf (0);
	      return r;
	    }
	  double lz = as_ln (z);
	  f = (z - 0.5) * (lz - 1) + 0x1.acfe390c97d69p-2;
	  if (ax < 0x1.0p+20f)
	    {
	      double iz = 1.0 / z, iz2 = iz * iz;
	      if (ax > 1198.0f)
		f += iz * (1. / 12.);
	      else if (ax > 0x1.279a7p+6f)
		{
		  static const double c[] =
		    {
		      0x1.555555547fbadp-4, -0x1.6c0fd270c465p-9
		    };
		  f += iz * (c[0] + iz2 * c[1]);
		}
	      else if (ax > 0x1.555556p+3f)
		{
		  static const double c[] =
		    {
		      0x1.555555554de0bp-4,  -0x1.6c16bdc45944fp-9,
		      0x1.a0077f300ecb3p-11, -0x1.2e9cfff3b29c2p-11
		    };
		  double iz4 = iz2 * iz2;
		  f += iz * ((c[0] + iz2 * c[1]) + iz4 * (c[2] + iz2 * c[3]));
		}
	      else
		{
		  static const double c[] =
		    {
		      0x1.5555555551286p-4,  -0x1.6c16c0e7c4cf4p-9,
		      0x1.a0193267fe6f2p-11, -0x1.37e87ec19cb45p-11,
		      0x1.b40011dfff081p-11, -0x1.c16c8946b19b6p-10,
		      0x1.e9f47ace150d8p-9,  -0x1.4f5843a71a338p-8
		    };
		  double iz4 = iz2 * iz2, iz8 = iz4 * iz4;
		  double p = ((c[0] + iz2 * c[1]) + iz4 * (c[2] + iz2 * c[3]))
			     + iz8 * ((c[4] + iz2 * c[5])
				      + iz4 * (c[6] + iz2 * c[7]));
		  f += iz * p;
		}
	    }
	  if (x < 0.0f)
	    {
	      f = 0x1.250d048e7a1bdp+0 - f - lz;
	      double lp = as_ln (as_sinpi (x - fx));
	      f -= lp;
	    }
	}
      else
	{
	  static const double rn[] =
	    {
	      -0x1.667923ff14df7p+5, -0x1.2d35f25ad8f64p+3,
	      -0x1.b8c9eab9d5bd3p+1, -0x1.7a4a97f494127p+0,
	      -0x1.3a6c8295b4445p-1, -0x1.da44e8b810024p-3,
	      -0x1.9061e81c77e4ap-5
	    };
	  const double c0 = 0x1.3cc0e6a0106b3p+2;
	  static const double rd[] =
	    {
	      -0x1.491a899e84c52p+6, -0x1.d202961b9e098p+3,
	      -0x1.4ced68c631ed6p+2, -0x1.2589eedf40738p+1,
	      -0x1.1302e3337271p+0,  -0x1.c36b802f26dffp-2,
	      -0x1.3ded448acc39dp-3, -0x1.bffc491078eafp-6
	    };
	  f = (z - 1) * (z - 2) * c0 * as_r7 (z, rn) / as_r8 (z, rd);
	  if (x < 0.0f)
	    {
	      if (__glibc_unlikely (t < 0x40301b93u && t > 0x402f95c2u))
		{
		  double h = (s + 0x1.5fb410a1bd901p+1)
		    - 0x1.a19a96d2e6f85p-54;
		  double h2 = h * h;
		  double h4 = h2 * h2;
		  static const double c[] =
		    {
		      -0x1.ea12da904b18cp+0,  0x1.3267f3c265a54p+3,
		      -0x1.4185ac30cadb3p+4,  0x1.f504accc3f2e4p+5,
		      -0x1.8588444c679b4p+7,  0x1.43740491dc22p+9,
		      -0x1.12400ea23f9e6p+11, 0x1.dac829f365795p+12
		    };
		  f = h * ((c[0] + h * c[1]) + h2 * (c[2] + h * c[3])
			 + h4 * ((c[4] + h * c[5]) + h2 * (c[6] + h * c[7])));
		}
	      else if (__glibc_unlikely (t > 0x401ceccbu && t < 0x401d95cau))
		{
		  double h = (s + 0x1.3a7fc9600f86cp+1)
		    + 0x1.55f64f98af8dp-55;
		  double h2 = h * h;
		  double h4 = h2 * h2;
		  static const double c[] =
		    {
		      0x1.83fe966af535fp+0, 0x1.36eebb002f61ap+2,
		      0x1.694a60589a0b3p+0, 0x1.1718d7aedb0b5p+3,
		      0x1.733a045eca0d3p+2, 0x1.8d4297421205bp+4,
		      0x1.7feea5fb29965p+4
		    };
		  f = h
		      * ((c[0] + h * c[1]) + h2 * (c[2] + h * c[3])
			 + h4 * ((c[4] + h * c[5]) + h2 * (c[6])));
		}
	      else if (__glibc_unlikely (t > 0x40492009u && t < 0x404940efu))
		{
		  double h = (s + 0x1.9260dbc9e59afp+1)
		    + 0x1.f717cd335a7b3p-53;
		  double h2 = h * h;
		  double h4 = h2 * h2;
		  static const double c[] =
		    {
		      0x1.f20a65f2fac55p+2,  0x1.9d4d297715105p+4,
		      0x1.c1137124d5b21p+6,  0x1.267203d24de38p+9,
		      0x1.99a63399a0b44p+11, 0x1.2941214faaf0cp+14,
		      0x1.bb912c0c9cdd1p+16
		    };
		  f = h * ((c[0] + h * c[1]) + h2 * (c[2] + h * c[3])
			 + h4 * ((c[4] + h * c[5]) + h2 * (c[6])));
		}
	      else
		{
		  f = 0x1.250d048e7a1bdp+0 - f;
		  double lp = as_ln (as_sinpi (x - fx) * z);
		  f -= lp;
		}
	    }
	}
    }

  uint64_t tl = (asuint64 (f) + 5) & 0xfffffff;
  float r = f;
  if (__glibc_unlikely (tl <= 31u))
    {
      t = asuint (x);
      int a = 0, b = array_length (tb);
      /* invariant: t.u < tb[0].x.u or tb[a].x.u <= t.u < tb[b].x.u */
      while (a + 1 < b)
	{
	  int i = (a + b) / 2;
	  if (t < asuint (tb[i].x))
	    b = i;
	  else
	    a = i;
	}
      if (t == asuint (tb[a].x))
	return tb[a].f + tb[a].df;
    }
  return r;
}
strong_alias (__lgammaf_r, __ieee754_lgammaf_r)
libm_alias_finite (__lgammaf_r, __lgammaf_r)
#if LIBM_SVID_COMPAT
versioned_symbol (libm, __lgammaf_r, lgammaf_r, GLIBC_2_43);
# if __HAVE_FLOAT32 && !__HAVE_DISTINCT_FLOAT32
weak_alias (__lgammaf_r, lgammaf32_r)
# endif
#else
libm_alias_float_r (__lgamma, lgamma, _r)
#endif
