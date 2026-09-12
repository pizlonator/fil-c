/* Helper for AdvSIMD single-precision pow

   Copyright (C) 2025 Free Software Foundation, Inc.
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

/* Define data structure and some routines specific to positive x.  */
#include "v_powrf_inline.h"

static inline float64x2_t
exp2_core_with_sbias (const struct data *d, float64x2_t ylogx,
		      uint64x2_t sign_bias)
{
  /* N*x = k + r with r in [-1/2, 1/2].  */
  float64x2_t kd = vrndnq_f64 (ylogx);
  int64x2_t ki = vcvtaq_s64_f64 (ylogx);
  float64x2_t r = vsubq_f64 (ylogx, kd);

  /* exp2(x) = 2^(k/N) * 2^r ~= s * (C0*r^3 + C1*r^2 + C2*r + 1).  */
  uint64x2_t t = vcombine_u64 (exp2_lookup (d, vgetq_lane_s64 (ki, 0)),
			       exp2_lookup (d, vgetq_lane_s64 (ki, 1)));
  int64x2_t ski = vaddq_s64 (ki, vreinterpretq_s64_u64 (sign_bias));
  t = vaddq_u64 (t, vreinterpretq_u64_s64 (
			vshlq_n_s64 (ski, 52 - V_POWF_EXP2_TABLE_BITS)));
  float64x2_t s = vreinterpretq_f64_u64 (t);
  float64x2_t p = vfmaq_f64 (d->exp2_poly[1], r, d->exp2_poly[0]);
  p = vfmaq_f64 (d->exp2_poly[2], r, p);
  p = vfmaq_f64 (s, p, vmulq_f64 (s, r));
  return p;
}

static inline float32x4_t
powf_core (const struct data *d, float32x4_t *ylogx, uint32x4_t tmp,
	   float32x4_t iz, float32x4_t y, int32x4_t k, uint32x4_t sign_bias)
{
  /* Use double precision for each lane: split input vectors into lo and hi
     halves and promote.  */
  float64x2_t tab0 = log2_lookup (d, vgetq_lane_u32 (tmp, 0)),
	      tab1 = log2_lookup (d, vgetq_lane_u32 (tmp, 1)),
	      tab2 = log2_lookup (d, vgetq_lane_u32 (tmp, 2)),
	      tab3 = log2_lookup (d, vgetq_lane_u32 (tmp, 3));

  float64x2_t iz_lo = vcvt_f64_f32 (vget_low_f32 (iz)),
	      iz_hi = vcvt_high_f64_f32 (iz);

  float64x2_t k_lo = vcvtq_f64_s64 (vmovl_s32 (vget_low_s32 (k))),
	      k_hi = vcvtq_f64_s64 (vmovl_high_s32 (k));

  float64x2_t invc_lo = vzip1q_f64 (tab0, tab1),
	      invc_hi = vzip1q_f64 (tab2, tab3),
	      logc_lo = vzip2q_f64 (tab0, tab1),
	      logc_hi = vzip2q_f64 (tab2, tab3);

  float64x2_t y_lo = vcvt_f64_f32 (vget_low_f32 (y)),
	      y_hi = vcvt_high_f64_f32 (y);

  float64x2_t ylogx_lo = ylogx_core (d, iz_lo, k_lo, invc_lo, logc_lo, y_lo);
  float64x2_t ylogx_hi = ylogx_core (d, iz_hi, k_hi, invc_hi, logc_hi, y_hi);

  uint64x2_t sign_bias_lo = vmovl_u32 (vget_low_u32 (sign_bias));
  uint64x2_t sign_bias_hi = vmovl_high_u32 (sign_bias);

  float32x2_t p_lo
      = vcvt_f32_f64 (exp2_core_with_sbias (d, ylogx_lo, sign_bias_lo));
  float32x2_t p_hi
      = vcvt_f32_f64 (exp2_core_with_sbias (d, ylogx_hi, sign_bias_hi));

  *ylogx = vcombine_f32 (vcvt_f32_f64 (ylogx_lo), vcvt_f32_f64 (ylogx_hi));

  return vcombine_f32 (p_lo, p_hi);
}

/* Power implementation without assumptions on x or y.
   Evaluate powr(|x|) = exp (y * log(|x|)), and handle
   sign of x using the sign bias.
   Handle underflow and overflow in exponential.  */
static inline float32x4_t
v_powf_core (float32x4_t x, float32x4_t y, uint32x4_t sign_bias,
	     const struct data *d)
{
  uint32x4_t ix = vreinterpretq_u32_f32 (x);

  /* Part of core computation carried in working precision.  */
  uint32x4_t tmp = vsubq_u32 (ix, d->off);
  uint32x4_t top = vbicq_u32 (tmp, v_u32 (MantissaMask));
  float32x4_t iz = vreinterpretq_f32_u32 (vsubq_u32 (ix, top));
  int32x4_t k
      = vshrq_n_s32 (vreinterpretq_s32_u32 (top),
		     23 - V_POWF_EXP2_TABLE_BITS); /* arithmetic shift.  */

  /* Compute core in extended precision and return intermediate ylogx results
     to handle cases of underflow and overflow in exp.  */
  float32x4_t ylogx;
  float32x4_t ret = powf_core (d, &ylogx, tmp, iz, y, k, sign_bias);

  /* Handle exp special cases of underflow and overflow.  */
  uint32x4_t sign = vshlq_n_u32 (sign_bias, 20 - V_POWF_EXP2_TABLE_BITS);
  float32x4_t ret_oflow = vreinterpretq_f32_u32 (vorrq_u32 (sign, d->inf));
  float32x4_t ret_uflow = vreinterpretq_f32_u32 (sign);
  ret = vbslq_f32 (vcleq_f32 (ylogx, d->uflow_bound), ret_uflow, ret);
  ret = vbslq_f32 (vcgtq_f32 (ylogx, d->oflow_bound), ret_oflow, ret);
  return ret;
}
