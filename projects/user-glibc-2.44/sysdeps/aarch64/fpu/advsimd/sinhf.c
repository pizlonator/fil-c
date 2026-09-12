/* Single-precision vector (Advanced SIMD) sinh function

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
#include "v_expm1f_inline.h"

static const struct data
{
  struct v_expm1f_data expm1f_consts;
  float32x4_t special_bound, inf_bound, cosh_9, nine;
} data = {
  .expm1f_consts = V_EXPM1F_DATA,
  /* 88.38, above which expm1f helper overflows.  */
  .special_bound = V4 (0x1.61814ap+6),
  /* Value above which inf is returned.  */
  .inf_bound = V4 (0x1.65a9fap+6), /* ~ 89.42.  */
  .cosh_9 = V4 (0x1.fa715845p+11), /* cosh(9).  */
  .nine = V4 (0x1.2p+3),	   /* 9.0.  */
};

/* Uses the compound angle formula to adjust x back into an approximable range:
   sinh (A + B) = cosh(A)cosh(B) + sinh(A)sinh(B)
   By choosing sufficiently large values whereby after rounding sinh == cosh,
   this can be simplified into: sinh (A + B) = sinh(A) * e^B.  */
static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t t, float32x4_t halfsign,
	      uint32x4_t special)
{
  const struct data *d = ptr_barrier (&data);

  /* Complete fast path.  */
  t = vaddq_f32 (t, vdivq_f32 (t, vaddq_f32 (t, v_f32 (1.0))));
  float32x4_t y = vmulq_f32 (t, halfsign);

  float32x4_t ax = vabsq_f32 (x);
  uint32x4_t iax = vreinterpretq_u32_f32 (ax);

  /* Preserve sign for later use.  */
  uint32x4_t sign = veorq_u32 (vreinterpretq_u32_f32 (x), iax);

  /* Subtract 9.0 from x as a reduction to prevent early overflow.  */
  float32x4_t sx = vsubq_f32 (ax, d->nine);
  float32x4_t s = expm1f_inline (sx, &d->expm1f_consts);

  /* Multiply the result by cosh(9) slightly shifted for accuracy.  */
  float32x4_t r = vmulq_f32 (s, d->cosh_9);

  /* Check for overflowing lanes and return inf.  */
  uint32x4_t cmp = vcagtq_f32 (ax, d->inf_bound);

  /* Set overflowing lines to inf and set none over flowing to result.  */
  r = vbslq_f32 (cmp, v_f32 (INFINITY), r);

  /* Change sign back to original and return.  */
  r = vreinterpretq_f32_u32 (vorrq_u32 (sign, vreinterpretq_u32_f32 (r)));

  /* Return r for special lanes and y for none special lanes.  */
  return vbslq_f32 (special, r, y);
}

/* Approximation for vector single-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The maximum error is 2.26 ULP:
   _ZGVnN4v_sinhf (0x1.e34a9ep-4) got 0x1.e469ep-4
				 want 0x1.e469e4p-4.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F1 (sinh) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  float32x4_t ax = vabsq_f32 (x);
  float32x4_t halfsign = vreinterpretq_f32_u32 (
      vbslq_u32 (v_u32 (0x80000000), ix, vreinterpretq_u32_f32 (v_f32 (0.5))));

  /* Up to the point that expm1f overflows, we can use it to calculate sinhf
     using a slight rearrangement of the definition of asinh. This allows us
     to retain acceptable accuracy for very small inputs.  */
  float32x4_t t = expm1f_inline (ax, &d->expm1f_consts);

  /* Check for special cases.  */
  uint32x4_t special = vcageq_f32 (x, d->special_bound);

  /* Fall back to vectorised special case for any lanes which would cause
     expm1 to overflow.  */
  if (__glibc_unlikely (v_any_u32 (special)))
    return special_case (x, t, halfsign, special);

  /* Complete fast path if no special lanes.  */
  t = vaddq_f32 (t, vdivq_f32 (t, vaddq_f32 (t, v_f32 (1.0))));
  return vmulq_f32 (t, halfsign);
}
libmvec_hidden_def (V_NAME_F1 (sinh))
HALF_WIDTH_ALIAS_F1 (sinh)
