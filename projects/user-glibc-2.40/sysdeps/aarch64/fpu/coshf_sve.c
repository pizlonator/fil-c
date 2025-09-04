/* Single-precision vector (SVE) cosh function

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
#include "sv_expf_inline.h"

static const struct data
{
  struct sv_expf_data expf_consts;
  uint32_t special_bound;
} data = {
  .expf_consts = SV_EXPF_DATA,
  /* 0x1.5a92d8p+6: expf overflows above this, so have to use special case.  */
  .special_bound = 0x42ad496c,
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t pg)
{
  return sv_call_f32 (coshf, x, y, pg);
}

/* Single-precision vector cosh, using vector expf.
   Maximum error is 1.89 ULP:
   _ZGVsMxv_coshf (-0x1.65898cp+6) got 0x1.f00aep+127
				  want 0x1.f00adcp+127.  */
svfloat32_t SV_NAME_F1 (cosh) (svfloat32_t x, svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat32_t ax = svabs_x (pg, x);
  svbool_t special = svcmpge (pg, svreinterpret_u32 (ax), d->special_bound);

  /* Calculate cosh by exp(x) / 2 + exp(-x) / 2.  */
  svfloat32_t t = expf_inline (ax, pg, &d->expf_consts);
  svfloat32_t half_t = svmul_x (pg, t, 0.5);
  svfloat32_t half_over_t = svdivr_x (pg, t, 0.5);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, svadd_x (pg, half_t, half_over_t), special);

  return svadd_x (pg, half_t, half_over_t);
}
