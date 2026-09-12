/* Utilities for SVE libmvec routines.
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

#ifndef SV_MATH_H
#define SV_MATH_H

#include <arm_sve.h>
#include <stdbool.h>

#include "vecmath_config.h"

#if !defined(__ARM_FEATURE_SVE_BITS) || __ARM_FEATURE_SVE_BITS == 0
/* If not specified by -msve-vector-bits, assume maximum vector length.  */
# define SVE_VECTOR_BYTES 256
#else
# define SVE_VECTOR_BYTES (__ARM_FEATURE_SVE_BITS / 8)
#endif
#define SVE_NUM_FLTS (SVE_VECTOR_BYTES / sizeof (float))
#define SVE_NUM_DBLS (SVE_VECTOR_BYTES / sizeof (double))
/* Predicate is stored as one bit per byte of VL so requires VL / 64 bytes.  */
#define SVE_NUM_PG_BYTES (SVE_VECTOR_BYTES / sizeof (uint64_t))

#define SV_NAME_F1(fun) _ZGVsMxv_##fun##f
#define SV_NAME_D1(fun) _ZGVsMxv_##fun
#define SV_NAME_F2(fun) _ZGVsMxvv_##fun##f
#define SV_NAME_D2(fun) _ZGVsMxvv_##fun

static inline void
svstr_p (uint8_t *dst, svbool_t p)
{
  /* Predicate STR does not currently have an intrinsic.  */
  __asm__("str %0, [%x1]\n" : : "Upa"(p), "r"(dst) : "memory");
}

/* Double precision.  */
static inline svint64_t
sv_s64 (int64_t x)
{
  return svdup_n_s64 (x);
}

static inline svuint64_t
sv_u64 (uint64_t x)
{
  return svdup_n_u64 (x);
}

static inline svfloat64_t
sv_f64 (double x)
{
  return svdup_n_f64 (x);
}

static inline svfloat64_t
sv_call_f64 (double (*f) (double), svfloat64_t x, svfloat64_t y, svbool_t cmp)
{
  double tmp[SVE_NUM_DBLS];
  uint8_t pg_bits[SVE_NUM_PG_BYTES];
  svstr_p (pg_bits, cmp);
  svst1 (svptrue_b64 (), tmp, svsel (cmp, x, y));

  for (int i = 0; i < svcntd (); i++)
    {
      if (pg_bits[i] & 1)
	tmp[i] = f (tmp[i]);
    }
  return svld1 (svptrue_b64 (), tmp);
}

static inline svfloat64_t
sv_call2_f64 (double (*f) (double, double), svfloat64_t x1, svfloat64_t x2,
	      svfloat64_t y, svbool_t cmp)
{
  double tmp1[SVE_NUM_DBLS], tmp2[SVE_NUM_DBLS];
  uint8_t pg_bits[SVE_NUM_PG_BYTES];
  svstr_p (pg_bits, cmp);
  svst1 (svptrue_b64 (), tmp1, svsel (cmp, x1, y));
  svst1 (cmp, tmp2, x2);

  for (int i = 0; i < svcntd (); i++)
    {
      if (pg_bits[i] & 1)
	tmp1[i] = f (tmp1[i], tmp2[i]);
    }
  return svld1 (svptrue_b64 (), tmp1);
}

static inline svuint64_t
sv_mod_n_u64_x (svbool_t pg, svuint64_t x, uint64_t y)
{
  svuint64_t q = svdiv_n_u64_x (pg, x, y);
  return svmls_n_u64_x (pg, x, q, y);
}

/* Single precision.  */
static inline svint32_t
sv_s32 (int32_t x)
{
  return svdup_n_s32 (x);
}

static inline svuint32_t
sv_u32 (uint32_t x)
{
  return svdup_n_u32 (x);
}

static inline svfloat32_t
sv_f32 (float x)
{
  return svdup_n_f32 (x);
}

static inline svfloat32_t
sv_call_f32 (float (*f) (float), svfloat32_t x, svfloat32_t y, svbool_t cmp)
{
  float tmp[SVE_NUM_FLTS];
  uint8_t pg_bits[SVE_NUM_PG_BYTES];
  svstr_p (pg_bits, cmp);
  svst1 (svptrue_b32 (), tmp, svsel (cmp, x, y));

  for (int i = 0; i < svcntd (); i++)
    {
      uint8_t p = pg_bits[i];
      if (p & 1)
	tmp[i * 2] = f (tmp[i * 2]);
      if (p & (1 << 4))
	tmp[i * 2 + 1] = f (tmp[i * 2 + 1]);
    }
  return svld1 (svptrue_b32 (), tmp);
}

static inline svfloat32_t
sv_call2_f32 (float (*f) (float, float), svfloat32_t x1, svfloat32_t x2,
	      svfloat32_t y, svbool_t cmp)
{
  float tmp1[SVE_NUM_FLTS], tmp2[SVE_NUM_FLTS];
  uint8_t pg_bits[SVE_NUM_PG_BYTES];
  svstr_p (pg_bits, cmp);
  svst1 (svptrue_b32 (), tmp1, svsel (cmp, x1, y));
  svst1 (cmp, tmp2, x2);

  for (int i = 0; i < svcntd (); i++)
    {
      uint8_t p = pg_bits[i];
      if (p & 1)
	tmp1[i * 2] = f (tmp1[i * 2], tmp2[i * 2]);
      if (p & (1 << 4))
	tmp1[i * 2 + 1] = f (tmp1[i * 2 + 1], tmp2[i * 2 + 1]);
    }
  return svld1 (svptrue_b32 (), tmp1);
}
#endif
