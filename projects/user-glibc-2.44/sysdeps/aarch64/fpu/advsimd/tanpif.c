/* Single-precision (Advanced SIMD) tanpi function

   Copyright (C) 2024-2026 Free Software Foundation, Inc.
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

const static struct v_tanpif_data
{
  float32x4_t c0, c2, c4, c6;
  float c1, c3, c5, c7;
} tanpif_data = {
  /* Coefficents for tan(pi * x).  */
  .c0 = V4 (0x1.921fb4p1f),  .c1 = 0x1.4abbcep3f,      .c2 = V4 (0x1.466b8p5f),
  .c3 = 0x1.461c72p7f,	     .c4 = V4 (0x1.42e9d4p9f), .c5 = 0x1.69e2c4p11f,
  .c6 = V4 (0x1.e85558p11f), .c7 = 0x1.a52e08p16f,
};

/* Approximation for single-precision vector tanpi(x)
   The maximum error is 3.34 ULP:
   _ZGVnN4v_tanpif(0x1.d6c09ap-2) got 0x1.f70aacp+2
				 want 0x1.f70aa6p+2.  */
float32x4_t VPCS_ATTR V_NAME_F1 (tanpi) (float32x4_t x)
{
  const struct v_tanpif_data *d = ptr_barrier (&tanpif_data);

  float32x4_t n = vrndnq_f32 (x);

  /* inf produces nan that propagates.  */
  float32x4_t xr = vsubq_f32 (x, n);
  float32x4_t ar = vabdq_f32 (x, n);
  uint32x4_t flip = vcgtq_f32 (ar, v_f32 (0.25f));
  float32x4_t r = vbslq_f32 (flip, vsubq_f32 (v_f32 (0.5f), ar), ar);

  /* Order-7 pairwise Horner polynomial evaluation scheme.  */
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t r4 = vmulq_f32 (r2, r2);

  float32x4_t odd_coeffs = vld1q_f32 (&d->c1);
  float32x4_t p01 = vfmaq_laneq_f32 (d->c0, r2, odd_coeffs, 0);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, r2, odd_coeffs, 1);
  float32x4_t p45 = vfmaq_laneq_f32 (d->c4, r2, odd_coeffs, 2);
  float32x4_t p67 = vfmaq_laneq_f32 (d->c6, r2, odd_coeffs, 3);
  float32x4_t p = vfmaq_f32 (p45, r4, p67);
  p = vfmaq_f32 (p23, r4, p);
  p = vfmaq_f32 (p01, r4, p);

  p = vmulq_f32 (r, p);
  float32x4_t p_recip = vdivq_f32 (v_f32 (1.0f), p);
  float32x4_t y = vbslq_f32 (flip, p_recip, p);

  uint32x4_t sign
      = veorq_u32 (vreinterpretq_u32_f32 (xr), vreinterpretq_u32_f32 (ar));
  return vreinterpretq_f32_u32 (vorrq_u32 (vreinterpretq_u32_f32 (y), sign));
}

libmvec_hidden_def (V_NAME_F1 (tanpi))
HALF_WIDTH_ALIAS_F1 (tanpi)
