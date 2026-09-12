/* Double-precision (Advanced SIMD) log2p1 function

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

  float64x2_t c2, c4, c6, c8, c10, c12, c14, c16, c18, c20;
  double c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
  uint64x2_t hf_rt2_top, one_m_hf_rt2_top, umask;
  int64x2_t one_top;
  double inv_ln2;
} data = {
  /* Coefficients generated using FPMinmax deg=20, in
   [sqrt(2)/2-1, sqrt(2)-1].  */
  .c1 = 0x1.71547652b82fep0,
  .c2 = V2 (-0x1.71547652b8303p-1),
  .c3 = 0x1.ec709dc39ff3bp-2,
  .c4 = V2 (-0x1.71547652b807fp-2),
  .c5 = 0x1.2776c50f6352cp-2,
  .c6 = V2 (-0x1.ec709dc417686p-3),
  .c7 = 0x1.a61762480fdcap-3,
  .c8 = V2 (-0x1.715475e0466fp-3),
  .c9 = 0x1.484b288053b76p-3,
  .c10 = V2 (-0x1.2776eb394443fp-3),
  .c11 = 0x1.0c98198fec4f9p-3,
  .c12 = V2 (-0x1.ec64788a5e48p-4),
  .c13 = 0x1.c6df9fa8ddd65p-4,
  .c14 = V2 (-0x1.a72110bd827d7p-4),
  .c15 = 0x1.875793fc4422dp-4,
  .c16 = V2 (-0x1.64f6f207a7e15p-4),
  .c17 = 0x1.60b98dabeba61p-4,
  .c18 = V2 (-0x1.90f4556c87c7fp-4),
  .c19 = 0x1.76554651dcda3p-4,
  .c20 = V2 (-0x1.4f96285ff7616p-5),
  .hf_rt2_top = V2 (0x3fe6a09e00000000),
  .one_m_hf_rt2_top = V2 (0x00095f6200000000),
  .umask = V2 (0x000fffff00000000),
  .one_top = V2 (0x3ff),
  .inv_ln2 = 0x1.71547652b82fep+0,
};
#define BottomMask v_u64 (0xffffffff)
static float64x2_t VPCS_ATTR
special_case (float64x2_t x, float64x2_t y, uint64x2_t cmp)
{

  uint64x2_t ret_inf = vcgeq_f64 (x, v_f64 (INFINITY));
  uint64x2_t neg_val
      = vbslq_u64 (vcgeq_f64 (x, v_f64 (-1)), v_u64 (0xfff0000000000000),
		   v_u64 (0x7fffffffffffffff));
  float64x2_t s = vreinterpretq_f64_u64 (
      vbslq_u64 (ret_inf, (v_u64 (0x7ff0000000000000)), neg_val));

  return vbslq_f64 (cmp, s, y);
}

/* Vector log2p1 approximation using polynomial on reduced interval.
   Worst-case error is 3.0 ULP:
   _ZGVnN2v_log2p1(0x1.07062df05d415p-23) got 0x1.7b76ae4a7f996p-23
					 want 0x1.7b76ae4a7f999p-23 .  */
VPCS_ATTR float64x2_t V_NAME_D1 (log2p1) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t m = vaddq_f64 (x, v_f64 (1.0));
  uint64x2_t mi = vreinterpretq_u64_f64 (m);
  uint64x2_t u = vaddq_u64 (mi, d->one_m_hf_rt2_top);

  int64x2_t ki
      = vsubq_s64 (vreinterpretq_s64_u64 (vshrq_n_u64 (u, 52)), d->one_top);
  float64x2_t k = vcvtq_f64_s64 (ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt (2)].  */
  uint64x2_t utop = vaddq_u64 (vandq_u64 (u, d->umask), d->hf_rt2_top);
  uint64x2_t u_red = vorrq_u64 (utop, vandq_u64 (mi, BottomMask));

  float64x2_t f = vsubq_f64 (vreinterpretq_f64_u64 (u_red), v_f64 (1.0));

  /* Correction term c/m.  */
  float64x2_t cm = vdivq_f64 (vsubq_f64 (x, vsubq_f64 (m, v_f64 (1.0))), m);

  float64x2_t f2 = vmulq_f64 (f, f);

  float64x2_t c13 = vld1q_f64 (&d->c1);
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
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, f, c13, 1);
  float64x2_t p = vfmaq_f64 (p1819, f2, d->c20);
  p = vfmaq_f64 (p1617, f2, p);
  p = vfmaq_f64 (p1415, f2, p);
  p = vfmaq_f64 (p1213, f2, p);
  p = vfmaq_f64 (p1011, f2, p);
  p = vfmaq_f64 (p89, f2, p);
  p = vfmaq_f64 (p67, f2, p);
  p = vfmaq_f64 (p45, f2, p);
  p = vfmaq_f64 (p23, f2, p);
  p = vfmaq_f64 (v_f64 (d->c1), f, p);

  /* Assemble log2p1(x) = k + log2p1(f) + c/(m * ln2).  */
  float64x2_t cm_ln2 = vmulq_f64 (cm, v_f64 (d->inv_ln2));
  float64x2_t y = vfmaq_f64 (k, p, f);
  y = vaddq_f64 (y, cm_ln2);

  uint64x2_t special_cases
      = vorrq_u64 (vcleq_f64 (x, v_f64 (-1)), vcgeq_f64 (x, v_f64 (INFINITY)));
  if (__glibc_unlikely (v_any_u64 (special_cases)))
    return special_case (x, y, special_cases);
  return y;
}
