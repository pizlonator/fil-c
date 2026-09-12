/* Double-precision SVE log1p

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

#include "sv_math.h"
#include "poly_sve_f64.h"

static const struct data
{
  float64_t c0, c2, c4, c6, c8, c10, c12, c14, c16;
  float64_t c1, c3, c5, c7, c9, c11, c13, c15, c17, c18;
  double ln2_hi, ln2_lo;
  uint64_t hfrt2_top, onemhfrt2_top, inf, mone;
} data = {
  /* Generated using Remez in [ sqrt(2)/2 - 1, sqrt(2) - 1]. Order 20
     polynomial, however first 2 coefficients are 0 and 1 so are not
     stored.  */
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
  .ln2_hi = 0x1.62e42fefa3800p-1,
  .ln2_lo = 0x1.ef35793c76730p-45,
  /* top32(asuint64(sqrt(2)/2)) << 32.  */
  .hfrt2_top = 0x3fe6a09e00000000,
  /* (top32(asuint64(1)) - top32(asuint64(sqrt(2)/2))) << 32.  */
  .onemhfrt2_top = 0x00095f6200000000,
  .inf = 0x7ff0000000000000,
  .mone = 0xbff0000000000000,
};

#define AbsMask 0x7fffffffffffffff
#define BottomMask 0xffffffff

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special, svbool_t pg)
{
  y = svsel (special, sv_f64 (NAN), y);
  svbool_t ret_pinf = svcmpeq (pg, x, INFINITY);
  svbool_t ret_minf = svcmpeq (pg, x, -1.0);
  y = svsel (ret_pinf, sv_f64 (INFINITY), y);
  return svsel (ret_minf, sv_f64 (-INFINITY), y);
}

/* Vector approximation for log1p using polynomial on reduced interval. Maximum
   observed error is 2.46 ULP:
   _ZGVsMxv_log1p(0x1.654a1307242a4p+11) got 0x1.fd5565fb590f4p+2
					want 0x1.fd5565fb590f6p+2.  */
svfloat64_t SV_NAME_D1 (log1p) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t ax = svand_x (pg, ix, AbsMask);
  svbool_t special
      = svorr_z (pg, svcmpge (pg, ax, d->inf), svcmpge (pg, ix, d->mone));

  /* With x + 1 = t * 2^k (where t = f + 1 and k is chosen such that f
			   is in [sqrt(2)/2, sqrt(2)]):
     log1p(x) = k*log(2) + log1p(f).

     f may not be representable exactly, so we need a correction term:
     let m = round(1 + x), c = (1 + x) - m.
     c << m: at very small x, log1p(x) ~ x, hence:
     log(1+x) - log(m) ~ c/m.

     We therefore calculate log1p(x) by k*log2 + log1p(f) + c/m.  */

  /* Obtain correctly scaled k by manipulation in the exponent.
     The scalar algorithm casts down to 32-bit at this point to calculate k and
     u_red. We stay in double-width to obtain f and k, using the same constants
     as the scalar algorithm but shifted left by 32.  */
  svfloat64_t m = svadd_x (pg, x, 1);
  svuint64_t mi = svreinterpret_u64 (m);
  svuint64_t u = svadd_x (pg, mi, d->onemhfrt2_top);

  svint64_t ki = svsub_x (pg, svreinterpret_s64 (svlsr_x (pg, u, 52)), 0x3ff);
  svfloat64_t k = svcvt_f64_x (pg, ki);

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  svuint64_t utop
      = svadd_x (pg, svand_x (pg, u, 0x000fffff00000000), d->hfrt2_top);
  svuint64_t u_red
      = svorr_x (pg, utop, svand_x (svptrue_b64 (), mi, BottomMask));
  svfloat64_t f = svsub_x (svptrue_b64 (), svreinterpret_f64 (u_red), 1);

  /* Correction term c/m.  */
  svfloat64_t cm = svdiv_x (pg, svsub_x (pg, x, svsub_x (pg, m, 1)), m);

  /* Approximate log1p(x) on the reduced input using a polynomial. Because
     log1p(0)=0 we choose an approximation of the form:
	x + C0*x^2 + C1*x^3 + C2x^4 + ...
     Hence approximation has the form f + f^2 * P(f)
     where P(x) = C0 + C1*x + C2x^2 + ...
     Assembling this all correctly is dealt with at the final step.  */
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

  svfloat64_t ylo = svmla_x (pg, cm, k, d->ln2_lo);
  svfloat64_t yhi = svmla_x (pg, f, k, d->ln2_hi);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (
	x, svmla_x (svptrue_b64 (), svadd_x (svptrue_b64 (), ylo, yhi), f2, p),
	special, pg);
  return svmla_x (svptrue_b64 (), svadd_x (svptrue_b64 (), ylo, yhi), f2, p);
}

strong_alias (SV_NAME_D1 (log1p), SV_NAME_D1 (logp1))
