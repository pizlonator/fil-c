/* Single-precision vector (SVE) cosh function

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

#include "sv_math.h"
#include "sv_expf_inline.h"

/* For x < SpecialBound, the result of exp is subnormal and not handled
   correctly by FEXPA.  */
#define SpecialBound 0x1.5d5e2ap+6f /* ~ 87.34.  */

static const struct data
{
  struct sv_expf_data expf_consts;
  float32_t special_bound, cosh_9;
} data = {
  .expf_consts = SV_EXPF_DATA,
  .special_bound = SpecialBound,
  .cosh_9 = 0x1.fa715845p+11, /* cosh(9).  */
};

/* Uses the compound angle formula to adjust x back into an approximable range:
   cosh (A + B) = cosh(A)cosh(B) + sinh(A)sinh(B)
   By choosing sufficiently large values whereby after rounding cosh == sinh,
   this can be simplified into: cosh (A + B) = cosh(A) * e^B.  */
static inline svfloat32_t
special_case (svfloat32_t x, svbool_t special, svfloat32_t half_e,
	      svfloat32_t half_over_e, const struct data *d)
{
  /* Finish fastpass to compute values for non-special cases.  */
  svfloat32_t y = svadd_x (svptrue_b32 (), half_e, half_over_e);

  /* Make special values positive.  */
  svfloat32_t ax = svabs_x (svptrue_b32 (), x);

  /* The input `x` is reduced by an offset of 9.0 to allow for accurate
     approximation on the interval `x > SpecialBound ~ 87.34`.  */
  ax = svsub_x (svptrue_b32 (), ax, 9.0);
  svfloat32_t r = expf_inline (ax, svptrue_b32 (), &d->expf_consts);

  /* Multiply the result e by cosh(9) = exp(9)/2 for special lanes only.  */
  svfloat32_t coshf_sum = svmul_x (svptrue_b32 (), r, d->cosh_9);

  /* Check for overflow in exponential for special case lanes.  */
  svbool_t is_inf = svcmpge (special, ax, d->special_bound);

  /* Set overflowing lines to inf and set none over flowing to result.  */
  svfloat32_t special_y = svsel (is_inf, sv_f32 (INFINITY), coshf_sum);

  /* Return special_y for special lanes and y for none special lanes.  */
  return svsel (special, special_y, y);
}

/* Single-precision vector cosh, using vector expf.
   Maximum error is 2.55 +0.5 ULP:
   _ZGVsMxv_coshf(-0x1.5b40f4p+1) got 0x1.e47748p+2
				 want 0x1.e4774ep+2.  */
svfloat32_t SV_NAME_F1 (cosh) (svfloat32_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Calculate cosh by exp(x) / 2 + exp(-x) / 2.
     Note that x is passed to exp here, rather than |x|. This is to avoid using
     destructive unary ABS for better register usage. However it means the
     routine is not exactly symmetrical, as the exp helper is slightly less
     accurate in the negative range.  */
  svfloat32_t e = expf_inline (x, pg, &d->expf_consts);
  svfloat32_t half_e = svmul_x (svptrue_b32 (), e, 0.5);
  svfloat32_t half_over_e = svdivr_x (pg, e, 0.5);

  /* Check for special cases and fall back to vectorised special case for any
  lanes which would cause expf to overflow.  */
  svbool_t special = svacgt (pg, x, d->special_bound);
  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, special, half_e, half_over_e, d);

  /* Complete fast path if no special lanes.  */
  return svadd_x (svptrue_b32 (), half_e, half_over_e);
}
