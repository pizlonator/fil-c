/* Double-precision AdvSIMD inverse tan

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

#include "v_math.h"

static const struct data
{
  float64x2_t c0, c2, c4, c6, c8, c10, c12, c14, c16, c18;
  float64x2_t pi_over_2;
  double c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
	      [2**-1022, 1.0].  */
  .c0 = V2 (-0x1.555555555552ap-2),	  .c1 = 0x1.9999999995aebp-3,
  .c2 = V2 (-0x1.24924923923f6p-3),	  .c3 = 0x1.c71c7184288a2p-4,
  .c4 = V2 (-0x1.745d11fb3d32bp-4),	  .c5 = 0x1.3b136a18051b9p-4,
  .c6 = V2 (-0x1.110e6d985f496p-4),	  .c7 = 0x1.e1bcf7f08801dp-5,
  .c8 = V2 (-0x1.ae644e28058c3p-5),	  .c9 = 0x1.82eeb1fed85c6p-5,
  .c10 = V2 (-0x1.59d7f901566cbp-5),	  .c11 = 0x1.2c982855ab069p-5,
  .c12 = V2 (-0x1.eb49592998177p-6),	  .c13 = 0x1.69d8b396e3d38p-6,
  .c14 = V2 (-0x1.ca980345c4204p-7),	  .c15 = 0x1.dc050eafde0b3p-8,
  .c16 = V2 (-0x1.7ea70755b8eccp-9),	  .c17 = 0x1.ba3da3de903e8p-11,
  .c18 = V2 (-0x1.44a4b059b6f67p-13),	  .c19 = 0x1.c4a45029e5a91p-17,
  .pi_over_2 = V2 (0x1.921fb54442d18p+0),
};

#define SignMask v_u64 (0x8000000000000000)
#define TinyBound 0x3e10000000000000 /* asuint64(0x1p-30).  */
#define BigBound 0x4340000000000000  /* asuint64(0x1p53).  */

/* Fast implementation of vector atan.
   Based on atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1] using
   z=1/x and shift = pi/2. Maximum observed error is 2.45 ulps:
   _ZGVnN2v_atan (0x1.0008d737eb3e6p+0) got 0x1.92288c551a4c1p-1
				       want 0x1.92288c551a4c3p-1.  */
float64x2_t VPCS_ATTR V_NAME_D1 (atan) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  float64x2_t c13 = vld1q_f64 (&d->c1);
  float64x2_t c57 = vld1q_f64 (&d->c5);
  float64x2_t c911 = vld1q_f64 (&d->c9);
  float64x2_t c1315 = vld1q_f64 (&d->c13);
  float64x2_t c1719 = vld1q_f64 (&d->c17);

  /* Small cases, infs and nans are supported by our approximation technique,
     but do not set fenv flags correctly. Only trigger special case if we need
     fenv.  */
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t sign = vandq_u64 (ix, SignMask);

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := pi/2 + arctan(-1/x) for x > 1
     Hence, use z=-1/a if x>=1, otherwise z=a.  */
  uint64x2_t red = vcagtq_f64 (x, v_f64 (-1.0));
  /* Avoid dependency in abs(x) in division (and comparison).  */
  float64x2_t z = vbslq_f64 (red, vdivq_f64 (v_f64 (-1.0), x), x);

  float64x2_t shift = vreinterpretq_f64_u64 (
      vandq_u64 (red, vreinterpretq_u64_f64 (d->pi_over_2)));

  /* Reinsert sign bit from argument into the shift value.  */
  shift = vreinterpretq_f64_u64 (
      veorq_u64 (vreinterpretq_u64_f64 (shift), sign));

  /* Calculate polynomial approximation P(z^2) with deg(P)=19.  */
  float64x2_t z2 = vmulq_f64 (z, z);
  float64x2_t z4 = vmulq_f64 (z2, z2);
  float64x2_t z8 = vmulq_f64 (z4, z4);
  float64x2_t z16 = vmulq_f64 (z8, z8);

  /* Order-7 Estrin.  */
  float64x2_t p01 = vfmaq_laneq_f64 (d->c0, z2, c13, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, z2, c13, 1);
  float64x2_t p03 = vfmaq_f64 (p01, z4, p23);

  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, z2, c57, 0);
  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, z2, c57, 1);
  float64x2_t p47 = vfmaq_f64 (p45, z4, p67);

  float64x2_t p07 = vfmaq_f64 (p03, z8, p47);

  /* Order-11 Estrin.  */
  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, z2, c911, 0);
  float64x2_t p1011 = vfmaq_laneq_f64 (d->c10, z2, c911, 1);
  float64x2_t p811 = vfmaq_f64 (p89, z4, p1011);

  float64x2_t p1213 = vfmaq_laneq_f64 (d->c12, z2, c1315, 0);
  float64x2_t p1415 = vfmaq_laneq_f64 (d->c14, z2, c1315, 1);
  float64x2_t p1215 = vfmaq_f64 (p1213, z4, p1415);

  float64x2_t p1617 = vfmaq_laneq_f64 (d->c16, z2, c1719, 0);
  float64x2_t p1819 = vfmaq_laneq_f64 (d->c18, z2, c1719, 1);
  float64x2_t p1619 = vfmaq_f64 (p1617, z4, p1819);

  float64x2_t p815 = vfmaq_f64 (p811, z8, p1215);
  float64x2_t p819 = vfmaq_f64 (p815, z16, p1619);

  float64x2_t y = vfmaq_f64 (p07, p819, z16);

  /* Finalize. y = shift + z + z^3 * P(z^2).  */
  y = vfmsq_f64 (v_f64 (-1.0), z2, y);
  return vfmsq_f64 (shift, z, y);
}
