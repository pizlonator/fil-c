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

#include "sv_math.h"

static const struct data
{
  double c2, c4, c6, c8, c10, c12, c14, c16, c18, c20;
  double inv_log2_10, inv_ln10;
  double c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
  uint64_t hf_rt2_top, one_m_hf_rt2_top, umask, bottom_mask;
  int64_t one_top;
} data = {
  /* Coefficients generated using FPMinimax deg=20, in
   [sqrt(2)/2-1, sqrt(2)-1].  */
  .c1 = 0x1.bcb7b1526e50fp-2,
  .c2 = -0x1.bcb7b1526e4fep-3,
  .c3 = 0x1.287a7636f3cfp-3,
  .c4 = -0x1.bcb7b152707cap-4,
  .c5 = 0x1.63c62775e6667p-4,
  .c6 = -0x1.287a76368cf37p-4,
  .c7 = 0x1.fc3fa57ed69cep-5,
  .c8 = -0x1.bcb7b0eed8335p-5,
  .c9 = 0x1.8b4e10d4ecd69p-5,
  .c10 = -0x1.63c65554f2db4p-5,
  .c11 = 0x1.436b003e5358p-5,
  .c12 = -0x1.2872d378b6363p-5,
  .c13 = 0x1.11e15dd8ac0efp-5,
  .c14 = -0x1.fd8dc08e6b21p-6,
  .c15 = 0x1.d6fabf7e5c622p-6,
  .c16 = -0x1.ad53855566e62p-6,
  .c17 = 0x1.a9547f6043884p-6,
  .c18 = -0x1.e4a167fcd3e22p-6,
  .c19 = 0x1.c2f6859a15a65p-6,
  .c20 = -0x1.91c6df82d809bp-7,
  .hf_rt2_top = 0x3fe6a09e00000000,
  .one_m_hf_rt2_top = 0x00095f6200000000,
  .one_top = 0x3ff,
  .inv_ln10 = 0x1.bcb7b1526e50ep-2,
  .inv_log2_10 = 0x1.34413509f79ffp-2,
  .umask = 0x000fffff00000000,
  .bottom_mask = 0x00000000ffffffff,
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

/* Vector log10p1 approximation using polynomial on reduced interval.
   Worst-case error is 2.81 ULP:
   _ZGVsMxv_log10p1(0x1.25c3f17d7602p-53) got 0x1.fe52a1624aad1p-55
					 want 0x1.fe52a1624aacep-55.  */
svfloat64_t SV_NAME_D1 (log10p1) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Calculate scaling factor k.  */
  svfloat64_t m = svsub_x (pg, x, -1.0);
  svuint64_t mi = svreinterpret_u64 (m);
  svuint64_t u = svadd_x (pg, mi, d->one_m_hf_rt2_top);

  svint64_t ki
      = svsub_x (pg, svreinterpret_s64 (svlsr_x (pg, u, 52)), d->one_top);
  svfloat64_t k = svcvt_f64_x (pg, ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  svuint64_t utop = svadd_x (pg, svand_x (pg, u, d->umask), d->hf_rt2_top);
  svuint64_t u_red = svorr_x (pg, utop, svand_x (pg, mi, d->bottom_mask));
  svfloat64_t f = svsub_x (svptrue_b64 (), svreinterpret_f64 (u_red), 1);

  /* Correction term c/m.  */
  svfloat64_t c = svsub_x (pg, x, svsub_x (svptrue_b64 (), m, 1));
  svfloat64_t cm = svdiv_x (pg, c, m);
  /* Order-18 Pairwise Horner evaluation scheme.  */
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

  /* Assemble log10p1(x) = k*log10(2) + log10p1(f) + c/(m * ln2).  */
  svfloat64_t inv_log_consts = svld1rq_f64 (svptrue_b64 (), &d->inv_log2_10);
  svfloat64_t y = svmla_lane_f64 (svmul_x (pg, p, f), k, inv_log_consts, 0);
  y = svmla_lane_f64 (y, cm, inv_log_consts, 1);
  /* Special cases: x == (-inf), x == nan, x <= -1 .  */
  svbool_t special = svorn_z (pg, svcmple (svptrue_b64 (), x, sv_f64 (-1.0)),
			      svcmplt (pg, x, sv_f64 (INFINITY)));
  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, y, special, pg);
  return y;
}
