/* Helper for double-precision SVE routines which depend on log1p

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

#ifndef AARCH64_FPU_SV_LOG1P_INLINE_H
#define AARCH64_FPU_SV_LOG1P_INLINE_H

#include "sv_math.h"

static const struct sv_log1p_data
{
  double c0, c2, c4, c6, c8, c10, c12, c14, c16;
  double c1, c3, c5, c7, c9, c11, c13, c15, c17, c18;
  double ln2_lo, ln2_hi;
  uint64_t hf_rt2_top;
  uint64_t one_m_hf_rt2_top;
  uint32_t bottom_mask;
  int64_t one_top;
} sv_log1p_data = {
  /* Coefficients generated using Remez, deg=20, in [sqrt(2)/2-1, sqrt(2)-1].
   */
  .c0 = -0x1.ffffffffffffbp-2,
  .c1 = 0x1.55555555551a9p-2,
  .c2 = -0x1.00000000008e3p-2,
  .c3 = 0x1.9999999a32797p-3,
  .c4 = -0x1.555555552fecfp-3,
  .c5 = 0x1.249248e071e5ap-3,
  .c6 = -0x1.ffffff8bf8482p-4,
  .c7 = 0x1.c71c8f07da57ap-4,
  .c8 = -0x1.9999ca4ccb617p-4,
  .c9 = 0x1.7459ad2e1dfa3p-4,
  .c10 = -0x1.554d2680a3ff2p-4,
  .c11 = 0x1.3b4c54d487455p-4,
  .c12 = -0x1.2548a9ffe80e6p-4,
  .c13 = 0x1.0f389a24b2e07p-4,
  .c14 = -0x1.eee4db15db335p-5,
  .c15 = 0x1.e95b494d4a5ddp-5,
  .c16 = -0x1.15fdf07cb7c73p-4,
  .c17 = 0x1.0310b70800fcfp-4,
  .c18 = -0x1.cfa7385bdb37ep-6,
  .ln2_lo = 0x1.62e42fefa3800p-1,
  .ln2_hi = 0x1.ef35793c76730p-45,
  /* top32(asuint64(sqrt(2)/2)) << 32.  */
  .hf_rt2_top = 0x3fe6a09e00000000,
  /* (top32(asuint64(1)) - top32(asuint64(sqrt(2)/2))) << 32.  */
  .one_m_hf_rt2_top = 0x00095f6200000000,
  .bottom_mask = 0xffffffff,
  .one_top = 0x3ff
};

