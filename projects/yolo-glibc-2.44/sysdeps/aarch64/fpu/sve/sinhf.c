/* Single-precision vector (SVE) sinh function

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

#include "sv_expm1f_inline.h"
#include "sv_expf_inline.h"
#include "sv_math.h"

static const struct data
{
  struct sv_expm1f_data expm1f_consts;
  struct sv_expf_data expf_consts;
  float32_t special_bound, cosh_9;
  uint32_t halff;
} data = {
  .expm1f_consts = SV_EXPM1F_DATA,
  .expf_consts = SV_EXPF_DATA,
  .halff = 0x3f000000,
  /* ~ 88.37 above which expm1f helper overflows.  */
  .special_bound = 0x1.61814ap+6,
  .cosh_9 = 0x1.fa715845p+11, /* cosh(9) ~ 4051.54.  */
};

/* Uses the compound angle formula to adjust x back into an approximable range:
   sinh (A + B) = cosh(A)cosh(B) + sinh(A)sinh(B)
   By choosing sufficiently large values whereby after rounding sinh == cosh,
   this can be simplified into: sinh (A + B) = sinh(A) * e^B.  */
static inline svfloat32_t
special_case (const svbool_t pg, svbool_t special, svfloat32_t ax,
	      svfloat32_t x, svfloat32_t t, const struct data *d)
{
  /* Preserve the sign bit to return final calcualtion to correct sign.  */
  svuint32_t sign
      = sveor_x (pg, svreinterpret_u32 (x), svreinterpret_u32 (ax));

  /* Finish fastpass to compute values for non-special cases.  */
  svfloat32_t halfsign = svreinterpret_f32 (svorr_x (pg, sign, d->halff));
  t = svadd_x (pg, t, svdiv_x (pg, t, svadd_x (pg, t, 1.0)));
  svfloat32_t y = svmul_x (svptrue_b32 (), t, halfsign);

  /* The input `x` is reduced by an offset of 9.0 to allow for accurate
     approximation on the interval x > SpecialBound ~ 88.37.  */
  ax = svsub_x (svptrue_b32 (), ax, 9.0);
  svfloat32_t e = expf_inline (ax, svptrue_b32 (), &d->expf_consts);

  /* Multiply the result e by cosh(9) = exp(9)/2 for special lanes only.  */
  svfloat32_t sinhf_sum = svmul_x (svptrue_b32 (), e, d->cosh_9);

  /* Check for overflow in exponential for special case lanes.  */
  svbool_t is_inf = svcmpge (special, ax, d->special_bound);

  /* Set overflowing lines to inf and set none over flowing to result.  */
  svfloat32_t special_y = svsel (is_inf, sv_f32 (INFINITY), sinhf_sum);

  /* Change sign back to original and return.  */
  special_y = svreinterpret_f32 (
      svorr_x (svptrue_b32 (), sign, svreinterpret_u32 (special_y)));

  /* Return special_y for special lanes and y for none special lanes.  */
  return svsel (special, special_y, y);
}

/* Approximation for SVE single-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   Maximum error is 2.76 +0.5 ULP:
   _ZGVsMxv_sinhf (0x1.6587e8p+6) got 0x1.ef3f98p+127
				 want 0x1.ef3f92p+127.  */
svfloat32_t SV_NAME_F1 (sinh) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Use absolute number for calculations for accuracy.  */
  svfloat32_t ax = svabs_x (pg, x);

  /* Up to the point that expm1f overflows, we can use it to calculate sinhf
     using a slight rearrangement of the definition of asinh. This allows us to
     retain acceptable accuracy for very small inputs.  */
  svfloat32_t t = expm1f_inline (ax, pg, &d->expm1f_consts);

  /* Check for special cases and fall back to vectorised special case for any
     lanes which would cause expm1f to overflow.  */
  svbool_t special = svacge (pg, x, d->special_bound);
  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (pg, special, ax, x, t, d);

  /* Preserve the sign bit to return final calcualtion to correct sign.  */
  svuint32_t sign
      = sveor_x (pg, svreinterpret_u32 (x), svreinterpret_u32 (ax));
  svfloat32_t halfsign = svreinterpret_f32 (svorr_x (pg, sign, d->halff));
  /* Complete fast path if no special lanes.  */
  t = svadd_x (pg, t, svdiv_x (pg, t, svadd_x (pg, t, 1.0)));
  return svmul_x (svptrue_b32 (), t, halfsign);
}
