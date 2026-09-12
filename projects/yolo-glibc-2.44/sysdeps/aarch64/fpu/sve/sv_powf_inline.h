/* Helper for SVE single-precision pow

   Copyright (C) 2025-2026 Free Software Foundation, Inc.
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

#include "powf_common.h"

#ifndef WANT_SV_POWF_SIGN_BIAS
#  error                                                                       \
      "Cannot use sv_powf_inline.h without specifying whether you need sign_bias."
#endif

/* The following data is used in the SVE pow core computation
   and special case detection.  */
#define Tinvc __v_powf_data.invc
#define Tlogc __v_powf_data.logc
#define Texp __v_powf_data.scale
#define SignBias (1 << (V_POWF_EXP2_TABLE_BITS + 11))
static const struct data
{
  double log_poly[4];
  double exp_poly[3];
  float uflow_bound, oflow_bound, small_bound;
  uint32_t sign_bias, subnormal_bias, off;
} data = {
  /* rel err: 1.5 * 2^-30. Each coefficients is multiplied the value of
     V_POWF_EXP2_N.  */
  .log_poly = { -0x1.6ff5daa3b3d7cp+3, 0x1.ec81d03c01aebp+3,
		-0x1.71547bb43f101p+4, 0x1.7154764a815cbp+5 },
  /* rel err: 1.69 * 2^-34.  */
  .exp_poly = {
    0x1.c6af84b912394p-20, /* A0 / V_POWF_EXP2_N^3.  */
    0x1.ebfce50fac4f3p-13, /* A1 / V_POWF_EXP2_N^2.  */
    0x1.62e42ff0c52d6p-6,   /* A3 / V_POWF_EXP2_N.  */
  },
  .uflow_bound = -0x1.2cp+12f, /* -150.0 * V_POWF_EXP2_N.  */
  .oflow_bound = 0x1p+12f, /* 128.0 * V_POWF_EXP2_N.  */
  .small_bound = 0x1p-126f,
  .off = 0x3f35d000,
  .sign_bias = SignBias,
  .subnormal_bias = 0x0b800000, /* 23 << 23.  */
};

/* Check if x is an integer.  */
static inline svbool_t
svisint (svbool_t pg, svfloat32_t x)
{
  return svcmpeq (pg, svrintz_z (pg, x), x);
}

/* Check if x is real not integer valued.  */
static inline svbool_t
svisnotint (svbool_t pg, svfloat32_t x)
{
  return svcmpne (pg, svrintz_z (pg, x), x);
}

/* Check if x is an odd integer.  */
static inline svbool_t
svisodd (svbool_t pg, svfloat32_t x)
{
  svfloat32_t y = svmul_x (pg, x, 0.5f);
  return svisnotint (pg, y);
}

/* Check if zero, inf or nan.  */
static inline svbool_t
sv_zeroinfnan (svbool_t pg, svuint32_t i)
{
  return svcmpge (pg, svsub_x (pg, svadd_x (pg, i, i), 1),
		  2u * 0x7f800000 - 1);
}

/* Compute core for half of the lanes in double precision.  */
static inline svfloat64_t
sv_powf_core_ext (const svbool_t pg, svuint64_t i, svfloat64_t z, svint64_t k,
#if WANT_SV_POWF_SIGN_BIAS
		  svfloat64_t y, svuint64_t sign_bias, svfloat64_t *pylogx,
#else
		  svfloat64_t y, svfloat64_t *pylogx,
#endif
		  const struct data *d)
{
  svfloat64_t invc = svld1_gather_index (pg, Tinvc, i);
  svfloat64_t logc = svld1_gather_index (pg, Tlogc, i);

  /* log2(x) = log1p(z/c-1)/ln2 + log2(c) + k.  */
  svfloat64_t r = svmla_x (pg, sv_f64 (-1.0), z, invc);
  svfloat64_t y0 = svadd_x (pg, logc, svcvt_f64_x (pg, k));

  /* Polynomial to approximate log1p(r)/ln2.  */
  svfloat64_t logx = sv_f64 (d->log_poly[0]);
  logx = svmad_x (pg, r, logx, sv_f64 (d->log_poly[1]));
  logx = svmad_x (pg, r, logx, sv_f64 (d->log_poly[2]));
  logx = svmad_x (pg, r, logx, sv_f64 (d->log_poly[3]));
  logx = svmad_x (pg, r, logx, y0);
  *pylogx = svmul_x (pg, y, logx);

  /* z - kd is in [-1, 1] in non-nearest rounding modes.  */
  svfloat64_t kd = svrinta_x (svptrue_b64 (), *pylogx);
  svuint64_t ki = svreinterpret_u64 (svcvt_s64_x (svptrue_b64 (), kd));

  r = svsub_x (pg, *pylogx, kd);

  /* exp2(x) = 2^(k/N) * 2^r ~= s * (C0*r^3 + C1*r^2 + C2*r + 1).  */
  svuint64_t t = svld1_gather_index (
      svptrue_b64 (), Texp, svand_x (svptrue_b64 (), ki, V_POWF_EXP2_N - 1));
#if WANT_SV_POWF_SIGN_BIAS
  svuint64_t ski = svadd_x (svptrue_b64 (), ki, sign_bias);
  t = svadd_x (svptrue_b64 (), t,
	       svlsl_x (svptrue_b64 (), ski, 52 - V_POWF_EXP2_TABLE_BITS));
#else
  t = svadd_x (svptrue_b64 (), t,
	       svlsl_x (svptrue_b64 (), ki, 52 - V_POWF_EXP2_TABLE_BITS));
#endif
  svfloat64_t s = svreinterpret_f64 (t);

  svfloat64_t p = sv_f64 (d->exp_poly[0]);
  p = svmla_x (pg, sv_f64 (d->exp_poly[1]), p, r);
  p = svmla_x (pg, sv_f64 (d->exp_poly[2]), p, r);
  p = svmla_x (pg, s, p, svmul_x (svptrue_b64 (), s, r));

  return p;
}

