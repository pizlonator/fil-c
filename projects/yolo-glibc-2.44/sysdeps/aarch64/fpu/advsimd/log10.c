/* Double-precision vector (AdvSIMD) log10 function

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
  uint64x2_t off, offset_lower_bound;
  uint32x4_t special_bound_u32;
  uint64x2_t sign_exp_mask, special_bound;
  double invln10, log10_2;
  double c1, c3;
  float64x2_t c0, c2, c4;
  float64x2_t pinf, minf, nan;
} data = {
  /* Computed from log coefficients divided by log(10) then rounded to double
     precision.  */
  .c0 = V2 (-0x1.bcb7b1526e506p-3),
  .c1 = 0x1.287a7636be1d1p-3,
  .c2 = V2 (-0x1.bcb7b158af938p-4),
  .c3 = 0x1.63c78734e6d07p-4,
  .c4 = V2 (-0x1.287461742fee4p-4),
  .invln10 = 0x1.bcb7b1526e50ep-2,
  .log10_2 = 0x1.34413509f79ffp-2,
  .off = V2 (0x3fe6900900000000),
  /* Lower bound is 0x0010000000000000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound - offset (which wraps around).  */
  .offset_lower_bound = V2 (0x0010000000000000 - 0x3fe6900900000000),
  .special_bound_u32
  = V4 (0x7fe00000), /* asuint64(inf) - 0x0010000000000000.  */
  .sign_exp_mask = V2 (0xfff0000000000000),
  .special_bound = V2 (0x7ffe000000000000),
  .pinf = V2 (INFINITY),
  .minf = V2 (-INFINITY),
  .nan = V2 (NAN),
};

#define N (1 << V_LOG10_TABLE_BITS)
#define IndexMask (N - 1)

struct entry
{
  float64x2_t invc;
  float64x2_t log10c;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  uint64_t i0
      = (vgetq_lane_u64 (i, 0) >> (52 - V_LOG10_TABLE_BITS)) & IndexMask;
  uint64_t i1
      = (vgetq_lane_u64 (i, 1) >> (52 - V_LOG10_TABLE_BITS)) & IndexMask;
  float64x2_t e0 = vld1q_f64 (&__v_log10_data.table[i0].invc);
  float64x2_t e1 = vld1q_f64 (&__v_log10_data.table[i1].invc);
  e.invc = vuzp1q_f64 (e0, e1);
  e.log10c = vuzp2q_f64 (e0, e1);
  return e;
}

static float64x2_t VPCS_ATTR NOINLINE
log10_core (uint64x2_t u, uint64x2_t u_off, const struct data *d)
{
  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  int64x2_t k = vshrq_n_s64 (vreinterpretq_s64_u64 (u_off), 52);
  uint64x2_t iz = vsubq_u64 (u, vandq_u64 (u_off, d->sign_exp_mask));
  float64x2_t z = vreinterpretq_f64_u64 (iz);

  struct entry e = lookup (u_off);

  /* log10(x) = log1p(z/c-1)/log(10) + log10(c) + k*log10(2).  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  float64x2_t kd = vcvtq_f64_s64 (k);

  /* hi = r / log(10) + log10(c) + k*log10(2).
     Constants in v_log10_data.c are computed (in extended precision) as
     e.log10c := e.logc * invln10.  */
  float64x2_t cte = vld1q_f64 (&d->invln10);
  float64x2_t hi = vfmaq_laneq_f64 (e.log10c, r, cte, 0);

  /* y = log10(1+r) + n * log10(2).  */
  hi = vfmaq_laneq_f64 (hi, kd, cte, 1);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t odd_coeffs = vld1q_f64 (&d->c1);
  float64x2_t y = vfmaq_laneq_f64 (d->c2, r, odd_coeffs, 1);
  float64x2_t p = vfmaq_laneq_f64 (d->c0, r, odd_coeffs, 0);
  y = vfmaq_f64 (y, d->c4, r2);
  y = vfmaq_f64 (p, y, r2);
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
  float64x2_t y = log10_core (vreinterpretq_u64_f64 (x_sqrt), u_off, d);

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

/* Fast implementation of double-precision vector log10
   is a slight modification of double-precision vector log.
   Max ULP error: < 2.5 ulp (nearest rounding.)
   Maximum measured at 2.46 ulp for x in [0.96, 0.97]
   _ZGVnN2v_log10(0x1.13192407fcb46p+0) got 0x1.fff6be3cae4bbp-6
				       want 0x1.fff6be3cae4b9p-6.  */
float64x2_t VPCS_ATTR V_NAME_D1 (log10) (float64x2_t x)
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
  /* Making what would usually be an inline function into NOINLINE helps
     performance. This is because the register allocation in the fast pass does
     not consider what registers are used in the special case function.  */
  return log10_core (u, u_off, d);
}
