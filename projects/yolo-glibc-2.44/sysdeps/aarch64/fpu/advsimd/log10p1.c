/* Double-precision vector log10p1 function

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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
  float64x2_t c1, c2, c3, c4, c6, c8, c10, c12, c14, c16, c18, c20;
  double c5, c7, c9, c11, c13, c15, c17, c19;
  double inv_log2_10, inv_ln10, one, minus_one;
  int64x2_t one_top;
  uint64x2_t one_m_hf_rt2_top, umask, hf_rt2_top, bottom_mask;
  uint64x2_t inf, minf, nan;
} data = {
  /* Coefficients generated using FPMinimax deg=20, in
   [sqrt(2)/2-1, sqrt(2)-1].  */
  .c1 = V2 (0x1.bcb7b1526e50fp-2),
  .c2 = V2 (-0x1.bcb7b1526e4fep-3),
  .c3 = V2 (0x1.287a7636f3cfp-3),
  .c4 = V2 (-0x1.bcb7b152707cap-4),
  .c5 = 0x1.63c62775e6667p-4,
  .c6 = V2 (-0x1.287a76368cf37p-4),
  .c7 = 0x1.fc3fa57ed69cep-5,
  .c8 = V2 (-0x1.bcb7b0eed8335p-5),
  .c9 = 0x1.8b4e10d4ecd69p-5,
  .c10 = V2 (-0x1.63c65554f2db4p-5),
  .c11 = 0x1.436b003e5358p-5,
  .c12 = V2 (-0x1.2872d378b6363p-5),
  .c13 = 0x1.11e15dd8ac0efp-5,
  .c14 = V2 (-0x1.fd8dc08e6b21p-6),
  .c15 = 0x1.d6fabf7e5c622p-6,
  .c16 = V2 (-0x1.ad53855566e62p-6),
  .c17 = 0x1.a9547f6043884p-6,
  .c18 = V2 (-0x1.e4a167fcd3e22p-6),
  .c19 = 0x1.c2f6859a15a65p-6,
  .c20 = V2 (-0x1.91c6df82d809bp-7),
  .hf_rt2_top = V2 (0x3fe6a09e00000000),
  .one_m_hf_rt2_top = V2 (0x00095f6200000000),
  .umask = V2 (0x000fffff00000000),
  .one_top = V2 (0x3ff),
  .inv_ln10 = 0x1.bcb7b1526e50ep-2,
  .inv_log2_10 = 0x1.34413509f79ffp-2,
  .inf = V2 (0x7ff0000000000000),
  .minf = V2 (0xfff0000000000000),
  .nan = V2 (0x7fffffffffffffff),
  .bottom_mask = V2 (0xffffffff),
  .minus_one = -1.0f,
  .one = 1.0f,
};

static inline float64x2_t
special_case (const struct data *d, float64x2_t x, float64x2_t y,
	      uint64x2_t cmp)
{
  uint64x2_t ret_inf = vcgeq_f64 (x, vreinterpretq_f64_u64 (d->inf));
  uint64x2_t neg_val
      = vbslq_u64 (vcgeq_f64 (x, v_f64 (d->minus_one)), d->minf, d->nan);
  float64x2_t s = vreinterpretq_f64_u64 (vbslq_u64 (ret_inf, d->inf, neg_val));
  return vbslq_f64 (cmp, s, y);
}

/* Vector log10p1 approximation using polynomial on reduced interval.
   Worst-case error is 2.69 ULP:
   _ZGVnN2v_log10p1(-0x1.2582542cd267p-15) got -0x1.fde2ee0eb629p-17
					  want -0x1.fde2ee0eb628dp-17 .  */
VPCS_ATTR float64x2_t V_NAME_D1 (log10p1) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* Calculate scaling factor k.  */
  float64x2_t m = vaddq_f64 (x, v_f64 (d->one));
  uint64x2_t mi = vreinterpretq_u64_f64 (m);
  uint64x2_t u = vaddq_u64 (mi, d->one_m_hf_rt2_top);
  int64x2_t ki
      = vsubq_s64 (vreinterpretq_s64_u64 (vshrq_n_u64 (u, 52)), d->one_top);
  float64x2_t k = vcvtq_f64_s64 (ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt (2)].  */
  uint64x2_t utop = vaddq_u64 (vandq_u64 (u, d->umask), d->hf_rt2_top);
  uint64x2_t u_red = vorrq_u64 (utop, vandq_u64 (mi, d->bottom_mask));
  float64x2_t f
      = vaddq_f64 (vreinterpretq_f64_u64 (u_red), v_f64 (d->minus_one));
  /* Correction term c/m.  */
  float64x2_t cm
      = vdivq_f64 (vsubq_f64 (x, vaddq_f64 (m, v_f64 (d->minus_one))), m);

  /* Order-18 Pairwise Horner evaluation scheme.  */
  float64x2_t f2 = vmulq_f64 (f, f);
  float64x2_t c57 = vld1q_f64 (&d->c5);
  float64x2_t c911 = vld1q_f64 (&d->c9);
  float64x2_t c1315 = vld1q_f64 (&d->c13);
  float64x2_t c1719 = vld1q_f64 (&d->c17);
  float64x2_t p1819 = vfmaq_laneq_f64 (d->c18, f, c1719, 1);
  float64x2_t p1617 = vfmaq_laneq_f64 (d->c16, f, c1719, 0);
  float64x2_t p1415 = vfmaq_laneq_f64 (d->c14, f, c1315, 1);
  float64x2_t p1213 = vfmaq_laneq_f64 (d->c12, f, c1315, 0);
  float64x2_t p1011 = vfmaq_laneq_f64 (d->c10, f, c911, 1);
  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, f, c911, 0);

  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, f, c57, 1);
  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, f, c57, 0);
  float64x2_t p23 = vfmaq_f64 (d->c2, f, d->c3);
  float64x2_t p = vfmaq_f64 (p1819, f2, d->c20);
  p = vfmaq_f64 (p1617, f2, p);
  p = vfmaq_f64 (p1415, f2, p);
  p = vfmaq_f64 (p1213, f2, p);
  p = vfmaq_f64 (p1011, f2, p);
  p = vfmaq_f64 (p89, f2, p);
  p = vfmaq_f64 (p67, f2, p);
  p = vfmaq_f64 (p45, f2, p);
  p = vfmaq_f64 (p23, f2, p);
  p = vfmaq_f64 (d->c1, f, p);

  float64x2_t inv_log_consts = vld1q_f64 (&d->inv_log2_10);
  /* Assemble log10p1(x) = k/log2(10) + log10p1(f) + c/(m * ln10).  */
  float64x2_t y = vfmaq_laneq_f64 (vmulq_f64 (p, f), k, inv_log_consts, 0);
  y = vfmaq_laneq_f64 (y, cm, inv_log_consts, 1);

  /* Special cases: x == (-inf), x == nan, x <= -1 .  */
  uint64x2_t special
      = vorrq_u64 (vcleq_f64 (x, v_f64 (d->minus_one)),
		   vcgeq_f64 (x, vreinterpretq_f64_u64 (d->inf)));
  if (__glibc_unlikely (v_any_u64 (special)))
    return special_case (d, x, y, special);
  return y;
}
