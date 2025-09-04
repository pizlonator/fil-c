/* Single-precision vector (SVE) log2 function

   Copyright (C) 2023-2024 Free Software Foundation, Inc.
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
  float poly_02468[5];
  float poly_1357[4];
} data = {
  .poly_1357 = {
    /* Coefficients copied from the AdvSIMD routine, then rearranged so that coeffs
       1, 3, 5 and 7 can be loaded as a single quad-word, hence used with _lane
       variant of MLA intrinsic.  */
    -0x1.715458p-1f, -0x1.7171a4p-2f, -0x1.e5143ep-3f, -0x1.c675bp-3f
  },
  .poly_02468 = { 0x1.715476p0f, 0x1.ec701cp-2f, 0x1.27a0b8p-2f,
		  0x1.9d8ecap-3f, 0x1.9e495p-3f },
};

#define Min (0x00800000)
#define Max (0x7f800000)
#define Thres (0x7f000000) /* Max - Min.  */
#define MantissaMask (0x007fffff)
#define Off (0x3f2aaaab) /* 0.666667.  */

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t cmp)
{
  return sv_call_f32 (log2f, x, y, cmp);
}

/* Optimised implementation of SVE log2f, using the same algorithm
   and polynomial as AdvSIMD log2f.
   Maximum error is 2.48 ULPs:
   SV_NAME_F1 (log2)(0x1.558174p+0) got 0x1.a9be84p-2
				   want 0x1.a9be8p-2.  */
svfloat32_t SV_NAME_F1 (log2) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t u = svreinterpret_u32 (x);
  svbool_t special = svcmpge (pg, svsub_x (pg, u, Min), Thres);

  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  u = svsub_x (pg, u, Off);
  svfloat32_t n = svcvt_f32_x (
      pg, svasr_x (pg, svreinterpret_s32 (u), 23)); /* Sign-extend.  */
  u = svand_x (pg, u, MantissaMask);
  u = svadd_x (pg, u, Off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (u), 1.0f);

  /* y = log2(1+r) + n.  */
  svfloat32_t r2 = svmul_x (pg, r, r);

  /* Evaluate polynomial using pairwise Horner scheme.  */
  svfloat32_t p_1357 = svld1rq (svptrue_b32 (), &d->poly_1357[0]);
  svfloat32_t q_01 = svmla_lane (sv_f32 (d->poly_02468[0]), r, p_1357, 0);
  svfloat32_t q_23 = svmla_lane (sv_f32 (d->poly_02468[1]), r, p_1357, 1);
  svfloat32_t q_45 = svmla_lane (sv_f32 (d->poly_02468[2]), r, p_1357, 2);
  svfloat32_t q_67 = svmla_lane (sv_f32 (d->poly_02468[3]), r, p_1357, 3);
  svfloat32_t y = svmla_x (pg, q_67, r2, sv_f32 (d->poly_02468[4]));
  y = svmla_x (pg, q_45, r2, y);
  y = svmla_x (pg, q_23, r2, y);
  y = svmla_x (pg, q_01, r2, y);

  if (__glibc_unlikely (svptest_any (pg, special)))
    return special_case (x, svmla_x (svnot_z (pg, special), n, r, y), special);
  return svmla_x (pg, n, r, y);
}
