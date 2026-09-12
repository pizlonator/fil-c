/* Double-precision (Advanced SIMD) exp10m1 function

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


#include "v_math.h"
#include "v_exp_special_case_inline.h"

/* Value of |x| above which scale overflows without special treatment.  */
#define SpecialBound 0x1.33f424c404a73p+8 /* log10(2^1023) ~ 307.96.  */

/* Value of n above which scale overflows even with special treatment.  */
#define ScaleBound 163840.0 /* 1280.0 * N.  */

/* Value of |x| below which scale - 1 contributes produces large error.  */
#define TableBound 0x1.a308a4198c9d7p-4 /* log10(2) * 87/256.  */

const static struct data
{
  struct v_exp_special_data special_data;
  double c6, c0;
  double c2, c4;
  double log2_10_hi, log2_10_lo;
  float64x2_t c1, c3, c5;
  float64x2_t log10_2, shift;
  float64x2_t special_bound, scale_thresh;
  uint64x2_t sm1_tbl_off, sm1_tbl_mask;
  float64x2_t rnd2zero;
  uint64_t scalem1[88];
} data = {
  /* Special case data from helper.  */
  .special_data = V_EXP_SPECIAL_DATA,

  /* Coefficients generated using Remez algorithm.  */
  .c0 = 0x1.26bb1bbb55516p1,
  .c1 = V2 (0x1.53524c73cea69p1),
  .c2 = 0x1.0470591b30b6bp1,
  .c3 = V2 (0x1.2bd7609b57e25p0),
  .c4 = 0x1.1517d41bddcbep-1,
  .c5 = V2 (0x1.a9a12d6f0755cp-3),
  .c6 = -0x1.76a8d3abd7025p9,

  /* Values of x which should round to the zeroth index of the exp10m1 table.  */
  .rnd2zero = V2 (-0x1.34413509f79ffp-10), /* (2^-8)/log2(10).  */
  .sm1_tbl_off = V2 (24),
  .sm1_tbl_mask = V2 (0x3f),
  .log10_2 = V2 (0x1.a934f0979a371p8), /* N/log2(10).  */
  .log2_10_hi = 0x1.34413509f79ffp-9,  /* log2(10)/N.  */
  .log2_10_lo = -0x1.9dc1da994fd21p-66,
  .shift = V2 (0x1.8p+52),
  .scale_thresh = V2 (ScaleBound),
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than exp10m1's overflow bound
     `log10(DBL_MAX) ≈ 308.25` or underflow bound
     `log10(DBL_TRUE_MIN) ≈ -323.31`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .special_bound = V2 (SpecialBound),

  /* Table containing 2^x - 1, for 2^x values close to 1.
     The table holds values of 2^(i/128) - 1, computed in
     arbitrary precision.
     The 1st half contains values associated to i=0..43.
     The 2nd half contains values associated to i=-44..-1.  */
  .scalem1 = {
    0x0000000000000000, 0x3f763da9fb33356e, 0x3f864d1f3bc03077,
    0x3f90c57a1b9fe12f, 0x3f966c34c5615d0f, 0x3f9c1aca777db772,
    0x3fa0e8a30eb37901, 0x3fa3c7d958de7069, 0x3fa6ab0d9f3121ec,
    0x3fa992456e48fee8, 0x3fac7d865a7a3440, 0x3faf6cd5ffda635e,
    0x3fb1301d0125b50a, 0x3fb2abdc06c31cc0, 0x3fb429aaea92ddfb,
    0x3fb5a98c8a58e512, 0x3fb72b83c7d517ae, 0x3fb8af9388c8de9c,
    0x3fba35beb6fcb754, 0x3fbbbe084045cd3a, 0x3fbd4873168b9aa8,
    0x3fbed5022fcd91cc, 0x3fc031dc431466b2, 0x3fc0fa4c8beee4b1,
    0x3fc1c3d373ab11c3, 0x3fc28e727d9531fa, 0x3fc35a2b2f13e6e9,
    0x3fc426ff0fab1c05, 0x3fc4f4efa8fef709, 0x3fc5c3fe86d6cc80,
    0x3fc6942d3720185a, 0x3fc7657d49f17ab1, 0x3fc837f0518db8a9,
    0x3fc90b87e266c18a, 0x3fc9e0459320b7fa, 0x3fcab62afc94ff86,
    0x3fcb8d39b9d54e55, 0x3fcc6573682ec32c, 0x3fcd3ed9a72cffb7,
    0x3fce196e189d4724, 0x3fcef5326091a112, 0x3fcfd228256400dd,
    0x3fd0582887dcb8a8, 0x3fd0c7d76542a25b, 0xbfcb23213cc8e86c,
    0xbfca96ecd0deb7c4, 0xbfca09f58086c6c2, 0xbfc97c3a3cd7e119,
    0xbfc8edb9f5703dc0, 0xbfc85e7398737374, 0xbfc7ce6612886a6d,
    0xbfc73d904ed74b33, 0xbfc6abf137076a8e, 0xbfc61987b33d329e,
    0xbfc58652aa180903, 0xbfc4f25100b03219, 0xbfc45d819a94b14b,
    0xbfc3c7e359c9266a, 0xbfc331751ec3a814, 0xbfc29a35c86a9b1a,
    0xbfc20224341286e4, 0xbfc1693f3d7be6da, 0xbfc0cf85bed0f8b7,
    0xbfc034f690a387de, 0xbfbf332113d56b1f, 0xbfbdfaa500017c2d,
    0xbfbcc0768d4175a6, 0xbfbb84935fc8c257, 0xbfba46f918837cb7,
    0xbfb907a55511e032, 0xbfb7c695afc3b424, 0xbfb683c7bf93b074,
    0xbfb53f391822dbc7, 0xbfb3f8e749b3e342, 0xbfb2b0cfe1266bd4,
    0xbfb166f067f25cfe, 0xbfb01b466423250a, 0xbfad9b9eb0a5ed76,
    0xbfaafd11874c009e, 0xbfa85ae0438b37cb, 0xbfa5b505d5b6f268,
    0xbfa30b7d271980f7, 0xbfa05e4119ea5d89, 0xbf9b5a991288ad16,
    0xbf95f134923757f3, 0xbf90804a4c683d8f, 0xbf860f9f985bc9f4,
    0xbf761eea3847077b,
  }
};

