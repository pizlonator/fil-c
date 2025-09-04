/* Single-precision AdvSIMD atan2

   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
#include "poly_advsimd_f32.h"

static const struct data
{
  float32x4_t poly[8];
  float32x4_t pi_over_2;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-128, 1.0].
     Generated using fpminimax between FLT_MIN and 1.  */
  .poly = { V4 (-0x1.55555p-2f), V4 (0x1.99935ep-3f), V4 (-0x1.24051ep-3f),
	    V4 (0x1.bd7368p-4f), V4 (-0x1.491f0ep-4f), V4 (0x1.93a2c0p-5f),
	    V4 (-0x1.4c3c60p-6f), V4 (0x1.01fd88p-8f) },
  .pi_over_2 = V4 (0x1.921fb6p+0f),
};

#define SignMask v_u32 (0x80000000)

/* Special cases i.e. 0, infinity and nan (fall back to scalar calls).  */
static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t y, float32x4_t x, float32x4_t ret, uint32x4_t cmp)
{
  return v_call2_f32 (atan2f, y, x, ret, cmp);
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline uint32x4_t
zeroinfnan (uint32x4_t i)
{
  /* 2 * i - 1 >= 2 * 0x7f800000lu - 1.  */
  return vcgeq_u32 (vsubq_u32 (vmulq_n_u32 (i, 2), v_u32 (1)),
		    v_u32 (2 * 0x7f800000lu - 1));
}

/* Fast implementation of vector atan2f. Maximum observed error is
   2.95 ULP in [0x1.9300d6p+6 0x1.93c0c6p+6] x [0x1.8c2dbp+6 0x1.8cea6p+6]:
   _ZGVnN4vv_atan2f (0x1.93836cp+6, 0x1.8cae1p+6) got 0x1.967f06p-1
						 want 0x1.967f00p-1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F2 (atan2) (float32x4_t y, float32x4_t x)
{
  const struct data *data_ptr = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t iy = vreinterpretq_u32_f32 (y);

  uint32x4_t special_cases = vorrq_u32 (zeroinfnan (ix), zeroinfnan (iy));

  uint32x4_t sign_x = vandq_u32 (ix, SignMask);
  uint32x4_t sign_y = vandq_u32 (iy, SignMask);
  uint32x4_t sign_xy = veorq_u32 (sign_x, sign_y);

  float32x4_t ax = vabsq_f32 (x);
  float32x4_t ay = vabsq_f32 (y);

  uint32x4_t pred_xlt0 = vcltzq_f32 (x);
  uint32x4_t pred_aygtax = vcgtq_f32 (ay, ax);

  /* Set up z for call to atanf.  */
  float32x4_t n = vbslq_f32 (pred_aygtax, vnegq_f32 (ax), ay);
  float32x4_t d = vbslq_f32 (pred_aygtax, ay, ax);
  float32x4_t z = vdivq_f32 (n, d);

  /* Work out the correct shift.  */
  float32x4_t shift = vreinterpretq_f32_u32 (
      vandq_u32 (pred_xlt0, vreinterpretq_u32_f32 (v_f32 (-2.0f))));
  shift = vbslq_f32 (pred_aygtax, vaddq_f32 (shift, v_f32 (1.0f)), shift);
  shift = vmulq_f32 (shift, data_ptr->pi_over_2);

  /* Calculate the polynomial approximation.
     Use 2-level Estrin scheme for P(z^2) with deg(P)=7. However,
     a standard implementation using z8 creates spurious underflow
     in the very last fma (when z^8 is small enough).
     Therefore, we split the last fma into a mul and an fma.
     Horner and single-level Estrin have higher errors that exceed
     threshold.  */
  float32x4_t z2 = vmulq_f32 (z, z);
  float32x4_t z4 = vmulq_f32 (z2, z2);

  float32x4_t ret = vfmaq_f32 (
      v_pairwise_poly_3_f32 (z2, z4, data_ptr->poly), z4,
      vmulq_f32 (z4, v_pairwise_poly_3_f32 (z2, z4, data_ptr->poly + 4)));

  /* y = shift + z * P(z^2).  */
  ret = vaddq_f32 (vfmaq_f32 (z, ret, vmulq_f32 (z2, z)), shift);

  /* Account for the sign of y.  */
  ret = vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (ret), sign_xy));

  if (__glibc_unlikely (v_any_u32 (special_cases)))
    {
      return special_case (y, x, ret, special_cases);
    }

  return ret;
}
libmvec_hidden_def (V_NAME_F2 (atan2))
HALF_WIDTH_ALIAS_F2(atan2)
