/* Single-precision vector (SVE) log2 function

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

static const struct data
{
  float c1, c3, c5, c7;
  float c0, c2, c4, c6, c8;
  float two_2_23;
  uint32_t off, lower, thresh;
} data = {
  /* Coefficients copied from the AdvSIMD routine, then rearranged so that
     coeffs 1, 3, 5 and 7 can be loaded as a single quad-word, hence used with
     _lane variant of MLA intrinsic.  */
  .c0 = 0x1.715476p0f,
  .c1 = -0x1.715458p-1f,
  .c2 = 0x1.ec701cp-2f,
  .c3 = -0x1.7171a4p-2f,
  .c4 = 0x1.27a0b8p-2f,
  .c5 = -0x1.e5143ep-3f,
  .c6 = 0x1.9d8ecap-3f,
  .c7 = -0x1.c675bp-3f,
  .c8 = 0x1.9e495p-3f,
  .off = 0x3f2aaaab,
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .lower = 0xC1555555,	/* 0x00800000 - 0x3f2aaaab.  */
  .two_2_23 = 0x1p23,	/* 2^23.  */
  .thresh = 0x7f000000, /* asuint32(inf) - 0x00800000.  */
};

#define MantissaMask 0x007fffff

static inline svfloat32_t
v_log2f_inline (svuint32_t u_off, svbool_t pg, const struct data *d)
{
  svuint32_t u = svand_x (pg, u_off, MantissaMask);
  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  svfloat32_t n = svcvt_f32_x (
      pg, svasr_x (pg, svreinterpret_s32 (u_off), 23)); /* Sign-extend.  */
  u = svadd_x (pg, u, d->off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (u), 1.0f);

  /* Evaluate polynomial using pairwise Horner scheme.  */
  svfloat32_t p_1357 = svld1rq (svptrue_b32 (), &d->c1);
  svfloat32_t q_01 = svmla_lane (sv_f32 (d->c0), r, p_1357, 0);
  svfloat32_t q_23 = svmla_lane (sv_f32 (d->c2), r, p_1357, 1);
  svfloat32_t q_45 = svmla_lane (sv_f32 (d->c4), r, p_1357, 2);
  svfloat32_t q_67 = svmla_lane (sv_f32 (d->c6), r, p_1357, 3);
  /* y = log2(1+r) + n.  */
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);
  svfloat32_t y = svmla_x (pg, q_67, r2, sv_f32 (d->c8));
  y = svmla_x (pg, q_45, r2, y);
  y = svmla_x (pg, q_23, r2, y);
  y = svmla_x (pg, q_01, r2, y);

  return svmla_x (svptrue_b32 (), n, r, y);
}

/* The special case is made up of a series of selects which chose the correct
   outcome of the special lanes from inf, -inf or nan or for subnormals a
   calculation of x * 2^23 (2^mantissa) to normalise the number at entry to
   the log function and then subtract log2(2) * 23 = 23 to re-subnormalise the
   output to the correct result.  */
static inline svfloat32_t
special_case (svfloat32_t x, svbool_t pg, svbool_t special,
	      const struct data *d)
{
  /* Check covers subnormal range. This is greater than the actual range but
     standard case lanes and +inf are handled seperately.  */
  svbool_t is_sub = svcmpgt_f32 (pg, x, sv_f32 (0));
  /* Check for 0 which = -Infinity.  */
  svbool_t is_minf = svcmpeq_f32 (pg, x, sv_f32 (0));
  svbool_t is_pinf = svcmpeq_f32 (pg, x, sv_f32 (INFINITY));

  /* Increase x for special cases to catch sub normals.  */
  x = svmul_m (special, x, d->two_2_23);
  svuint32_t u_off = svreinterpret_u32 (x);
  u_off = svsub_m (pg, u_off, d->off);

  /* Select correct special case correction depending on x.  */
  svfloat32_t special_log = svsel (is_sub, sv_f32 (-23), sv_f32 (NAN));
  special_log = svsel (is_minf, sv_f32 (-INFINITY), special_log);
  special_log = svsel (is_pinf, sv_f32 (INFINITY), special_log);

  /* Return log for both special after offset and none special cases.  */
  svfloat32_t ret_log = v_log2f_inline (u_off, svptrue_b32 (), d);

  /* Reduce the output of log for special cases to complete the subnormals
     calculation or add inf, -inf or nan depending on special_log.
     Return log without correction for none special lanes.  */
  return svadd_m (special, ret_log, special_log);
}

/* Optimised implementation of SVE log2f, using the same algorithm
   and polynomial as AdvSIMD log2f.
   Maximum error is 2.48 ULPs:
   SV_NAME_F1 (log2)(0x1.558174p+0) got 0x1.a9be84p-2
				   want 0x1.a9be8p-2.  */
svfloat32_t SV_NAME_F1 (log2) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t u_off = svreinterpret_u32 (x);
  u_off = svsub_x (pg, u_off, d->off);
  /* Special cases: x is subnormal, x <= 0, x == inf, x == nan.  */
  svbool_t special = svcmpge (pg, svsub_x (pg, u_off, d->lower), d->thresh);
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_case (x, pg, special, d);

  /* If no special cases just return log_2f function call.  */
  return v_log2f_inline (u_off, pg, d);
}
