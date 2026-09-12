/* Single-precision vector (SVE) exp2 function

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

/* For x < -SpecialBound, the result is subnormal and not handled
   correctly by FEXPA.  */
#define SpecialBound 0x1.f8p6f /* log2(2^126) = 126.0f.  */

/* Values of x over which exp overflows or underflows.  */
#define InfBound 0x1.0p7f /* 128.0f.  */
#define ZeroBound -0x1.2a8p7f /* -149.0f.  */

static const struct data
{
  float c0, c1, shift, special_bound;
  float inf_bound, zero_bound;
} data = {
  /* Coefficients generated using Remez algorithm with minimisation of relative
     error.  */
  .c0 = 0x1.62e485p-1,
  .c1 = 0x1.ebfbe0p-3,
  /* 1.5*2^17 + 127.  */
  .shift = 0x1.803f8p17f,
  .special_bound = SpecialBound,
  .inf_bound = InfBound,
  .zero_bound = ZeroBound,
};

static inline svfloat32_t
sv_exp2f_inline (svfloat32_t x, const svbool_t pg, const struct data *d)
{
  /* exp2(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = n + r, with r in [-1/2, 1/2].  */
  svfloat32_t z = svadd_x (svptrue_b32 (), x, d->shift);
  svfloat32_t n = svsub_x (svptrue_b32 (), z, d->shift);
  svfloat32_t r = svsub_x (svptrue_b32 (), x, n);

  svfloat32_t scale = svexpa (svreinterpret_u32 (z));

  svfloat32_t poly = svmla_x (pg, sv_f32 (d->c0), r, sv_f32 (d->c1));
  poly = svmul_x (svptrue_b32 (), poly, r);

  return svmla_x (pg, scale, scale, poly);
}

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svbool_t pg, svbool_t special,
	      const struct data *d)
{
  /* This deals with overflow and underflow in exponential for special case
     lanes.  */
  svbool_t is_inf = svcmpgt (pg, x, d->inf_bound);
  svbool_t is_zero = svcmplt (pg, x, d->zero_bound);

  /* The input `x` is further reduced (to `x/2`) to allow for accurate
     approximation on the interval `x > SpecialBound = 126.0`.  */
  x = svmul_x (special, x, 0.5);

  /* Computes exp(x/2), and set lanes with underflow/overflow.  */
  svfloat32_t half_exp = sv_exp2f_inline (x, svptrue_b32 (), d);
  half_exp = svmul_m (special, half_exp, half_exp);
  half_exp = svsel (is_inf, sv_f32 (INFINITY), half_exp);

  return svsel (is_zero, sv_f32 (0), half_exp);
}

/* Single-precision SVE exp2f routine, based on the FEXPA instruction.
   Worst case error is 2.87 +0.50 ULP.
   _ZGVsMxv_exp2f (0x1.fbcb78p+6) got 0x1.ee1d32p+126
				 want 0x1.ee1d2cp+126.  */
svfloat32_t SV_NAME_F1 (exp2) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);
  svbool_t special = svacgt (pg, x, d->special_bound);
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_case (x, pg, special, d);
  return sv_exp2f_inline (x, pg, d);
}
