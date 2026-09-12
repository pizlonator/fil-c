/* Double-precision vector (AdvSIMD) exp2 function

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
  uint64x2_t off, sign_exp_mask, offset_lower_bound;
  uint64x2_t special_bound;
  uint32x4_t special_bound_u32;
  float64x2_t c0, c2;
  double c1, c3, invln2, c4;
  float64x2_t pinf, minf, nan;
} data = {
  /* Each coefficient was generated to approximate log(r) for |r| < 0x1.fp-9
     and N = 128, then scaled by log2(e) in extended precision and rounded back
     to double precision.  */
  .c0 = V2 (-0x1.71547652b8300p-1),
  .c1 = 0x1.ec709dc340953p-2,
  .c2 = V2 (-0x1.71547651c8f35p-2),
  .c3 = 0x1.2777ebe12dda5p-2,
  .c4 = -0x1.ec738d616fe26p-3,
  .invln2 = 0x1.71547652b82fep0,
  .off = V2 (0x3fe6900900000000),
  .sign_exp_mask = V2 (0xfff0000000000000),
  /* Lower bound is 0x0010000000000000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound - offset (which wraps around).  */
  .offset_lower_bound = V2 (0x0010000000000000 - 0x3fe6900900000000),
  .special_bound = V2 (0x7ffe000000000000),
  .special_bound_u32
  = V4 (0x7fe00000), /* asuint64(inf) - asuint64(0x1p-1022).  */
  .pinf = V2 (INFINITY),
  .minf = V2 (-INFINITY),
  .nan = V2 (NAN),
};

#define N (1 << V_LOG2_TABLE_BITS)
#define IndexMask (N - 1)

struct entry
{
  float64x2_t invc;
  float64x2_t log2c;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  uint64_t i0
      = (vgetq_lane_u64 (i, 0) >> (52 - V_LOG2_TABLE_BITS)) & IndexMask;
  uint64_t i1
      = (vgetq_lane_u64 (i, 1) >> (52 - V_LOG2_TABLE_BITS)) & IndexMask;
  float64x2_t e0 = vld1q_f64 (&__v_log2_data.table[i0].invc);
  float64x2_t e1 = vld1q_f64 (&__v_log2_data.table[i1].invc);
  e.invc = vuzp1q_f64 (e0, e1);
  e.log2c = vuzp2q_f64 (e0, e1);
  return e;
}

static inline float64x2_t VPCS_ATTR
inline_log2 (uint64x2_t u, uint64x2_t u_off, const struct data *d)
{
  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  int64x2_t k = vshrq_n_s64 (vreinterpretq_s64_u64 (u_off), 52);
  uint64x2_t iz = vsubq_u64 (u, vandq_u64 (u_off, d->sign_exp_mask));
  float64x2_t z = vreinterpretq_f64_u64 (iz);

  struct entry e = lookup (u_off);

  /* log2(x) = log1p(z/c-1)/log(2) + log2(c) + k.  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  float64x2_t kd = vcvtq_f64_s64 (k);

  float64x2_t invln2_and_c4 = vld1q_f64 (&d->invln2);
  float64x2_t hi
      = vfmaq_laneq_f64 (vaddq_f64 (e.log2c, kd), r, invln2_and_c4, 0);

  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t odd_coeffs = vld1q_f64 (&d->c1);
  float64x2_t y = vfmaq_laneq_f64 (d->c2, r, odd_coeffs, 1);
  float64x2_t p = vfmaq_laneq_f64 (d->c0, r, odd_coeffs, 0);
  y = vfmaq_laneq_f64 (y, r2, invln2_and_c4, 1);
  y = vfmaq_f64 (p, r2, y);

  return vfmaq_f64 (hi, y, r2);
}

static inline float64x2_t VPCS_ATTR
special_case (uint64x2_t u_off, const struct data *d)
{
  float64x2_t x = vreinterpretq_f64_u64 (vaddq_u64 (u_off, d->off));
  /* If x is special, compute 2log(sqrt(x)), else compute log(x).
     x might be subnormal, and sqrting it makes it larger.
     And the above two expressions are equivalent.  */
  uint64x2_t special
      = vcgeq_u64 (vsubq_u64 (u_off, d->offset_lower_bound), d->special_bound);
  float64x2_t x_sqrt = vbslq_f64 (special, vsqrtq_f64 (x), x);

  u_off = vsubq_u64 (vreinterpretq_u64_f64 (x_sqrt), d->off);

  /* Don't pass u into this, it isn't using x_sqrt.  */
  float64x2_t y = inline_log2 (vreinterpretq_u64_f64 (x_sqrt), u_off, d);

  y = vbslq_f64 (special, vmulq_f64 (y, v_f64 (2.0f)), y);

  /* Is true for +/- inf, +/- nan as well as all negative numbers.  */
  uint64x2_t is_infnan
      = vcgeq_u64 (vreinterpretq_u64_f64 (x), vreinterpretq_u64_f64 (d->pinf));
  uint64x2_t infnan_or_zero = vorrq_u64 (is_infnan, vceqzq_f64 (x));

  y = vbslq_f64 (infnan_or_zero, d->nan, y);
  uint64x2_t ret_pinf = vceqq_f64 (x, d->pinf);
  uint64x2_t ret_minf = vceqzq_f64 (x);
  y = vbslq_f64 (ret_pinf, d->pinf, y);
  y = vbslq_f64 (ret_minf, d->minf, y);
  return y;
}

/* Double-precision vector log2 routine. Implements the same algorithm as
   vector log10, with coefficients and table entries scaled in extended
   precision. The maximum observed error is 2.09 + 0.5 ULP:
   _ZGVnN2v_log2(0x1.0b556b093869bp+0) got 0x1.fffb34198d9dap-5
				      want 0x1.fffb34198d9ddp-5.  */
float64x2_t VPCS_ATTR V_NAME_D1 (log2) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* To avoid having to mov x out of the way, keep u after offset has been
     applied, and recover x by adding the offset back in the special-case
     handler.  */
  uint64x2_t u = vreinterpretq_u64_f64 (x);
  uint64x2_t u_off = vsubq_u64 (u, d->off);

  uint32x2_t special_u32 = vcge_u32 (vsubhn_u64 (u_off, d->offset_lower_bound),
				     vget_low_u32 (d->special_bound_u32));

  if (__glibc_unlikely (v_any_u32h (special_u32)))
    return special_case (u_off, d);
  return inline_log2 (u, u_off, d);
}