static inline svfloat64_t
sv_log1p_inline (svfloat64_t x, const svbool_t pg)
{
  /* Helper for calculating log(x + 1). Adapted from v_log1p_inline.h, which
     differs from advsimd/log1p.c by:
     - No special-case handling - this should be dealt with by the caller.
     - Pairwise Horner polynomial evaluation for improved accuracy.
     - Optionally simulate the shortcut for k=0, used in the scalar routine,
       using svsel, for improved accuracy when the argument to log1p is close
     to 0. This feature is enabled by defining WANT_SV_LOG1P_K0_SHORTCUT as 1
     in the source of the caller before including this file.
     See sve/log1p.c for details of the algorithm.  */
  const struct sv_log1p_data *d = ptr_barrier (&sv_log1p_data);
  svfloat64_t m = svadd_x (pg, x, 1);
  svuint64_t mi = svreinterpret_u64 (m);
  svuint64_t u = svadd_x (pg, mi, d->one_m_hf_rt2_top);

  svint64_t ki
      = svsub_x (pg, svreinterpret_s64 (svlsr_x (pg, u, 52)), d->one_top);
  svfloat64_t k = svcvt_f64_x (pg, ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  svuint64_t utop
      = svadd_x (pg, svand_x (pg, u, 0x000fffff00000000), d->hf_rt2_top);
  svuint64_t u_red = svorr_x (pg, utop, svand_x (pg, mi, d->bottom_mask));
  svfloat64_t f = svsub_x (pg, svreinterpret_f64 (u_red), 1);

  /* Correction term c/m.  */
  svfloat64_t c = svsub_x (pg, x, svsub_x (pg, m, 1));
  svfloat64_t cm;

#ifndef WANT_SV_LOG1P_K0_SHORTCUT
#error                                                                       \
  "Cannot use sv_log1p_inline.h without specifying whether you need the k0 shortcut for greater accuracy close to 0"
#elif WANT_SV_LOG1P_K0_SHORTCUT
  /* Shortcut if k is 0 - set correction term to 0 and f to x. The result is
     that the approximation is solely the polynomial.  */
  svbool_t knot0 = svcmpne (pg, k, 0);
  cm = svdiv_z (knot0, c, m);
  if (__glibc_likely (!svptest_any (pg, knot0)))
    {
      f = svsel (knot0, f, x);
    }
#else
  /* No shortcut.  */
  cm = svdiv_x (pg, c, m);
#endif

  /* Approximate log1p(f) on the reduced input using a polynomial.  */
  svfloat64_t f2 = svmul_x (svptrue_b64 (), f, f),
	      f4 = svmul_x (svptrue_b64 (), f2, f2),
	      f8 = svmul_x (svptrue_b64 (), f4, f4),
	      f16 = svmul_x (svptrue_b64 (), f8, f8);

  svfloat64_t c13 = svld1rq (svptrue_b64 (), &d->c1);
  svfloat64_t c57 = svld1rq (svptrue_b64 (), &d->c5);
  svfloat64_t c911 = svld1rq (svptrue_b64 (), &d->c9);
  svfloat64_t c1315 = svld1rq (svptrue_b64 (), &d->c13);
  svfloat64_t c1718 = svld1rq (svptrue_b64 (), &d->c17);

  /* Order-18 Estrin scheme.  */
  svfloat64_t p01 = svmla_lane (sv_f64 (d->c0), f, c13, 0);
  svfloat64_t p23 = svmla_lane (sv_f64 (d->c2), f, c13, 1);
  svfloat64_t p45 = svmla_lane (sv_f64 (d->c4), f, c57, 0);
  svfloat64_t p67 = svmla_lane (sv_f64 (d->c6), f, c57, 1);

  svfloat64_t p03 = svmla_x (pg, p01, f2, p23);
  svfloat64_t p47 = svmla_x (pg, p45, f2, p67);
  svfloat64_t p07 = svmla_x (pg, p03, f4, p47);

  svfloat64_t p89 = svmla_lane (sv_f64 (d->c8), f, c911, 0);
  svfloat64_t p1011 = svmla_lane (sv_f64 (d->c10), f, c911, 1);
  svfloat64_t p1213 = svmla_lane (sv_f64 (d->c12), f, c1315, 0);
  svfloat64_t p1415 = svmla_lane (sv_f64 (d->c14), f, c1315, 1);

  svfloat64_t p811 = svmla_x (pg, p89, f2, p1011);
  svfloat64_t p1215 = svmla_x (pg, p1213, f2, p1415);
  svfloat64_t p815 = svmla_x (pg, p811, f4, p1215);

  svfloat64_t p015 = svmla_x (pg, p07, f8, p815);
  svfloat64_t p1617 = svmla_lane (sv_f64 (d->c16), f, c1718, 0);
  svfloat64_t p1618 = svmla_lane (p1617, f2, c1718, 1);
  svfloat64_t p = svmla_x (pg, p015, f16, p1618);

  /* Assemble log1p(x) = k * log2 + log1p(f) + c/m.  */
  svfloat64_t ln2_lo_hi = svld1rq (svptrue_b64 (), &d->ln2_lo);
  svfloat64_t ylo = svmla_lane (cm, k, ln2_lo_hi, 0);
  svfloat64_t yhi = svmla_lane (f, k, ln2_lo_hi, 1);

  return svmad_x (pg, p, f2, svadd_x (pg, ylo, yhi));
}
#endif
