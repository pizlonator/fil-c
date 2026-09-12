/* Vectorised fallback for Single-Precision SVE trig functions.

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
   frac((2/pi) * 2^(8*q)), biased into [0.5, 1.5). With x_reduced exponent in
   [32, 39], ph.hi then lands on a multiple of 2^8 and contributes no bits to
   q mod 4. Tables are split into groups to improve gather performance.  */
static const struct trigf_fallback_data
{
  float d0[16];
  float d1[16];
  float d2[16];
  float d3[16];
  float pio2;
} trigf_fallback_data = {
  .d0 = {
    0x1p0f,          0x1.0000a2p0f, 0x1.00a2fap0f, 0x1.45f306p-1f,
    0x1.f306dcp-1f, 0x1.06dc9cp-1f, 0x1.6e4e44p0f, 0x1.4e4416p0f,
    0x1.44152ap0f,  0x1.1529fcp0f,  0x1.29fc28p0f, 0x1.f84ebp-1f,
    0x1.2757d2p0f,  0x1.57d1f6p0f, 0x1.a3ea6ap-1f, 0x1.ea69bcp-1f,
  },
  .d1 = {
    0x1.45f306p-25f, 0x1.f306dcp-25f,  -0x1.f246c6p-26f, 0x1.b9391p-26f,
    0x1.391054p-26f, 0x1.1054a8p-26f,  0x1.529fc2p-28f,  -0x1.ac07b2p-25f,
    -0x1.ec5418p-31f, 0x1.3abe9p-27f,  -0x1.505c16p-25f, -0x1.70565ap-27f,
    -0x1.596448p-29f, -0x1.96447ep-25f, -0x1.11f924p-27f, -0x1.f924ecp-27f,
  },
  .d2 = {
    0x1.b9391p-50f,  0x1.391054p-50f, -0x1.df56bp-51f,  0x1.529fc2p-52f,
    0x1.4fe13ap-51f, -0x1.ec5418p-55f, 0x1.d5f47ep-54f, 0x1.5f47d4p-50f,
    0x1.f534dep-56f, -0x1.596448p-53f, 0x1.a6ee06p-51f, 0x1.dc0db6p-52f,
    0x1.b6c52cp-57f, -0x1.24eb54p-51f, -0x1.d6a66cp-52f, 0x1.5993c4p-52f,
  },
  .d3 = {
    0x1.529fc2p-76f,  0x1.4fe13ap-75f,  -0x1.ec5418p-79f, 0x1.d5f47ep-78f,
    0x1.7d1f54p-76f, 0x1.f534dep-80f,  -0x1.65912p-79f,  0x1.a6ee06p-75f,
    -0x1.f924ecp-83f, 0x1.b6c52cp-81f, 0x1.b6295ap-76f,  0x1.4acc9ep-79f,
    -0x1.9b0ef2p-82f, 0x1.93c43ap-76f, -0x1.de37ep-79f,  0x1.c821p-79f,
  },
  .pio2 = 0x1.921fb6p+0f,
};

/* Error-free multiplication using double-float computation via TwoProd.
   hi is the rounded product, lo is the exact FMA residual.  */
static inline svfloat32x2_t
two_prod (svbool_t pg, svfloat32_t a, svfloat32_t b)
{
  svfloat32_t hi = svmul_x (pg, a, b);
  svfloat32_t lo = svnmls_x (pg, hi, a, b);
  return svcreate2 (hi, lo);
}

/* Error-free sum using double-float computation via FastTwoSum, which
   requires |a| >= |b|. hi is the rounded sum and lo recovers the low-order
   bits lost by that rounding.  */
static inline svfloat32x2_t
fast_two_sum (svbool_t pg, svfloat32_t a, svfloat32_t b)
{
  svfloat32_t hi = svadd_x (pg, a, b);
  svfloat32_t t = svsub_x (pg, hi, a);
  svfloat32_t lo = svsub_x (pg, b, t);
  return svcreate2 (hi, lo);
}

/* Gather coefficients of 2/pi for the selected rows.  */
static inline svfloat32x4_t
load_datablock (svbool_t pg, svuint32_t idx,
		const struct trigf_fallback_data *d)
{
  idx = svand_x (pg, idx, 15);

  svfloat32_t d0 = svld1_gather_index (pg, d->d0, idx);
  svfloat32_t d1 = svld1_gather_index (pg, d->d1, idx);
  svfloat32_t d2 = svld1_gather_index (pg, d->d2, idx);
  svfloat32_t d3 = svld1_gather_index (pg, d->d3, idx);

  return svcreate4 (d0, d1, d2, d3);
}

