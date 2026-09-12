/* Double-precision vector (SVE) log2 function

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

#define N (1 << V_LOG2_TABLE_BITS)

static const struct data
{
  double c1, c3;
  double invln2, c4;
  double c0, c2;
  double two_2_52;
  uint64_t off, thresh;
} data = {
  .c0 = -0x1.71547652b83p-1,
  .c1 = 0x1.ec709dc340953p-2,
  .c2 = -0x1.71547651c8f35p-2,
  .c3 = 0x1.2777ebe12dda5p-2,
  .c4 = -0x1.ec738d616fe26p-3,
  .invln2 = 0x1.71547652b82fep0,
  .two_2_52 = 0x1p52, /* 2^52.  */
  .off = 0x3fe6900900000000,
  /* The threshold is computed from the lower and upper bounds,
     respectively the smallest normalised number, min = 0x0010000000000000
     and infinity, 0x7ff0000000000000.  */
  .thresh = (0x7fe0000000000000), /* infinity - min.  */
};

static inline svfloat64_t
v_log2_inline (svuint64_t ix, const svbool_t pg, const struct data *d)
{
  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, d->off);
  svfloat64_t k = svcvt_f64_x (pg, svasr_x (pg, svreinterpret_s64 (tmp), 52));
  svfloat64_t z = svreinterpret_f64 (
      svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52)));
  svuint64_t i = svlsr_x (pg, tmp, 51 - V_LOG2_TABLE_BITS);
  i = svand_x (pg, i, (N - 1) << 1);
  svfloat64_t invc = svld1_gather_index (pg, &__v_log2_data.table[0].invc, i);
  svfloat64_t log2c
      = svld1_gather_index (pg, &__v_log2_data.table[0].log2c, i);

  /* log2(x) = log1p(z/c-1)/log(2) + log2(c) + k.  */

  svfloat64_t invln2_and_c4 = svld1rq_f64 (svptrue_b64 (), &d->invln2);
  svfloat64_t r = svmad_x (pg, invc, z, -1.0);
  svfloat64_t w = svmla_lane_f64 (log2c, r, invln2_and_c4, 0);
  w = svadd_x (pg, k, w);

  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t odd_coeffs = svld1rq_f64 (svptrue_b64 (), &d->c1);
  svfloat64_t p = svmla_lane_f64 (sv_f64 (d->c0), r, odd_coeffs, 0);
  svfloat64_t y = svmla_lane_f64 (sv_f64 (d->c2), r, odd_coeffs, 1);
  y = svmla_lane_f64 (y, r2, invln2_and_c4, 1);
  y = svmla_x (pg, p, r2, y);

  return svmla_x (pg, w, r2, y);
}

/* The special case is made up of a series of selects which chose the correct
   outcome of the special lanes from inf, -inf or nan or for subnormals a
   calculation of x * 2^52 (2^mantissa) to normalise the number at entry to
   the log function and then subtract log2(2) * 52 = 52 to re-subnormalise the
   output to the correct result.  */
static svfloat64_t NOINLINE
special_case (svfloat64_t x, svbool_t pg, svbool_t special)
{
  const struct data *d = ptr_barrier (&data);
  /* Check covers subnormal range. This is greater than the actual range but
     standard case lanes and +inf are handled seperately.  */
  svbool_t is_sub = svcmpgt_f64 (pg, x, sv_f64 (0));
  /* Check for 0 which = -Infinity.  */
  svbool_t is_minf = svcmpeq_f64 (pg, x, sv_f64 (0));
  svbool_t is_pinf = svcmpeq_f64 (pg, x, sv_f64 (INFINITY));

  /* Increase x for special cases to catch sub normals.  */
  x = svmul_m (special, x, d->two_2_52);
  svuint64_t ix = svreinterpret_u64 (x);

  /* Select correct special case correction depending on x.  */
  svfloat64_t special_log = svsel (is_sub, sv_f64 (-52), sv_f64 (NAN));
  special_log = svsel (is_minf, sv_f64 (-INFINITY), special_log);
  special_log = svsel (is_pinf, sv_f64 (INFINITY), special_log);

  /* Return log for both special after offset and none special cases.  */
  svfloat64_t log_sum = v_log2_inline (ix, pg, d);

  /* Reduce the output of log for special cases to complete the subnormals
     calculation or add inf, -inf or nan depending on special_log.
     Return log without correction for none special lanes.  */
  return svadd_m (special, log_sum, special_log);
}

/* Double-precision SVE log2 routine.
   Implements the same algorithm as AdvSIMD log10, with coefficients and table
   entries scaled in extended precision.
   The maximum observed error is 2.58 ULP:
   SV_NAME_D1 (log2)(0x1.0b556b093869bp+0) got 0x1.fffb34198d9dap-5
					  want 0x1.fffb34198d9ddp-5.  */
svfloat64_t SV_NAME_D1 (log2) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t ix = svreinterpret_u64 (x);
  /* Special cases: x is subnormal, x <= 0, x == inf, x == nan.  */
  svbool_t special
      = svcmpge (pg, svsub_x (pg, ix, 0x0010000000000000), d->thresh);
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_case (x, pg, special);

  /* If no special cases just return log_2 function call.  */
  return v_log2_inline (ix, pg, d);
}
