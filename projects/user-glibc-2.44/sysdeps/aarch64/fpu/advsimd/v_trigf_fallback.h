/* Vectorised fallback for single-precision AdvSIMD trig functions.

   Copyright (C) 2026 Free Software Foundation, Inc.
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

/* Row i uses q = i - 3 and stores a 4-term binary32 expansion of
   frac((4/pi) * 2^(8*q)), biased into [0.5, 1.5). With x_reduced exponent in
   [32, 39], ph.hi then lands on a multiple of 2^8 and contributes no bits to
   k mod 8.  */
static const float FOUR_OVER_PI[16][4] = {
  { 0x1.000002p0f, -0x1.7419f2p-25f, -0x1.1b1bbep-51f, -0x1.5ac07cp-76f },
  { 0x1.000146p0f, -0x1.9f246cp-29f, -0x1.bbead6p-55f, -0x1.ec5418p-86f },
  { 0x1.0145f4p0f, -0x1.f246c6p-25f, -0x1.df56bp-50f, -0x1.ec5418p-78f },
  { 0x1.45f306p0f, 0x1.b9391p-25f, 0x1.529fc2p-51f, 0x1.d5f47ep-77f },
  { 0x1.e60dbap-1f, -0x1.8ddf56p-26f, -0x1.603d8ap-51f, -0x1.05c15ap-76f },
  { 0x1.06dc9cp0f, 0x1.1054a8p-25f, -0x1.ec5418p-54f, 0x1.f534dep-79f },
  { 0x1.b9391p-1f, 0x1.529fc2p-27f, 0x1.d5f47ep-53f, -0x1.65912p-78f },
  { 0x1.391054p-1f, 0x1.4fe13ap-26f, 0x1.7d1f54p-51f, -0x1.6447e4p-76f },
  { 0x1.1054a8p-1f, -0x1.ec5418p-30f, 0x1.f534dep-55f, -0x1.f924ecp-82f },
  { 0x1.2a53f8p0f, 0x1.3abe9p-26f, -0x1.596448p-52f, 0x1.b6c52cp-80f },
  { 0x1.53f84ep0f, 0x1.5f47d4p-25f, 0x1.a6ee06p-50f, 0x1.b6295ap-75f },
  { 0x1.f09d6p-1f, -0x1.70565ap-26f, 0x1.dc0db6p-51f, 0x1.4acc9ep-78f },
  { 0x1.4eafa4p0f, -0x1.596448p-28f, 0x1.b6c52cp-56f, -0x1.9b0ef2p-81f },
  { 0x1.5f47d4p-1f, 0x1.a6ee06p-26f, 0x1.b6295ap-51f, -0x1.b0ef1cp-77f },
  { 0x1.47d4d4p-1f, -0x1.11f924p-26f, -0x1.d6a66cp-51f, -0x1.de37ep-78f },
  { 0x1.d4d378p-1f, -0x1.f924ecp-26f, 0x1.5993c4p-51f, 0x1.c821p-78f },
};

/* Table of { sin (K * pi/4), cos (K * pi/4) }.
   For K < 8.  */
static const float SINCOS_K_PI_OVER_4[8][2] = {
  { 0, 0x1p0f },  { 0x1.6a09e6p-1f, 0x1.6a09e6p-1f },
  { 0x1p0f, 0 },  { 0x1.6a09e6p-1f, -0x1.6a09e6p-1f },
  { 0, -0x1p0f }, { -0x1.6a09e6p-1f, -0x1.6a09e6p-1f },
  { -0x1p0f, 0 }, { -0x1.6a09e6p-1f, 0x1.6a09e6p-1f },
};

struct reduction_result_t
{
  uint32x4_t octant;
  float32x4x2_t remainder;
};

/* Error-free multiplication using double-float computation
   using the TwoProd algorithm.
   hi is the rounded product, lo is the exact FMA residual.  */
static inline float32x4x2_t VPCS_ATTR
two_prod (float32x4_t a, float32x4_t b)
{
  float32x4x2_t result;
  result.val[0] = vmulq_f32 (a, b);
  result.val[1] = vfmaq_f32 (vnegq_f32 (result.val[0]), a, b);
  return result;
}

/* Error-free sum using double-float computation.
   using the FastTwoSum algorithm, which requires |a| >= |b|.
   hi is the rounded sum and lo recovers the low-order
   bits lost by that rounding.  */
static inline float32x4x2_t VPCS_ATTR
fast_two_sum (float32x4_t a, float32x4_t b)
{
  float32x4x2_t result;
  result.val[0] = vaddq_f32 (a, b);
  float32x4_t t = vsubq_f32 (result.val[0], a);
  result.val[1] = vsubq_f32 (b, t);
  return result;
}

/* Loads four rows of FOUR_OVER_PI, and then transposes it to align correctly
   for vector multiplies.  */