#define N (1 << V_EXP_TABLE_BITS)
#define IndexMask (N - 1)

static inline uint64x2_t
lookup_sbits (uint64x2_t i)
{
  return (uint64x2_t){ __v_exp_data[i[0] & IndexMask],
		       __v_exp_data[i[1] & IndexMask] };
}

static inline float64x2_t
lookup_sm1bits (float64x2_t x, uint64x2_t u, const struct data *d)
{
  /* Extract sign bit and use as offset into table.  */
  uint64x2_t is_neg = vcltq_f64 (x, d->rnd2zero);
  uint64x2_t offset = vandq_u64 (is_neg, d->sm1_tbl_off);
  uint64x2_t base_idx = vandq_u64 (u, d->sm1_tbl_mask);
  uint64x2_t idx = vaddq_u64 (base_idx, offset);

  uint64x2_t sm1 = { d->scalem1[idx[0]], d->scalem1[idx[1]] };
  return vreinterpretq_f64_u64 (sm1);
}

/* Fast vector implementation of exp10m1.
   Maximum measured error is 2.53 + 0.5 ulp.
   _ZGVnN2v_exp10m1 (0x1.347007ffed62cp-10) got 0x1.63955944d1d83p-9
					   want 0x1.63955944d1d80p-9.  */
float64x2_t VPCS_ATTR V_NAME_D1 (exp10m1) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint64x2_t cmp = vcagtq_f64 (x, d->special_bound);

  /* n = round(x/(log10(2)/N)).  */
  float64x2_t z = vfmaq_f64 (d->shift, x, d->log10_2);
  uint64x2_t u = vreinterpretq_u64_f64 (z);
  float64x2_t n = vsubq_f64 (z, d->shift);

  float64x2_t c24 = vld1q_f64 (&d->c2);
  float64x2_t c60 = vld1q_f64 (&d->c6);
  float64x2_t log_2_10 = vld1q_f64 (&d->log2_10_hi);

  /* r = x - n*log10(2)/N.  */
  float64x2_t r = x;
  r = vfmsq_laneq_f64 (r, n, log_2_10, 0);
  r = vfmsq_laneq_f64 (r, n, log_2_10, 1);

  /* y = exp10(r) - 1 ~= C0 r + C1 r^2 + C2 r^3 + C3 r^4.  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t p12 = vfmaq_laneq_f64 (d->c1, r, c24, 0);
  float64x2_t p34 = vfmaq_laneq_f64 (d->c3, r, c24, 1);
  float64x2_t p56 = vfmaq_laneq_f64 (d->c5, r, c60, 0);

  float64x2_t p36 = vfmaq_f64 (p34, r2, p56);
  float64x2_t p16 = vfmaq_f64 (p12, r2, p36);

  float64x2_t p0 = vmulq_laneq_f64 (r, c60, 1);
  float64x2_t poly = vfmaq_f64 (p0, r2, p16);

  uint64x2_t e = vshlq_n_u64 (u, 52 - V_EXP_TABLE_BITS);

  /* scale = 2^(n/N).  */
  uint64x2_t scale_bits = lookup_sbits (u);
  float64x2_t scale = vreinterpretq_f64_u64 (vaddq_u64 (scale_bits, e));
  float64x2_t scalem1 = vsubq_f64 (scale, v_f64 (1.0));

  /* Use table to gather scalem1 for small values of x.  */
  uint64x2_t is_small = vcaltq_f64 (x, v_f64 (TableBound));
  if (v_any_u64 (is_small))
    scalem1 = vbslq_f64 (is_small, lookup_sm1bits (x, u, d), scalem1);

  /* Construct exp10m1 = (scale - 1) + scale * poly.  */
  float64x2_t y = vfmaq_f64 (scalem1, scale, poly);

  /* Fallback to special case for lanes with overflow.  */
  if (__glibc_unlikely (v_any_u64 (cmp)))
    {
      float64x2_t special
	  = (exp_special (poly, n, scale, d->scale_thresh, &d->special_data));
      float64x2_t specialm1 = vsubq_f64 (special, v_f64 (1.0));
      return vbslq_f64 (cmp, specialm1, y);
    }
  return y;
}
