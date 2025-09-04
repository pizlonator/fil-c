/* Double-precision vector (SVE) tanh function

   Copyright (C) 2024 Free Software Foundation, Inc.
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
  float64_t poly[11];
  float64_t inv_ln2, ln2_hi, ln2_lo, shift;
  uint64_t thresh, tiny_bound;
} data = {
  /* Generated using Remez, deg=12 in [-log(2)/2, log(2)/2].  */
  .poly = { 0x1p-1, 0x1.5555555555559p-3, 0x1.555555555554bp-5,
	    0x1.111111110f663p-7, 0x1.6c16c16c1b5f3p-10,
	    0x1.a01a01affa35dp-13, 0x1.a01a018b4ecbbp-16,
	    0x1.71ddf82db5bb4p-19, 0x1.27e517fc0d54bp-22,
	    0x1.af5eedae67435p-26, 0x1.1f143d060a28ap-29, },

  .inv_ln2 = 0x1.71547652b82fep0,
  .ln2_hi = -0x1.62e42fefa39efp-1,
  .ln2_lo = -0x1.abc9e3b39803fp-56,
  .shift = 0x1.8p52,

  .tiny_bound = 0x3e40000000000000, /* asuint64 (0x1p-27).  */
  /* asuint64(0x1.241bf835f9d5fp+4) - asuint64(tiny_bound).  */
  .thresh = 0x01f241bf835f9d5f,
};

static inline svfloat64_t
expm1_inline (svfloat64_t x, const svbool_t pg, const struct data *d)
{
  /* Helper routine for calculating exp(x) - 1. Vector port of the helper from
     the scalar variant of tanh.  */

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  svfloat64_t j
      = svsub_x (pg, svmla_x (pg, sv_f64 (d->shift), x, d->inv_ln2), d->shift);
  svint64_t i = svcvt_s64_x (pg, j);
  svfloat64_t f = svmla_x (pg, x, j, d->ln2_hi);
  f = svmla_x (pg, f, j, d->ln2_lo);

  /* Approximate expm1(f) using polynomial.  */
  svfloat64_t f2 = svmul_x (pg, f, f);
  svfloat64_t f4 = svmul_x (pg, f2, f2);
  svfloat64_t p = svmla_x (
      pg, f, f2,
      sv_estrin_10_f64_x (pg, f, f2, f4, svmul_x (pg, f4, f4), d->poly));

  /* t = 2 ^ i.  */
  svfloat64_t t = svscale_x (pg, sv_f64 (1), i);
  /* expm1(x) = p * t + (t - 1).  */
  return svmla_x (pg, svsub_x (pg, t, 1), p, t);
}

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  return sv_call_f64 (tanh, x, y, special);
}

/* SVE approximation for double-precision tanh(x), using a simplified
   version of expm1. The greatest observed error is 2.77 ULP:
   _ZGVsMxv_tanh(-0x1.c4a4ca0f9f3b7p-3) got -0x1.bd6a21a163627p-3
				       want -0x1.bd6a21a163624p-3.  */
svfloat64_t SV_NAME_D1 (tanh) (svfloat64_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t ia = svreinterpret_u64 (svabs_x (pg, x));

  /* Trigger special-cases for tiny, boring and infinity/NaN.  */
  svbool_t special = svcmpgt (pg, svsub_x (pg, ia, d->tiny_bound), d->thresh);

  svfloat64_t u = svadd_x (pg, x, x);

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  svfloat64_t q = expm1_inline (u, pg, d);
  svfloat64_t qp2 = svadd_x (pg, q, 2);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, svdiv_x (pg, q, qp2), special);
  return svdiv_x (pg, q, qp2);
}
