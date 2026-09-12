/* Double-precision vector (SVE) log function.

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

#define N (1 << V_LOG_TABLE_BITS)

static const struct data
{
  double c1, c3;
  double ln2, c4;
  double c0, c2;
  double two_2_52, ln2_52;
  uint64_t off, thresh;
} data = {
  .c0 = -0x1.ffffffffffff7p-2,
  .c1 = 0x1.55555555170d4p-2,
  .c2 = -0x1.0000000399c27p-2,
  .c3 = 0x1.999b2e90e94cap-3,
  .c4 = -0x1.554e550bd501ep-3,
  .ln2 = 0x1.62e42fefa39efp-1,
  .two_2_52 = 0x1p52,		   /* 2^52.  */
  .ln2_52 = -0x1.205966f2b4f12p+5, /* ln(2) * 52 ~ 36.04.  */
  .off = 0x3fe6900900000000,
  /* The threshold is computed from the lower and upper bounds,
     respectively the smallest normalised number, min = 0x0010000000000000
     and infinity, 0x7ff0000000000000.  */
  .thresh = (0x7fe0000000000000), /* infinity - min.  */
};

static inline svfloat64_t
v_log_inline (svuint64_t ix, const svbool_t pg, const struct data *d)
{
  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  svuint64_t tmp = svsub_x (pg, ix, d->off);
  /* Calculate table index = (tmp >> (52 - V_LOG_TABLE_BITS)) % N.
     The actual value of i is double this due to table layout.  */
  svuint64_t i
      = svand_x (pg, svlsr_x (pg, tmp, (51 - V_LOG_TABLE_BITS)), (N - 1) << 1);
  svuint64_t iz = svsub_x (pg, ix, svand_x (pg, tmp, 0xfffULL << 52));
  svfloat64_t z = svreinterpret_f64 (iz);
  /* Lookup in 2 global lists (length N).  */
  svfloat64_t invc = svld1_gather_index (pg, &__v_log_data.table[0].invc, i);
  svfloat64_t logc = svld1_gather_index (pg, &__v_log_data.table[0].logc, i);

  /* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
  svfloat64_t kd = svcvt_f64_x (pg, svasr_x (pg, svreinterpret_s64 (tmp), 52));
  /* hi = r + log(c) + k*Ln2.  */
  svfloat64_t ln2_and_c4 = svld1rq_f64 (svptrue_b64 (), &d->ln2);
  svfloat64_t r = svmad_x (pg, invc, z, -1);
  svfloat64_t hi = svmla_lane_f64 (logc, kd, ln2_and_c4, 0);
  hi = svadd_x (pg, r, hi);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  svfloat64_t odd_coeffs = svld1rq_f64 (svptrue_b64 (), &d->c1);
  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t y = svmla_lane_f64 (sv_f64 (d->c2), r, odd_coeffs, 1);
  svfloat64_t p = svmla_lane_f64 (sv_f64 (d->c0), r, odd_coeffs, 0);
  y = svmla_lane_f64 (y, r2, ln2_and_c4, 1);
  y = svmla_x (pg, p, r2, y);

  return svmla_x (pg, hi, r2, y);
}

/* The special case is made up of a series of selects which chose the correct
   outcome of the special lanes from inf, -inf or nan or for subnormals a
   calculation of x * 2^52 (2^mantissa) to normalise the number at entry to
   the log function and then subtract ln(2) * 52 to re-subnormalise the
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
  svfloat64_t special_log = svsel (is_sub, sv_f64 (d->ln2_52), sv_f64 (NAN));
  special_log = svsel (is_minf, sv_f64 (-INFINITY), special_log);
  special_log = svsel (is_pinf, sv_f64 (INFINITY), special_log);

  /* Return log for both special after offset and none special cases.  */
  svfloat64_t log_sum = v_log_inline (ix, pg, d);

  /* Reduce the output of log for special cases to complete the subnormals
     calculation or add inf, -inf or nan depending on special_log.
     Return log without correction for none special lanes.  */
  return svadd_m (special, log_sum, special_log);
}

/* Double-precision SVE log routine.
   Maximum measured error is 2.64 ulp:
   SV_NAME_D1 (log)(0x1.95e54bc91a5e2p+184) got 0x1.fffffffe88cacp+6
					   want 0x1.fffffffe88cafp+6.  */
svfloat64_t SV_NAME_D1 (log) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svuint64_t ix = svreinterpret_u64 (x);
  /* Special cases: x is subnormal, x <= 0, x == inf, x == nan.  */
  svbool_t special
      = svcmpge (pg, svsub_x (pg, ix, 0x0010000000000000), d->thresh);
  if (__glibc_unlikely (svptest_any (special, special)))
    return special_case (x, pg, special);

  /* If no special cases just return log function call.  */
  return v_log_inline (ix, pg, d);
}
