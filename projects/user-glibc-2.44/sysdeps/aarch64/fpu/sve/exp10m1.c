/* Double-precision (SVE) exp10m1 function

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#include "sv_math.h"
#include "sv_exp_special_inline.h"

/* Value of |x| above which scale overflows without special treatment.
   log10(2^1023 + 1/128) ~ 307.96.  */
#define SpecialBound 0x1.33f4bedd4f3fdp+8

/* Value of |x| below which scale - 1 contributes produces large error.  */
#define FexpaBound 0x1.2a9f2b61a7e2p-4 /* 31*(log10(2)/128).  */

static const struct data
{
  struct sv_exp_special_data special_data;
  double log2_10_hi, log2_10_lo;
  double log10_2, c1;
  double c3, c5;
  double c0, c2, c4;
  double shift, special_bound;
  uint64_t scalem1[32];
} data = {
  .special_data = SV_EXP_SPECIAL_DATA,
  /* Coefficients generated using Remez algorithm.  */
  .c0 = 0x1.26bb1bbb55516p1,
  .c1 = 0x1.53524c73cea6ap1,
  .c2 = 0x1.0470591d8bd2ep1,
  .c3 = 0x1.2bd7609dfea43p0,
  .c4 = 0x1.142d0d89058f1p-1,
  .c5 = 0x1.a80cf2ddd513p-3,

  /* 1.5*2^46+1023. This value is further explained below.  */
  .shift = 0x1.800000000ffc0p+46,
  .log10_2 = 0x1.a934f0979a371p1,     /* 1/log2(10).  */
  .log2_10_hi = 0x1.34413509f79ffp-2, /* log2(10).  */
  .log2_10_lo = -0x1.9dc1da994fd21p-59,
  .special_bound = SpecialBound,

  /* Table emulating FEXPA - 1, for values of FEXPA close to 1.
     The table holds values of 2^(i/64) - 1, computed in arbitrary precision.
     The 1st half contains values associated to i=0..+15.
     The 2nd half contains values associated to i=0..-15.  */
  .scalem1 = {
    0x0000000000000000, 0x3f864d1f3bc03077, 0x3f966c34c5615d0f,
    0x3fa0e8a30eb37901, 0x3fa6ab0d9f3121ec, 0x3fac7d865a7a3440,
    0x3fb1301d0125b50a, 0x3fb429aaea92ddfb, 0x3fb72b83c7d517ae,
    0x3fba35beb6fcb754, 0x3fbd4873168b9aa8, 0x3fc031dc431466b2,
    0x3fc1c3d373ab11c3, 0x3fc35a2b2f13e6e9, 0x3fc4f4efa8fef709,
    0x3fc6942d3720185a, 0x0000000000000000, 0xbfc331751ec3a814,
    0xbfc20224341286e4, 0xbfc0cf85bed0f8b7, 0xbfbf332113d56b1f,
    0xbfbcc0768d4175a6, 0xbfba46f918837cb7, 0xbfb7c695afc3b424,
    0xbfb53f391822dbc7, 0xbfb2b0cfe1266bd4, 0xbfb01b466423250a,
    0xbfaafd11874c009e, 0xbfa5b505d5b6f268, 0xbfa05e4119ea5d89,
    0xbf95f134923757f3, 0xbf860f9f985bc9f4,
  },
};

static svfloat64_t NOINLINE
special_m1 (svbool_t special, svfloat64_t y, svfloat64_t z, svfloat64_t scale,
	    svfloat64_t poly, svfloat64_t n,
	    const struct sv_exp_special_data *ds)
{
  /* FEXPA zeroes the sign bit, however the sign is meaningful to the
  special case function so needs to be copied.
  e = sign bit of u << 46.  */
  svuint64_t u = svreinterpret_u64 (z);
  svuint64_t e = svand_x (svptrue_b64 (), svlsl_x (svptrue_b64 (), u, 46),
			  0x8000000000000000);
  /* Copy sign to scale.  */
  scale = svreinterpret_f64 (
      svadd_x (svptrue_b64 (), e, svreinterpret_u64 (scale)));
  svfloat64_t special_result = special_case (scale, poly, n, ds);
  return svsel (special, svsub_x (svptrue_b64 (), special_result, 1.0), y);
}

