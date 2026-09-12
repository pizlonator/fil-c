/* Double-precision (Advanced SIMD) exp2m1 function

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
#define SpecialBound 0x1.ff8p+9 /* log2(2^1023) = 1023.00.  */

/* Value of n above which scale overflows even with special treatment.  */
#define ScaleBound 0x1.4p+10 /* 1280.0.  */

/* 87/256, value of x under which table lookup is used for 2^x-1.  */
#define TableBound 0x1.5bfffffffffffp-2

/* Number of bits for each value in the table.  */
#define N (1 << V_EXP_TABLE_BITS)
#define IndexMask (N - 1)

static const struct data
{
  struct v_exp_special_data special_data;
  double log2_lo, c2, c4, c6;
  uint64x2_t sm1_tbl_off, sm1_tbl_mask;
  float64x2_t scale_thresh, special_bound, shift, rnd2zero;
  float64x2_t log2_hi, c1, c3, c5;
  uint64_t scalem1[88];
} data = {
  /* Special case data from helper.  */
  .special_data = V_EXP_SPECIAL_DATA,

  /* Coefficients generated using remez's algorithm for exp2m1(x).  */
  .log2_hi = V2 (0x1.62e42fefa39efp-1),
  .log2_lo = 0x1.abc9e3b39803f3p-56,
  .c1 = V2 (0x1.ebfbdff82c58ep-3),
  .c2 = 0x1.c6b08d71f5804p-5,
  .c3 = V2 (0x1.3b2ab6fee7509p-7),
  .c4 = 0x1.5d1d37eb33b15p-10,
  .c5 = V2 (0x1.423f35f371d9ap-13),
  .c6 = 0x1.e7d57ad9a5f93p-5,
  /* Implementation triggers special case handling as soon as the scale
     overflows, which is earlier than exp2m1's overflow bound
     `log2(DBL_MAX) ≈ 1024.0` or underflow bound
     `log2(DBL_TRUE_MIN) = -1074.0`.
     The absolute comparison catches all these cases efficiently, although
     there is a small window where special cases are triggered
     unnecessarily.  */
  .special_bound = V2 (SpecialBound),
  .shift = V2 (0x1.8p52 / N),
  .rnd2zero = V2 (-0x1p-8),
  .sm1_tbl_off = V2 (24),
  .sm1_tbl_mask = V2 (0x3f),
  .scale_thresh = V2 (ScaleBound),

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

  /* Fall back to table lookup for 2^x - 1, when x is close to zero to
     avoid large errors.  */
  uint64x2_t sm1 = { d->scalem1[idx[0]], d->scalem1[idx[1]] };
  return vreinterpretq_f64_u64 (sm1);
}

/* Double-precision vector exp2(x) - 1 function.
   The maximum error is 2.55 + 0.5 ULP.
   _ZGVnN2v_exp2m1 (0x1.1113e87a035ap-8) got 0x1.7b1d06f0a7d36p-9
					want 0x1.7b1d06f0a7d33p-9.  */
VPCS_ATTR float64x2_t V_NAME_D1 (exp2m1) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* exp2(x) = 2^n (1 + poly(r))
     x = n + r, with r in [-1/2N, 1/2N].
     n is a floating point number, multiple of 1/N.  */
  float64x2_t z = vaddq_f64 (d->shift, x);
  uint64x2_t u = vreinterpretq_u64_f64 (z);
  float64x2_t n = vsubq_f64 (z, d->shift);

  /* Calculate scale, 2^n.  */
  uint64x2_t e = vshlq_n_u64 (u, 52 - V_EXP_TABLE_BITS);
  uint64x2_t scale_bits = lookup_sbits (u);
  float64x2_t scale = vreinterpretq_f64_u64 (vaddq_u64 (scale_bits, e));

  uint64x2_t cmp = vcagtq_f64 (x, d->special_bound);

  /* Pairwise Horner scheme.  */
  float64x2_t r = vsubq_f64 (x, n);
  float64x2_t r2 = vmulq_f64 (r, r);

  float64x2_t log2lo_c2 = vld1q_f64 (&d->log2_lo);
  float64x2_t c4c6 = vld1q_f64 (&d->c4);

  float64x2_t p56 = vfmaq_laneq_f64 (d->c5, r, c4c6, 1);
  float64x2_t p34 = vfmaq_laneq_f64 (d->c3, r, c4c6, 0);
  float64x2_t p12 = vfmaq_laneq_f64 (d->c1, r, log2lo_c2, 1);
  float64x2_t p36 = vfmaq_f64 (p34, r2, p56);
  float64x2_t p16 = vfmaq_f64 (p12, r2, p36);
  float64x2_t poly
      = vfmaq_laneq_f64 (vmulq_f64 (d->log2_hi, r), r, log2lo_c2, 0);
  poly = vfmaq_f64 (poly, p16, r2);

  float64x2_t scalem1 = vsubq_f64 (scale, v_f64 (1.0));

  /* Use table to gather scalem1 for small values of x.  */
  uint64x2_t is_small = vcaltq_f64 (x, v_f64 (TableBound));
  if (v_any_u64 (is_small))
    scalem1 = vbslq_f64 (is_small, lookup_sm1bits (x, u, d), scalem1);

  /* Construct exp2m1 = (scale - 1) + scale * poly.  */
  float64x2_t y = vfmaq_f64 (scalem1, poly, scale);

  /* Fallback to special case for lanes with overflow.  */
  if (__glibc_unlikely (v_any_u64 (cmp)))
    {
      float64x2_t special
	  = (exp_special (poly, n, scale, d->scale_thresh, &d->special_data));
      float64x2_t specialm1 = vsubq_f64 (special, v_f64 (1.0f));
      return vbslq_f64 (cmp, specialm1, y);
    }
  return y;
}
