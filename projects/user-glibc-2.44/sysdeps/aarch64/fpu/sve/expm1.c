/* Double-precision SVE expm1

   Copyright (C) 2023-2026 Free Software Foundation, Inc.
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
   ln(2^(1023 + 1/128)) ~ 709.10.  */
#define SpecialBound 0x1.628c2855bfaddp+9

#define FexpaBound 0x1.4cb5ecef28adap-3 /* 15*ln2/64.  */

static const struct data
{
  struct sv_exp_special_data special_data;
  double ln2_hi, ln2_lo;
  double c2, c4;
  double inv_ln2, c1, c3, shift;
  double special_bound, fexpa_bound;
  uint64_t expm1_data[32];
} data = {
  .special_data = SV_EXP_SPECIAL_DATA,
  /* Table emulating FEXPA - 1, for values of FEXPA close to 1.
  The table holds values of 2^(i/64) - 1, computed in arbitrary precision.
  The first half of the table stores values associated to i from 0 to 15.
  The second half of the table stores values associated to i from 0 to -15.  */
  .expm1_data = { 0x0000000000000000, 0x3f864d1f3bc03077, 0x3f966c34c5615d0f,
		  0x3fa0e8a30eb37901, 0x3fa6ab0d9f3121ec, 0x3fac7d865a7a3440,
		  0x3fb1301d0125b50a, 0x3fb429aaea92ddfb, 0x3fb72b83c7d517ae,
		  0x3fba35beb6fcb754, 0x3fbd4873168b9aa8, 0x3fc031dc431466b2,
		  0x3fc1c3d373ab11c3, 0x3fc35a2b2f13e6e9, 0x3fc4f4efa8fef709,
		  0x3fc6942d3720185a, 0x0000000000000000, 0xbfc331751ec3a814,
		  0xbfc20224341286e4, 0xbfc0cf85bed0f8b7, 0xbfbf332113d56b1f,
		  0xbfbcc0768d4175a6, 0xbfba46f918837cb7, 0xbfb7c695afc3b424,
		  0xbfb53f391822dbc7, 0xbfb2b0cfe1266bd4, 0xbfb01b466423250a,
		  0xbfaafd11874c009e, 0xbfa5b505d5b6f268, 0xbfa05e4119ea5d89,
		  0xbf95f134923757f3, 0xbf860f9f985bc9f4 },

  /* Generated using Remez, in [-log(2)/128, log(2)/128].  */
  /* c0 = 0x1p-1 = 0.5. Passed directly as 0.5.  */
  .c1 = 0x1.55555555548f9p-3,
  .c2 = 0x1.5555555554c22p-5,
  .c3 = 0x1.111123aaa2fb2p-7,
  .c4 = 0x1.6c16d77d98e5bp-10,
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  .inv_ln2 = 0x1.71547652b82fep+0,
  .shift = 0x1.800000000ffc0p+46, /* 1.5*2^46+1023.  */
  .special_bound = SpecialBound,
  .fexpa_bound = FexpaBound,
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

/* FEXPA based SVE expm1 algorithm.
   Maximum measured error is 2.81 + 0.5 ULP:
   _ZGVsMxv_expm1 (0x1.974060e619bfp-3) got 0x1.c290e5858bb53p-3
				       want 0x1.c290e5858bb5p-3.  */
svfloat64_t SV_NAME_D1 (expm1) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t z = svmla_x (pg, sv_f64 (d->shift), x, d->inv_ln2);
  svuint64_t u = svreinterpret_u64 (z);
  svfloat64_t n = svsub_x (pg, z, d->shift);

  /* r = x - n * ln2, r is in [-ln2/128, ln2/128].  */
  svfloat64_t ln2 = svld1rq (svptrue_b64 (), &d->ln2_hi);
  svfloat64_t r = x;
  r = svmls_lane (r, n, ln2, 0);
  r = svmls_lane (r, n, ln2, 1);

  /* y = exp(r) - 1 ~= r + C0 r^2 + C1 r^3 + C2 r^4 + C3 r^5 + C4 r^6.  */
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t c24 = svld1rq (svptrue_b64 (), &d->c2);

  svfloat64_t poly;
  svfloat64_t c12 = svmla_lane (sv_f64 (d->c1), r, c24, 0);
  svfloat64_t c34 = svmla_lane (sv_f64 (d->c3), r, c24, 1);
  poly = svmad_x (pg, c34, r2, c12);
  poly = svmad_x (pg, poly, r, sv_f64 (0.5));
  poly = svmad_x (pg, poly, r2, r);

  svfloat64_t scale = svexpa (u);
  svfloat64_t scalem1 = svsub_x (pg, scale, sv_f64 (1.0));

  /* We want to construct expm1(x) = (scale - 1) + scale * poly.
     However, for values of scale close to 1, scale-1 causes large ULP errors
     due to cancellation.

     This can be circumvented by using a small lookup for scale-1
     when our input is below a certain bound, otherwise we can use FEXPA.

     This bound is based upon the table size:
	   Bound = (TableSize-1/64) * ln2.
     The current bound is based upon a table size of 16.  */

  svbool_t is_small = svaclt (pg, x, d->fexpa_bound);

  if (svptest_any (pg, is_small))
    {
      /* Index via the input of FEXPA, but we only care about the lower 4 bits.
       */
      svuint64_t base_idx = svand_x (pg, u, 0xf);

      /* We can use the sign of x as a fifth bit to account for the asymmetry
	 of e^x around 0.  */
      svuint64_t signBit
	  = svlsl_x (pg, svlsr_x (pg, svreinterpret_u64 (x), 63), 4);
      svuint64_t idx = svorr_x (pg, base_idx, signBit);

      /* Lookup values for scale - 1 for small x.  */
      svfloat64_t lookup = svreinterpret_f64 (
	  svld1_gather_index (is_small, d->expm1_data, idx));

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
