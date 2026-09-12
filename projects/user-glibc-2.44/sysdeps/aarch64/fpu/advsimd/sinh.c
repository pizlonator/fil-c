/* Double-precision vector (Advanced SIMD) sinh function

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
#include "v_expm1_inline.h"

static const struct data
{
  struct v_expm1_data d;
  uint64x2_t halff;
  float64x2_t special_bound;
  float64x2_t inf_bound, cosh_9;
} data = {
  .d = V_EXPM1_DATA,
  .halff = V2 (0x3fe0000000000000),
  /* ln(2^1023). expm1 helper overflows for large input.  */
  .special_bound = V2 (0x1.628b76e3a7b61p+9), /* 709.09.  */
  /* Bound past which function returns inf.  */
  .inf_bound = V2 (0x1.634p+9), /* 710.5.  */
  /* Cosh(9) slightly shifted for accuracy.  */
  .cosh_9 = V2 (0x1.fa7157c470f82p+11),
};

/* Uses the compound angle formula to adjust x back into an approximable range:
   sinh (A + B) = cosh(A)cosh(B) + sinh(A)sinh(B)
   By choosing sufficiently large values whereby after rounding sinh == cosh,
   this can be simplified into: sinh (A + B) = sinh(A) * e^B.  */
static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t t, float64x2_t halfsign,
	      uint64x2_t special)
{
  const struct data *d = ptr_barrier (&data);

  /* Complete fast path.  */
  t = vaddq_f64 (t, vdivq_f64 (t, vaddq_f64 (t, v_f64 (1.0))));
  float64x2_t y = vmulq_f64 (t, halfsign);

  float64x2_t ax = vabsq_f64 (x);

  /* Preserve sign for later use.  */
  uint64x2_t sign
      = veorq_u64 (vreinterpretq_u64_f64 (x), vreinterpretq_u64_f64 (ax));

  /* Subtract 9.0 from x as a reduction to prevent early overflow.  */
  float64x2_t sx = vsubq_f64 (ax, v_f64 (9.0));
  float64x2_t s = expm1_inline (sx, &d->d);

  /* Multiply the result by cosh(9) slightly shifted for accuracy.  */
  float64x2_t r = vmulq_f64 (s, d->cosh_9);

  /* Check for overflowing lanes and set to inf.  */
  uint64x2_t cmp = vcagtq_f64 (ax, d->inf_bound);

  /* Set overflowing lines to inf and set none over flowing to result.  */
  r = vbslq_f64 (cmp, v_f64 (INFINITY), r);

  /* Change sign back to original and return.  */
  r = vreinterpretq_f64_u64 (vorrq_u64 (sign, vreinterpretq_u64_f64 (r)));

  /* Return r for special lanes and y for none special lanes.  */
  return vbslq_f64 (special, r, y);
}

/* Approximation for vector double-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The greatest observed error is 2.52 ULP:
   _ZGVnN2v_sinh(0x1.9f6ff2ab6fb19p-2) got 0x1.aaed83a3153ccp-2
				      want 0x1.aaed83a3153c9p-2.  */
float64x2_t VPCS_ATTR V_NAME_D1 (sinh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t ax = vabsq_f64 (x);
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  float64x2_t halfsign = vreinterpretq_f64_u64 (
      vbslq_u64 (v_u64 (0x8000000000000000), ix, d->halff));

  /* Up to the point that expm1 overflows, we can use it to calculate sinh
     using a slight rearrangement of the definition of sinh. This allows us to
     retain acceptable accuracy for very small inputs.  */
  float64x2_t t = expm1_inline (ax, &d->d);

  /* Check for special cases.  */
  uint64x2_t special = vcageq_f64 (x, d->special_bound);
  /* Fall back to vectorised special case for any lanes which would cause
     expm1 to overflow.  */
  if (__glibc_unlikely (v_any_u64 (special)))
    return special_case (x, t, halfsign, special);

  /* Complete fast path if no special lanes.  */
  t = vaddq_f64 (t, vdivq_f64 (t, vaddq_f64 (t, v_f64 (1.0))));
  return vmulq_f64 (t, halfsign);
}