/* Reduce x for |x| > 0x1p8 inputs, such that:
    x = (q + y) * (pi / 2), with y in [-1/2, 1/2]

   Returns a svfloat32x2_t struct containing:
    remainder: The remainder after reduction
    quadrant: Quadrant of x as an integer reinterpreted as a float for packing.

   Designed to be used with the SVE trig instructions.  */
static inline svfloat32x2_t
large_range_reduction (svbool_t pg, svfloat32_t x)
{
  const struct trigf_fallback_data *d = ptr_barrier (&trigf_fallback_data);

  /* First, x is reduced into the range of [2^32, 2^40), by directly adjusting
     the exponent. This ensures the leading product contribute only multiples
     of 2^8, so the useful bits of q mod 4 are entirely contained within the
     lower product terms.  */
  svuint32_t ix = svreinterpret_u32 (x);
  svint32_t x_e_m32
      = svsub_x (pg, svreinterpret_s32 (svlsr_x (pg, ix, 23)), 127 + 32);

  /* We can then use the new exponent as an index for the 2/pi table.  */
  svuint32_t idx
      = svreinterpret_u32 (svadd_x (pg, svasr_x (pg, x_e_m32, 3), 3));
  svfloat32x4_t datablock = load_datablock (pg, idx, d);

  /* x_e_m32 has already been split into:
       x_e_m32 = 8 * ROW + offset
     where ROW selected the 2/pi row above.

     We want to keep the offset (x_e_m32 mod 8), and use it to produce a new
     exponent (32 + offset) so that x_reduced is within our intended [32, 39]
     window.  */
  svint32_t masked
      = svreinterpret_s32 (svand_x (pg, svreinterpret_u32 (x_e_m32), 7));
  svuint32_t new_exponent
      = svreinterpret_u32 (svlsl_x (pg, svadd_x (pg, masked, 127 + 32), 23));

  /* Finally, we get our reduced x value, by reinserting the new exponent into
     the original input mantissa.  */
  svuint32_t signed_mantissa = svand_x (pg, ix, 0x807fffff);
  svfloat32_t x_reduced
      = svreinterpret_f32 (svorr_x (pg, new_exponent, signed_mantissa));

  /* We now use the reduced x to calculate x ~= (q + y) * (pi / 2).
     First, we multiply x_reduced by the first three chunks of the 2/pi
     table, using double-single arithmetic to maintain a high precision
     intermediate.  */
  svfloat32x2_t ph = two_prod (pg, x_reduced, svget4 (datablock, 0));
  svfloat32x2_t pm = two_prod (pg, x_reduced, svget4 (datablock, 1));
  svfloat32x2_t pl = two_prod (pg, x_reduced, svget4 (datablock, 2));

  svfloat32_t ph_lo = svget2 (ph, 1);
  svfloat32_t pm_hi = svget2 (pm, 0);
  svfloat32_t pm_lo = svget2 (pm, 1);
  svfloat32_t pl_hi = svget2 (pl, 0);
  svfloat32_t pl_lo = svget2 (pl, 1);

  /* Next, we need to accumulate the results together to get an integer k for
     our quadrant. However, ph.hi will always be a multiple of 2^8, so it
     cannot affect k mod 4. pm.lo and pl will always be sufficiently small
     that they cannot affect the integer portion of the result. Therefore we
     only need to sum ph.lo and pm.hi when computing k mod 4. Rounding sum_hi
     chooses the nearest quadrant in pi/2 units, which is the control value
     expected by the trig instructions.  */
  svfloat32_t sum_hi = svadd_x (pg, ph_lo, pm_hi);
  svfloat32_t kd = svrinta_x (pg, sum_hi);

  /* To compute the remainder, we need to remove k from the pi/2-scaled
     value and accumulate the remaining terms as a double-single remainder.  */
  svfloat32_t y_hi = svadd_x (pg, svsub_x (pg, ph_lo, kd), pm_hi);
  svfloat32x2_t y_mid = fast_two_sum (pg, pm_lo, pl_hi);

  /* The low portion of x_reduced * D3 has no meaningful contribution to the
    result, so a simple FMA is sufficient.  */
  svfloat32_t y_lo = svmla_x (pg, pl_lo, x_reduced, svget4 (datablock, 3));

  /* We then accumulate the final hi/lo remainders.  */
  y_hi = svadd_x (pg, y_hi, svget2 (y_mid, 0));
  y_lo = svadd_x (pg, y_lo, svget2 (y_mid, 1));

  /* Multiply the accumulated remainders by pi/2, and adding gives a single
     final remainder.  */
  svfloat32_t remainder = svmla_x (pg, svmul_x (pg, y_lo, sv_f32 (d->pio2)),
				   y_hi, sv_f32 (d->pio2));
  svint32_t quadrant = svcvt_s32_x (pg, kd);
  /* Reinterpret quadrant into a float to pack into struct for return.  */
  return svcreate2 (remainder, svreinterpret_f32 (quadrant));
}
