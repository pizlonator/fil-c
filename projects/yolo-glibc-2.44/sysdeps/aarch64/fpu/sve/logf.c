/* Single-precision vector (SVE) log function.

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
  float c0, c1, c3, c5;
  float c2, c4, c6, ln2;
  float ln2_23, two_2_23;
  uint32_t off, lower, thresh;
} data = {
  /* Coefficients copied from the AdvSIMD routine, then rearranged so that
     coeffs 0, 1, 3,and 5 can be loaded as a single quad-word, hence used with
     _lane variant of MLA intrinsic.  */
  .c0 = -0x1.3e737cp-3f,
  .c1 = 0x1.5a9aa2p-3f,
  .c2 = -0x1.4f9934p-3f,
  .c3 = 0x1.961348p-3f,
  .c4 = -0x1.00187cp-2f,
  .c5 = 0x1.555d7cp-2f,
  .c6 = -0x1.ffffc8p-2f,
  .ln2 = 0x1.62e43p-1f,
  .off = 0x3f2aaaab,
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .lower = 0x00800000 - 0x3f2aaaab,
  .ln2_23 = -0x1.fe2804p+3, /* ln(2) * 23 ~ 15.94.  */
  .two_2_23 = 0x1p23,	    /* 2^23.  */
  .thresh = 0x7f000000,	    /* asuint32(inf) - 0x00800000.  */
};

#define MantissaMask 0x007fffff

static inline svfloat32_t
v_logf_inline (svuint32_t u_off, const svbool_t pg, const struct data *d)
{
  svuint32_t u = svand_x (pg, u_off, MantissaMask);
  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  svfloat32_t n = svcvt_f32_x (
      pg, svasr_x (pg, svreinterpret_s32 (u_off), 23)); /* Sign-extend.  */
  u = svadd_x (pg, u, d->off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (u), 1.0f);

  /* y = log(1+r) + n*ln2.  */
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);
  /* n*ln2 + r + r2*(P6 + r*P5 + r2*(P4 + r*P3 + r2*(P2 + r*P1 + r2*P0))).  */
  svfloat32_t p_0135 = svld1rq (svptrue_b32 (), &d->c0);
  svfloat32_t p = svmla_lane (sv_f32 (d->c2), r, p_0135, 1);
  svfloat32_t q = svmla_lane (sv_f32 (d->c4), r, p_0135, 2);
  svfloat32_t y = svmla_lane (sv_f32 (d->c6), r, p_0135, 3);
  p = svmla_lane (p, r2, p_0135, 0);

  q = svmla_x (pg, q, r2, p);
  y = svmla_x (pg, y, r2, q);
  p = svmla_x (pg, r, n, d->ln2);

  /* Minus the shift to get the result by ln2(52).  */
  return svmla_x (svptrue_b32 (), p, r2, y);
}

/* The special case is made up of a series of selects which chose the correct
   outcome of the special lanes from inf, -inf or nan or for subnormals a
   calculation of x * 2^23 (2^mantissa) to normalise the number at entry to
   the log function and then subtract ln(2) * 23 to re-subnormalise the result
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
  svfloat32_t special_log = svsel (is_sub, sv_f32 (d->ln2_23), sv_f32 (NAN));
  special_log = svsel (is_minf, sv_f32 (-INFINITY), special_log);
  special_log = svsel (is_pinf, sv_f32 (INFINITY), special_log);

  /* Return log for both special after offset and none special cases.  */
  svfloat32_t ret_log = v_logf_inline (u_off, svptrue_b32 (), d);

  /* Reduce the output of log for special cases to complete the subnormals
     calculation or add inf, -inf or nan depending on special_log.
     Return log without correction for none special lanes.  */
  return svadd_m (special, ret_log, special_log);
}

/* Optimised implementation of SVE logf, using the same algorithm and
   polynomial as the AdvSIMD routine. Maximum error is 3.34 ULPs:
   SV_NAME_F1 (log)(0x1.557298p+0) got 0x1.26edecp-2
				  want 0x1.26ede6p-2.  */
svfloat32_t SV_NAME_F1 (log) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t u_off = svreinterpret_u32 (x);
  u_off = svsub_x (pg, u_off, d->off);
  /* Special cases: x is subnormal, x <= 0, x == inf, x == nan.  */
  svbool_t special = svcmpge (pg, svsub_x (pg, u_off, d->lower), d->thresh);
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_case (x, pg, special, d);

  /* If no special cases just return log_f function call.  */
  return v_logf_inline (u_off, pg, d);
}