static inline float32x4x4_t VPCS_ATTR
load_datablock (uint32x4_t idx)
{
  float32x4x4_t data;
  uint32_t idx0 = vgetq_lane_u32 (idx, 0) & 15;
  uint32_t idx1 = vgetq_lane_u32 (idx, 1) & 15;
  uint32_t idx2 = vgetq_lane_u32 (idx, 2) & 15;
  uint32_t idx3 = vgetq_lane_u32 (idx, 3) & 15;

  float32x4_t temp0 = vld1q_f32 (&FOUR_OVER_PI[idx0][0]);
  float32x4_t temp1 = vld1q_f32 (&FOUR_OVER_PI[idx1][0]);
  float32x4_t temp2 = vld1q_f32 (&FOUR_OVER_PI[idx2][0]);
  float32x4_t temp3 = vld1q_f32 (&FOUR_OVER_PI[idx3][0]);

  /* Transpose the table loaded data.  */
  float32x4_t row01_lo = vtrn1q_f32 (temp0, temp1);
  float32x4_t row01_hi = vtrn2q_f32 (temp0, temp1);
  float32x4_t row23_lo = vtrn1q_f32 (temp2, temp3);
  float32x4_t row23_hi = vtrn2q_f32 (temp2, temp3);

  data.val[0] = vreinterpretq_f32_f64 (vzip1q_f64 (
      vreinterpretq_f64_f32 (row01_lo), vreinterpretq_f64_f32 (row23_lo)));
  data.val[1] = vreinterpretq_f32_f64 (vzip1q_f64 (
      vreinterpretq_f64_f32 (row01_hi), vreinterpretq_f64_f32 (row23_hi)));
  data.val[2] = vreinterpretq_f32_f64 (vzip2q_f64 (
      vreinterpretq_f64_f32 (row01_lo), vreinterpretq_f64_f32 (row23_lo)));
  data.val[3] = vreinterpretq_f32_f64 (vzip2q_f64 (
      vreinterpretq_f64_f32 (row01_hi), vreinterpretq_f64_f32 (row23_hi)));

  return data;
}

/* Reduce x for |x| > 0x1p8 inputs, such that:
    x = (k + y) * (pi / 4)

   Returns a struct containing:
    The integer k,
    A double-single remainder y.

   Note that y is returned in terms of 4/pi, and so has to be converted
   back to radians before can be used for polynomial evaluation; this is done
   inside of sincos_eval, as it allows for more efficient constant loading.  */
static inline struct reduction_result_t VPCS_ATTR
large_range_reduction (float32x4_t x)
{
  /* First, x is reduced into the range of [2^32, 2^40), by directly adjusting
     the exponent. This ensures the leading product contribute only multiples
     of 2^8, so the useful bits of k mod 8 are entirely contained within the
     lower product terms.  */
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  int32x4_t x_e_m32
      = vreinterpretq_s32_u32 (vsraq_n_u32 (v_u32 ((-(127 + 32))), ix, 23));

  /* We can then use the new exponent as an index
     for the FOUR_OVER_PI table.  */
  uint32x4_t idx = vreinterpretq_u32_s32 (vsraq_n_s32 (v_s32 (3), x_e_m32, 3));
  float32x4x4_t data = load_datablock (idx);

  /* x_e_m32 has already been split into:
       x_e_m32 = 8 * ROW + offset
     where ROW selected the FOUR_OVER_PI row above.

     We want to keep the offset (x_e_m32 mod 8), and use it to produce a new
     exponent (32 + offset) so that x_reduced is within our intended [32, 39]
     window.  */
  int32x4_t masked = vandq_s32 (x_e_m32, v_s32 (7));
  int32x4_t added = vaddq_s32 (masked, v_s32 ((127 + 32)));
  int32x4_t shifted = vshlq_n_s32 (added, 23);
  uint32x4_t new_exponent = vreinterpretq_u32_s32 (shifted);

  /* Finally, we get our reduced x value, by reinserting the new exponent into
     the original input mantissa.  */
  uint32x4_t x_mantissa = vandq_u32 (ix, v_u32 (0x7fffff));
  uint32x4_t new_ix = vorrq_u32 (new_exponent, x_mantissa);
  float32x4_t unsigned_x_reduced = vreinterpretq_f32_u32 (new_ix);
  float32x4_t x_reduced
      = vbslq_f32 (v_u32 (0x80000000), x, unsigned_x_reduced);

  /* We now use the reduced x to calculate x ~= (k + r) * (pi / 4).
     First, we multiply x_reduced by the first three chunks of the FOUR_OVER_PI
     table, using double-single arithmetic to maintain a high precision
     intermediate.  */
  float32x4x2_t ph = two_prod (x_reduced, data.val[0]);
  float32x4x2_t pm = two_prod (x_reduced, data.val[1]);
  float32x4x2_t pl = two_prod (x_reduced, data.val[2]);

  /* Next, we need to accumulate the results together to get an integer k for
     our octant. However, ph.val[0] will always be a multiple of 2^8, so it
     cannot affect k mod 8. pm.val[1] and pl will always be sufficiently small
     that they cannot affect the integer portion of the result, Therefore we
     only need to sum ph.val[1] and pm.val[0] when computing k mod 8.  */
  float32x4_t sum_hi = vaddq_f32 (ph.val[1], pm.val[0]);
  float32x4_t kd = vrndaq_f32 (sum_hi);

  /* To compute the remainder, we need to remove the rounded integer k and
     accumulate the remaining terms as a double-single remainder.  */
  float32x4_t y_hi = vaddq_f32 (vsubq_f32 (ph.val[1], kd), pm.val[0]);
  float32x4x2_t y_mid = fast_two_sum (pm.val[1], pl.val[0]);
  float32x4_t y_lo = pl.val[1];

  /* The low portion of x_reduced * D3 has no meaningful contribution to the
     result, so a simple FMA is sufficient.  */
  float32x4_t y_l = vfmaq_f32 (y_lo, x_reduced, data.val[3]);

  /* We then accumulate the final remainder.  */
  float32x4x2_t y = fast_two_sum (y_hi, y_mid.val[0]);
  y.val[1] = vaddq_f32 (y.val[1], vaddq_f32 (y_mid.val[1], y_l));

  struct reduction_result_t result;
  result.remainder = y;
  result.octant = vreinterpretq_u32_s32 (vcvtnq_s32_f32 (kd));
  return result;
}