/* Widen vector to double precision and compute core on both halves of the
   vector. Lower cost of promotion by considering all lanes active.  */
static inline svfloat32_t
sv_powf_core (const svbool_t pg, svuint32_t i, svuint32_t iz, svint32_t k,
	      svfloat32_t y, svuint32_t sign_bias, svfloat32_t *pylogx,
	      const struct data *d)
{
  const svbool_t ptrue = svptrue_b64 ();

  /* Unpack and promote input vectors (pg, y, z, i, k and sign_bias) into two
     in order to perform core computation in double precision.  */
  const svbool_t pg_lo = svunpklo (pg);
  const svbool_t pg_hi = svunpkhi (pg);
  svfloat64_t y_lo = svcvt_f64_x (
      ptrue, svreinterpret_f32 (svunpklo (svreinterpret_u32 (y))));
  svfloat64_t y_hi = svcvt_f64_x (
      ptrue, svreinterpret_f32 (svunpkhi (svreinterpret_u32 (y))));
  svfloat64_t z_lo = svcvt_f64_x (ptrue, svreinterpret_f32 (svunpklo (iz)));
  svfloat64_t z_hi = svcvt_f64_x (ptrue, svreinterpret_f32 (svunpkhi (iz)));
  svuint64_t i_lo = svunpklo (i);
  svuint64_t i_hi = svunpkhi (i);
  svint64_t k_lo = svunpklo (k);
  svint64_t k_hi = svunpkhi (k);
#if WANT_SV_POWF_SIGN_BIAS
  svuint64_t sign_bias_lo = svunpklo (sign_bias);
  svuint64_t sign_bias_hi = svunpkhi (sign_bias);
#endif

  /* Compute each part in double precision.  */
  svfloat64_t ylogx_lo, ylogx_hi;
#if WANT_SV_POWF_SIGN_BIAS
  svfloat64_t lo = sv_powf_core_ext (pg_lo, i_lo, z_lo, k_lo, y_lo,
				     sign_bias_lo, &ylogx_lo, d);
  svfloat64_t hi = sv_powf_core_ext (pg_hi, i_hi, z_hi, k_hi, y_hi,
				     sign_bias_hi, &ylogx_hi, d);
#else
  svfloat64_t lo
      = sv_powf_core_ext (pg_lo, i_lo, z_lo, k_lo, y_lo, &ylogx_lo, d);
  svfloat64_t hi
      = sv_powf_core_ext (pg_hi, i_hi, z_hi, k_hi, y_hi, &ylogx_hi, d);
#endif

  /* Convert back to single-precision and interleave.  */
  svfloat32_t ylogx_lo_32 = svcvt_f32_x (ptrue, ylogx_lo);
  svfloat32_t ylogx_hi_32 = svcvt_f32_x (ptrue, ylogx_hi);
  *pylogx = svuzp1 (ylogx_lo_32, ylogx_hi_32);
  svfloat32_t lo_32 = svcvt_f32_x (ptrue, lo);
  svfloat32_t hi_32 = svcvt_f32_x (ptrue, hi);
  return svuzp1 (lo_32, hi_32);
}
