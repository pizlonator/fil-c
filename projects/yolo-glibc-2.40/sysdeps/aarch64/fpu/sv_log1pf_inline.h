/* Helper for single-precision SVE routines which depend on log1p

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

#ifndef AARCH64_FPU_SV_LOG1PF_INLINE_H
#define AARCH64_FPU_SV_LOG1PF_INLINE_H

#include "sv_math.h"
#include "vecmath_config.h"
#include "poly_sve_f32.h"

static const struct sv_log1pf_data
{
  float32_t poly[9];
  float32_t ln2;
  float32_t scale_back;
} sv_log1pf_data = {
  /* Polynomial generated using FPMinimax in [-0.25, 0.5].  */
  .poly = { -0x1p-1f, 0x1.5555aap-2f, -0x1.000038p-2f, 0x1.99675cp-3f,
	    -0x1.54ef78p-3f, 0x1.28a1f4p-3f, -0x1.0da91p-3f, 0x1.abcb6p-4f,
	    -0x1.6f0d5ep-5f },
  .scale_back = 0x1.0p-23f,
  .ln2 = 0x1.62e43p-1f,
};

static inline svfloat32_t
eval_poly (svfloat32_t m, const float32_t *c, svbool_t pg)
{
  svfloat32_t p_12 = svmla_x (pg, sv_f32 (c[0]), m, sv_f32 (c[1]));
  svfloat32_t m2 = svmul_x (pg, m, m);
  svfloat32_t q = svmla_x (pg, m, m2, p_12);
  svfloat32_t p = sv_pw_horner_6_f32_x (pg, m, m2, c + 2);
  p = svmul_x (pg, m2, p);

  return svmla_x (pg, q, m2, p);
}

static inline svfloat32_t
sv_log1pf_inline (svfloat32_t x, svbool_t pg)
{
  const struct sv_log1pf_data *d = ptr_barrier (&sv_log1pf_data);

  svfloat32_t m = svadd_x (pg, x, 1.0f);

  svint32_t ks = svsub_x (pg, svreinterpret_s32 (m),
			  svreinterpret_s32 (svdup_f32 (0.75f)));
  ks = svand_x (pg, ks, 0xff800000);
  svuint32_t k = svreinterpret_u32 (ks);
  svfloat32_t s = svreinterpret_f32 (
      svsub_x (pg, svreinterpret_u32 (svdup_f32 (4.0f)), k));

  svfloat32_t m_scale
      = svreinterpret_f32 (svsub_x (pg, svreinterpret_u32 (x), k));
  m_scale
      = svadd_x (pg, m_scale, svmla_x (pg, sv_f32 (-1.0f), sv_f32 (0.25f), s));
  svfloat32_t p = eval_poly (m_scale, d->poly, pg);
  svfloat32_t scale_back = svmul_x (pg, svcvt_f32_x (pg, k), d->scale_back);
  return svmla_x (pg, p, scale_back, d->ln2);
}

#endif