/* Lookup sin(k * pi/4) and cos(k * pi/4).  */
static inline float32x4x2_t VPCS_ATTR
sin_cos_lookup (uint32x4_t k)
{
  float32x4x2_t result;

  unsigned idx0_s = vgetq_lane_u32 (k, 0) & 7;
  unsigned idx1_s = vgetq_lane_u32 (k, 1) & 7;
  unsigned idx2_s = vgetq_lane_u32 (k, 2) & 7;
  unsigned idx3_s = vgetq_lane_u32 (k, 3) & 7;

  float32x2_t row0 = vld1_f32 (&SINCOS_K_PI_OVER_4[idx0_s][0]);
  float32x2_t row1 = vld1_f32 (&SINCOS_K_PI_OVER_4[idx1_s][0]);
  float32x2_t row2 = vld1_f32 (&SINCOS_K_PI_OVER_4[idx2_s][0]);
  float32x2_t row3 = vld1_f32 (&SINCOS_K_PI_OVER_4[idx3_s][0]);

  /* Transpose four { sin, cos } rows into separate sin/cos vectors.  */
  float32x2_t row01_sin = vtrn1_f32 (row0, row1);
  float32x2_t row01_cos = vtrn2_f32 (row0, row1);
  float32x2_t row23_sin = vtrn1_f32 (row2, row3);
  float32x2_t row23_cos = vtrn2_f32 (row2, row3);

  result.val[0] = vcombine_f32 (row01_sin, row23_sin);
  result.val[1] = vcombine_f32 (row01_cos, row23_cos);
  return result;
}

static const struct eval_data
{
  float pi_over_4_hi, pi_over_4_lo, s2, c3;
  float32x4_t s0, s1;
  float32x4_t c0, c1, c2;
} eval_data = { .s0 = V4 (-0x1.555546p-3f),
		.s1 = V4 (0x1.11076p-7f),
		.s2 = -0x1.994eb4p-13f,
		.c0 = V4 (-0x1p-1f),
		.c1 = V4 (0x1.55554ap-5f),
		.c2 = V4 (-0x1.6c0c1ap-10f),
		.c3 = 0x1.99e0eep-16f,
		.pi_over_4_hi = 0x1.921fb6p-1f,
		.pi_over_4_lo = -0x1.777a5cp-26f };

/* Evaluates sin(r) and cos(r) - 1 for the reduced argument.

   Note that the input y is still terms of 4/pi, and so has to be
   multiplied by pi/4 before evaluation.  */
static inline float32x4x2_t VPCS_ATTR
sincos_eval (float32x4x2_t y)
{
  const struct eval_data *d = ptr_barrier (&eval_data);
  float32x4_t coeffs = vld1q_f32 (&d->pi_over_4_hi);

  /* Reconstruct r = y * (pi/4).  */
  float32x4_t corr = vfmaq_laneq_f32 (vmulq_laneq_f32 (y.val[1], coeffs, 0),
				      y.val[0], coeffs, 1);

  float32x4_t r = vfmaq_laneq_f32 (corr, y.val[0], coeffs, 0);
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t r3 = vmulq_f32 (r2, r);

  /* sin(r) = r + r^3 * P(r^2).  */
  float32x4_t sin = vfmaq_laneq_f32 (d->s1, r2, coeffs, 2);
  sin = vfmaq_f32 (d->s0, r2, sin);
  sin = vfmaq_f32 (r, r3, sin);

  /* Return cos(r) - 1 rather than cos(r) to avoid cancellation during the
     later angle-add reconstruction around exact k * pi/4 values.  */
  float32x4_t cosm1 = vfmaq_laneq_f32 (d->c2, r2, coeffs, 3);
  cosm1 = vfmaq_f32 (d->c1, r2, cosm1);
  cosm1 = vfmaq_f32 (d->c0, r2, cosm1);
  cosm1 = vmulq_f32 (cosm1, r2);

  float32x4x2_t result;
  result.val[0] = sin;
  result.val[1] = cosm1;
  return result;
}
