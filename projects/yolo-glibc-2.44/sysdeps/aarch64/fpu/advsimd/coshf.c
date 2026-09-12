/* Single-precision vector (AdvSIMD) cosh function

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

#include "v_expf_inline.h"
#include "v_math.h"

static const struct data
{
  struct v_expf_data expf_consts;
  float32x4_t special_bound, inf_bound, cosh_9, nine;
} data = {
  .expf_consts = V_EXPF_DATA,
  /* 86.64: expf overflows above this, so have to use special case.  */
  .special_bound = V4 (0x1.5a92d8p+6),
  /* Value above which inf is returned.  */
  .inf_bound = V4 (0x1.65a9fap+6), /* ~ 89.42.  */
  .cosh_9 = V4 (0x1.fa715845p+11), /* cosh(9).  */
  .nine = V4 (0x1.2p+3),	   /* 9.0.  */
};

/* Uses the compound angle formula to adjust x back into an approximable range:
   cosh (A + B) = cosh(A)cosh(B) + sinh(A)sinh(B)
   By choosing sufficiently large values whereby after rounding cosh == sinh,
   this can be simplified into: cosh (A + B) = cosh(A) * e^B.  */
static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t t, uint32x4_t special)
{
  const struct data *d = ptr_barrier (&data);

  /* Complete fast path computation.  */
  /* Calculate cosh by exp(x) / 2 + exp(-x) / 2.  */
  float32x4_t half_t = vmulq_n_f32 (t, 0.5);
  float32x4_t half_over_t = vdivq_f32 (v_f32 (0.5), t);
  float32x4_t y = vaddq_f32 (half_t, half_over_t);

  /* Absolute x so we can subtract 9.0 without worrying about signing.  */
  float32x4_t ax = vabsq_f32 (x);
  /* Subtract 9.0 from x as a reduction to prevent early overflow.  */
  float32x4_t sx = vsubq_f32 (ax, d->nine);
  float32x4_t s = v_expf_inline (sx, &d->expf_consts);

  /* Multiply the result by cosh(9) slightly shifted for accuracy.  */
  float32x4_t r = vmulq_f32 (s, d->cosh_9);

  /* Check for overflowing lanes and return inf.  */
  uint32x4_t cmp = vcagtq_f32 (ax, d->inf_bound);

  /* Set overflowing lines to inf and set none over flowing to result.  */
  r = vbslq_f32 (cmp, v_f32 (INFINITY), r);

  /* Return r for special lanes and y for none special lanes.  */
  return vbslq_f32 (special, r, y);
}

/* Single-precision vector cosh, using vector expf.
   Maximum error is 2.38 ULP:
   _ZGVnN4v_coshf (0x1.e8001ep+1) got 0x1.6a491ep+4
				 want 0x1.6a4922p+4.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (cosh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  float32x4_t t = v_expf_inline (x, &d->expf_consts);

  /* Check for special cases.  */
  uint32x4_t special = vcageq_f32 (x, d->special_bound);
  /* Fall back to vectorised special case for any lanes which would cause
     expm1 to overflow.  */
  if (__glibc_unlikely (v_any_u32 (special)))
    return special_case (x, t, special);

  /* Complete fast path if no special lanes.  */
  /* Calculate cosh by exp(x) / 2 + exp(-x) / 2.  */
  float32x4_t half_t = vmulq_n_f32 (t, 0.5);
  float32x4_t half_over_t = vdivq_f32 (v_f32 (0.5), t);
  return vaddq_f32 (half_t, half_over_t);
}
libmvec_hidden_def (V_NAME_F1 (cosh))
HALF_WIDTH_ALIAS_F1 (cosh)
