/* Single-precision vector (AdvSIMD) pow function

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

#include "flt-32/math_config.h"
#include "powf_common.h"
#include "v_math.h"
#include "v_powf_inline.h"

/* Check if x is an integer.  */
static inline uint32x4_t
visint (float32x4_t x)
{
  return vceqq_f32 (vrndq_f32 (x), x);
}

/* Check if x is real not integer valued.  */
static inline uint32x4_t
visnotint (float32x4_t x)
{
  return vmvnq_u32 (vceqq_f32 (vrndq_f32 (x), x));
}

/* Check if x is an odd integer.  */
static inline uint32x4_t
visodd (float32x4_t x)
{
  float32x4_t y = vmulq_n_f32 (x, 0.5f);
  return visnotint (y);
}

/* A scalar subroutine used to fix main power special cases. Similar to the
   preamble of scalar powf except that we do not update ix and sign_bias. This
   is done in the preamble of the SVE powf.  */
static inline float
powf_specialcase (float x, float y)
{
  uint32_t ix = asuint (x);
  uint32_t iy = asuint (y);
  /* Either x or y is 0 or inf or nan.  */
  if (__glibc_unlikely (zeroinfnan (iy)))
    {
      if (2 * iy == 0)
	return __issignalingf (x) ? x + y : 1.0f;
      if (ix == 0x3f800000)
	return __issignalingf (y) ? x + y : 1.0f;
      if (2 * ix > 2u * 0x7f800000 || 2 * iy > 2u * 0x7f800000)
	return x + y;
      if (2 * ix == 2 * 0x3f800000)
	return 1.0f;
      if ((2 * ix < 2 * 0x3f800000) == !(iy & 0x80000000))
	return 0.0f; /* |x|<1 && y==inf or |x|>1 && y==-inf.  */
      return y * y;
    }
  if (__glibc_unlikely (zeroinfnan (ix)))
    {
      float x2 = x * x;
      if (ix & 0x80000000 && checkint (iy) == 1)
	x2 = -x2;
      return iy & 0x80000000 ? 1 / x2 : x2;
    }
  return x;
}

/* Special case function wrapper.  */
static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, float32x4_t ret, uint32x4_t cmp)
{
  return v_call2_f32 (powf_specialcase, x, y, ret, cmp);
}

/* Power implementation for x containing negative or subnormal lanes.  */
static inline float32x4_t
v_powf_x_is_neg_or_small (float32x4_t x, float32x4_t y, const struct data *d)
{
  uint32x4_t xisneg = vcltzq_f32 (x);
  uint32x4_t xsmall = vcaltq_f32 (x, v_f32 (0x1p-126f));

  /* Set sign_bias depending on sign of x and nature of y.  */
  uint32x4_t yisodd_xisneg = vandq_u32 (visodd (y), xisneg);

  /* Set variable to SignBias if x is negative and y is odd.  */
  uint32x4_t sign_bias = vandq_u32 (d->sign_bias, yisodd_xisneg);

  /* Normalize subnormals.  */
  float32x4_t a = vabsq_f32 (x);
  uint32x4_t ia_norm = vreinterpretq_u32_f32 (vmulq_f32 (a, d->norm));
  ia_norm = vsubq_u32 (ia_norm, d->subnormal_bias);
  a = vbslq_f32 (xsmall, vreinterpretq_f32_u32 (ia_norm), a);

  /* Evaluate exp (y * log(x)) using |x| and sign bias correction.  */
  float32x4_t ret = v_powf_core (a, y, sign_bias, d);

  /* Cases of finite y and finite negative x.  */
  uint32x4_t yint_or_xpos = vornq_u32 (visint (y), xisneg);
  return vbslq_f32 (yint_or_xpos, ret, d->nan);
}

/* Implementation of AdvSIMD powf.
   The theoretical maximum error is under 2.60 ULPs.
   Maximum measured error is 2.57 ULPs:
   V_NAME_F2 (pow) (0x1.031706p+0, 0x1.ce2ec2p+12)
     got 0x1.fff868p+127
    want 0x1.fff862p+127.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F2 (pow) (float32x4_t x, float32x4_t y)
{
  const struct data *d = ptr_barrier (&data);

  /* Special cases of x or y: zero, inf and nan.  */
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t iy = vreinterpretq_u32_f32 (y);
  uint32x4_t xspecial = v_zeroinfnan (d, ix);
  uint32x4_t yspecial = v_zeroinfnan (d, iy);
  uint32x4_t cmp = vorrq_u32 (xspecial, yspecial);

  /* Evaluate pow(x, y) for x containing negative or subnormal lanes.  */
  uint32x4_t x_is_neg_or_sub = vcltq_f32 (x, v_f32 (0x1p-126f));
  if (__glibc_unlikely (v_any_u32 (x_is_neg_or_sub)))
    {
      float32x4_t ret = v_powf_x_is_neg_or_small (x, y, d);
      if (__glibc_unlikely (v_any_u32 (cmp)))
	return special_case (x, y, ret, cmp);
      return ret;
    }

  /* Else evaluate pow(x, y) for normal and positive x only.
     Use the powrf helper routine.  */
  if (__glibc_unlikely (v_any_u32 (cmp)))
    return special_case (x, y, v_powrf_core (x, y, d), cmp);
  return v_powrf_core (x, y, d);
}
libmvec_hidden_def (V_NAME_F2 (pow))
HALF_WIDTH_ALIAS_F2(pow)
