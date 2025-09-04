/* Utilities for SVE libmvec routines.
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

#ifndef SV_MATH_H
#define SV_MATH_H

#include <arm_sve.h>
#include <stdbool.h>

#include "vecmath_config.h"

#define SV_NAME_F1(fun) _ZGVsMxv_##fun##f
#define SV_NAME_D1(fun) _ZGVsMxv_##fun
#define SV_NAME_F2(fun) _ZGVsMxvv_##fun##f
#define SV_NAME_D2(fun) _ZGVsMxvv_##fun

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
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      double elem = svclastb_n_f64 (p, 0, x);
      elem = (*f) (elem);
      svfloat64_t y2 = svdup_n_f64 (elem);
      y = svsel_f64 (p, y2, y);
      p = svpnext_b64 (cmp, p);
    }
  return y;
}

static inline svfloat64_t
sv_call2_f64 (double (*f) (double, double), svfloat64_t x1, svfloat64_t x2,
	      svfloat64_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      double elem1 = svclastb_n_f64 (p, 0, x1);
      double elem2 = svclastb_n_f64 (p, 0, x2);
      double ret = (*f) (elem1, elem2);
      svfloat64_t y2 = svdup_n_f64 (ret);
      y = svsel_f64 (p, y2, y);
      p = svpnext_b64 (cmp, p);
    }
  return y;
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
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      float elem = svclastb_n_f32 (p, 0, x);
      elem = f (elem);
      svfloat32_t y2 = svdup_n_f32 (elem);
      y = svsel_f32 (p, y2, y);
      p = svpnext_b32 (cmp, p);
    }
  return y;
}

static inline svfloat32_t
sv_call2_f32 (float (*f) (float, float), svfloat32_t x1, svfloat32_t x2,
	      svfloat32_t y, svbool_t cmp)
{
  svbool_t p = svpfirst (cmp, svpfalse ());
  while (svptest_any (cmp, p))
    {
      float elem1 = svclastb_n_f32 (p, 0, x1);
      float elem2 = svclastb_n_f32 (p, 0, x2);
      float ret = f (elem1, elem2);
      svfloat32_t y2 = svdup_n_f32 (ret);
      y = svsel_f32 (p, y2, y);
      p = svpnext_b32 (cmp, p);
    }
  return y;
}

#endif
