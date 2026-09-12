/* Single-precision vector (SVE) log10 function

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
  float c0, c2, c4, c6;
  float ln2, inv_ln10;
  float log10_23, two_2_23;
  uint32_t off, lower, thresh;
} data = {
  /* Coefficients copied from the AdvSIMD routine, then rearranged so that
     coeffs 1, 3, 5 and 7 can be loaded as a single quad-word, hence used with
     _lane variant of MLA intrinsic.  */
  .c0 = -0x1.bcb79cp-3f,
  .c1 = 0x1.2879c8p-3f,
  .c2 = -0x1.bcd472p-4f,
  .c3 = 0x1.6408f8p-4f,
  .c4 = -0x1.246f8p-4f,
  .c5 = 0x1.f0e514p-5f,
  .c6 = -0x1.0fc92cp-4f,
  .c7 = 0x1.f5f76ap-5f,
  .ln2 = 0x1.62e43p-1f,
  .inv_ln10 = 0x1.bcb7b2p-2f,
  .off = 0x3f2aaaab,
  /* Lower bound is the smallest positive normal float 0x00800000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound is 0x0080000 - offset (which wraps around).  */
  .lower = 0x00800000 - 0x3f2aaaab,
  .log10_23 = -0x1.bb1dbcp+2, /* log10(2) * 23 ~ 6.92.  */
  .two_2_23 = 0x1p23,	      /* 2^23.  */
  .thresh = 0x7f000000,	      /* asuint32(inf) - 0x00800000.  */
};

#define MantissaMask 0x007fffff

static inline svfloat32_t
v_log10f_inline (svuint32_t u_off, const svbool_t pg, const struct data *d)
{
  /* x = 2^n * (1+r), where 2/3 < 1+r < 4/3.  */
  svfloat32_t n = svcvt_f32_x (
      pg, svasr_x (pg, svreinterpret_s32 (u_off), 23)); /* signextend.  */
  svuint32_t ix = svand_x (pg, u_off, MantissaMask);
  ix = svadd_x (pg, ix, d->off);
  svfloat32_t r = svsub_x (pg, svreinterpret_f32 (ix), 1.0f);

  /* y = log10(1+r) + n*log10(2)
     log10(1+r) ~ r * InvLn(10) + P(r)
     where P(r) is a polynomial. Use order 9 for log10(1+x), i.e. order 8 for
     log10(1+x)/x, with x in [-1/3, 1/3] (offset=2/3).  */
  svfloat32_t r2 = svmul_x (svptrue_b32 (), r, r);
  svfloat32_t r4 = svmul_x (svptrue_b32 (), r2, r2);
  svfloat32_t p_1357 = svld1rq (svptrue_b32 (), &d->c1);
  svfloat32_t q_01 = svmla_lane (sv_f32 (d->c0), r, p_1357, 0);
  svfloat32_t q_23 = svmla_lane (sv_f32 (d->c2), r, p_1357, 1);
  svfloat32_t q_45 = svmla_lane (sv_f32 (d->c4), r, p_1357, 2);
  svfloat32_t q_67 = svmla_lane (sv_f32 (d->c6), r, p_1357, 3);
  svfloat32_t q_47 = svmla_x (pg, q_45, r2, q_67);
  svfloat32_t q_03 = svmla_x (pg, q_01, r2, q_23);
  svfloat32_t y = svmla_x (pg, q_03, r4, q_47);

  /* Using hi = Log10(2)*n + r*InvLn(10) is faster but less accurate.  */
  svfloat32_t ln2_inv_ln10 = svld1rq (svptrue_b32 (), &d->ln2);
  svfloat32_t hi = svmla_lane (r, n, ln2_inv_ln10, 0);
  hi = svmul_lane (hi, ln2_inv_ln10, 1);

  return svmla_x (svptrue_b32 (), hi, r2, y);
}

/* The special case is made up of a series of selects which chose the correct
   outcome of the special lanes from inf, -inf or nan or for subnormals a
   calculation of x * 2^23 (2^mantissa) to normalise the number at entry to
   the log function and then subtract log10(2) * 23 to re-subnormalise the
   output to the correct result.  */
static svfloat32_t NOINLINE
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
  svfloat32_t special_log = svsel (is_sub, sv_f32 (d->log10_23), sv_f32 (NAN));
  special_log = svsel (is_minf, sv_f32 (-INFINITY), special_log);
  special_log = svsel (is_pinf, sv_f32 (INFINITY), special_log);

  /* Return log for both special after offset and none special cases.  */
  svfloat32_t ret_log = v_log10f_inline (u_off, svptrue_b32 (), d);

  /* Reduce the output of log for special cases to complete the subnormals
     calculation or add inf, -inf or nan depending on special_log.
     Return log without correction for none special lanes.  */
  return svadd_m (special, ret_log, special_log);
}

/* Optimised implementation of SVE log10f using the same algorithm and
   polynomial as AdvSIMD log10f.
   Maximum error is 3.31ulps:
   SV_NAME_F1 (log10)(0x1.555c16p+0) got 0x1.ffe2fap-4
				    want 0x1.ffe2f4p-4.  */
svfloat32_t SV_NAME_F1 (log10) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint32_t u_off = svreinterpret_u32 (x);
  u_off = svsub_x (pg, u_off, d->off);
  /* Special cases: x is subnormal, x <= 0, x == inf, x == nan.  */
  svbool_t special = svcmpge (pg, svsub_x (pg, u_off, d->lower), d->thresh);
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_case (x, pg, special, d);

  /* If no special cases just return log_10f function call.  */
  return v_log10f_inline (u_off, pg, d);
}
