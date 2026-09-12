/* Double-precision (SVE) log2p1 function

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

#include "sv_math.h"
static const struct data
{
  double c2, c4, c6, c8, c10, c12, c14, c16, c18, c20, inv_ln2;
  double c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
  uint64_t hf_rt2_top, one_m_hf_rt2_top;
  int64_t one_top;
} data = {
  /* Coefficients generated using FPMinmax deg=20, in
   [sqrt(2)/2-1, sqrt(2)-1].  */
  .c1 = 0x1.71547652b82fep0,
  .c2 = -0x1.71547652b8303p-1,
  .c3 = 0x1.ec709dc39ff3bp-2,
  .c4 = -0x1.71547652b807fp-2,
  .c5 = 0x1.2776c50f6352cp-2,
  .c6 = -0x1.ec709dc417686p-3,
  .c7 = 0x1.a61762480fdcap-3,
  .c8 = -0x1.715475e0466fp-3,
  .c9 = 0x1.484b288053b76p-3,
  .c10 = -0x1.2776eb394443fp-3,
  .c11 = 0x1.0c98198fec4f9p-3,
  .c12 = -0x1.ec64788a5e48p-4,
  .c13 = 0x1.c6df9fa8ddd65p-4,
  .c14 = -0x1.a72110bd827d7p-4,
  .c15 = 0x1.875793fc4422dp-4,
  .c16 = -0x1.64f6f207a7e15p-4,
  .c17 = 0x1.60b98dabeba61p-4,
  .c18 = -0x1.90f4556c87c7fp-4,
  .c19 = 0x1.76554651dcda3p-4,
  .c20 = -0x1.4f96285ff7616p-5,
  .hf_rt2_top = 0x3fe6a09e00000000,
  .one_m_hf_rt2_top = 0x00095f6200000000,
  .one_top = 0x3ff,
  .inv_ln2 = 0x1.71547652b82fep+0,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special, svbool_t pg)
{

  y = svsel (special, sv_f64 (NAN), y);
  svbool_t ret_pinf = svcmpeq_f64 (pg, x, sv_f64 (INFINITY));
  svbool_t ret_minf = svcmpeq_f64 (pg, x, sv_f64 (-1.0));
  y = svsel (ret_pinf, sv_f64 (INFINITY), y);
  return svsel (ret_minf, sv_f64 (-INFINITY), y);
}

/* Vector log2p1 approximation using polynomial on reduced interval.
   Worst-case error is 3.0 ULP:
   _ZGVsMxv_log2p1(0x1.62e029c6f784fp-18) got 0x1.fff9d9148a06fp-18
					 want 0x1.fff9d9148a072p-18 .  */
svfloat64_t SV_NAME_D1 (log2p1) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat64_t m = svadd_x (pg, x, 1);
  svuint64_t mi = svreinterpret_u64 (m);
  svuint64_t u = svadd_x (pg, mi, d->one_m_hf_rt2_top);

  svint64_t ki
      = svsub_x (pg, svreinterpret_s64 (svlsr_x (pg, u, 52)), d->one_top);
  svfloat64_t k = svcvt_f64_x (pg, ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  svuint64_t utop
      = svadd_x (pg, svand_x (pg, u, 0x000fffff00000000), d->hf_rt2_top);
  svuint64_t u_red = svorr_x (pg, utop, svand_x (pg, mi, 0x00000000ffffffff));
  svfloat64_t f = svsub_x (svptrue_b64 (), svreinterpret_f64 (u_red), 1);

  /* Correction term c/m.  */
  svfloat64_t c = svsub_x (svptrue_b64 (), x, svsub_x (svptrue_b64 (), m, 1));
  svfloat64_t cm;
  cm = svdiv_x (pg, c, m);

  svfloat64_t f2 = svmul_x (svptrue_b64 (), f, f);

  svfloat64_t c13 = svld1rq (svptrue_b64 (), &d->c1);
  svfloat64_t c57 = svld1rq (svptrue_b64 (), &d->c5);
  svfloat64_t c911 = svld1rq (svptrue_b64 (), &d->c9);
  svfloat64_t c1315 = svld1rq (svptrue_b64 (), &d->c13);
  svfloat64_t c1719 = svld1rq (svptrue_b64 (), &d->c17);
  svfloat64_t c20_inv_ln2 = svld1rq (svptrue_b64 (), &d->c20);

  svfloat64_t p1819 = svmla_lane_f64 (sv_f64 (d->c18), f, c1719, 1);
  svfloat64_t p1617 = svmla_lane_f64 (sv_f64 (d->c16), f, c1719, 0);
  svfloat64_t p1415 = svmla_lane_f64 (sv_f64 (d->c14), f, c1315, 1);
  svfloat64_t p1213 = svmla_lane_f64 (sv_f64 (d->c12), f, c1315, 0);
  svfloat64_t p1011 = svmla_lane_f64 (sv_f64 (d->c10), f, c911, 1);
  svfloat64_t p89 = svmla_lane_f64 (sv_f64 (d->c8), f, c911, 0);
  svfloat64_t p67 = svmla_lane_f64 (sv_f64 (d->c6), f, c57, 1);
  svfloat64_t p45 = svmla_lane_f64 (sv_f64 (d->c4), f, c57, 0);
  svfloat64_t p23 = svmla_lane_f64 (sv_f64 (d->c2), f, c13, 1);
  svfloat64_t p = svmla_lane_f64 (p1819, f2, c20_inv_ln2, 0);
  p = svmla_x (pg, p1617, f2, p);
  p = svmla_x (pg, p1415, f2, p);
  p = svmla_x (pg, p1213, f2, p);
  p = svmla_x (pg, p1011, f2, p);
  p = svmla_x (pg, p89, f2, p);
  p = svmla_x (pg, p67, f2, p);
  p = svmla_x (pg, p45, f2, p);
  p = svmla_x (pg, p23, f2, p);
  p = svmla_x (pg, sv_f64 (d->c1), f, p);

  /* Assemble log2p1(x) = k + log2p1(f) + c/(m * ln2).  */
  svfloat64_t cm_k_ln2 = svmla_lane_f64 (k, cm, c20_inv_ln2, 1);
  svbool_t special_cases
      = svorn_z (pg, svcmple (svptrue_b64 (), x, sv_f64 (-1.0)),
		 svcmplt (pg, x, sv_f64 (INFINITY)));
  if (__glibc_unlikely (svptest_any (pg, special_cases)))
    return special_case (x, svmla_x (pg, cm_k_ln2, p, f), special_cases, pg);
  return svmla_x (pg, cm_k_ln2, p, f);
}