/* FEXPA based SVE exp10m1 algorithm.
   Maximum measured error is 2.87 + 0.5 ULP:
   _ZGVsMxv_exp10m1(0x1.64645f11e94c6p-4) got 0x1.c64d54eb7658dp-3
					 want 0x1.c64d54eb7658ap-3.  */
svfloat64_t SV_NAME_D1 (exp10m1) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* n = round(x/(log10(2)/N)).  */
  svfloat64_t shift = sv_f64 (d->shift);
  svfloat64_t log10_2_c1 = svld1rq (svptrue_b64 (), &d->log10_2);
  svfloat64_t z = svmla_lane (shift, x, log10_2_c1, 0);
  svfloat64_t n = svsub_x (pg, z, shift);

  /* r = x - n*log10(2)/N.  */
  svfloat64_t log2_10 = svld1rq (svptrue_b64 (), &d->log2_10_hi);
  svfloat64_t r = x;
  r = svmls_lane (r, n, log2_10, 0);
  r = svmls_lane (r, n, log2_10, 1);

  /* scale = 2^(n/N), computed using FEXPA. FEXPA does not propagate NaNs, so
     for consistent NaN handling we have to manually propagate them. This
     comes at significant performance cost.  */
  svuint64_t u = svreinterpret_u64 (z);
  svfloat64_t scale = svexpa (u);
  svfloat64_t c24 = svld1rq (svptrue_b64 (), &d->c3);
  /* Approximate exp10(r) using polynomial.  */
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t p01 = svmla_lane (sv_f64 (d->c0), r, log10_2_c1, 1);
  svfloat64_t p23 = svmla_lane (sv_f64 (d->c2), r, c24, 0);
  svfloat64_t p45 = svmla_lane (sv_f64 (d->c4), r, c24, 1);
  svfloat64_t p25 = svmla_x (pg, p23, p45, r2);
  svfloat64_t p05 = svmla_x (pg, p01, p25, r2);

  svfloat64_t poly = svmul_x (pg, p05, r);

  svfloat64_t scalem1 = svsub_x (pg, scale, 1.0);

  /* For small values, use a lookup table for a more accurate scalem1.  */
  svbool_t is_small = svaclt (pg, x, FexpaBound);
  if (svptest_any (pg, is_small))
    {
      /* Use the low 4 bits of the input of FEXPA as index.  */
      svuint64_t base_idx = svand_x (pg, u, 0xf);

      /* We can use the sign of x as a fifth bit to account for the asymmetry
	 of e^x around 0.  */
      svuint64_t signBit
	  = svlsl_x (pg, svlsr_x (pg, svreinterpret_u64 (x), 63), 4);
      svuint64_t idx = svorr_x (pg, base_idx, signBit);

      /* Lookup values for scale - 1 for small x.  */
      svfloat64_t lookup
	  = svreinterpret_f64 (svld1_gather_index (is_small, d->scalem1, idx));

      /* Select the appropriate scale - 1 value based on x.  */
      scalem1 = svsel (is_small, lookup, scalem1);
    }

  /* Fallback to special case for lanes with overflow.  */
  svbool_t special = svacgt (svptrue_b64 (), x, d->special_bound);
  /* FEXPA returns nan for large inputs so we special case those.  */
  if (__glibc_unlikely (svptest_any (special, special)))
    {
      svfloat64_t y = svmla_x (svptrue_b64 (), scalem1, scale, poly);
      return special_m1 (special, y, z, scale, poly, n, &d->special_data);
    }

  /* return expm1 = (scale - 1) + (scale * poly).  */
  return svmla_x (pg, scalem1, scale, poly);
}
