/* Scalar wrappers for double-precision SVE vector math functions.

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

#include <arm_sve.h>

#include "test-double-sve.h"

/* Wrapper from scalar to SVE function. Cannot just use VECTOR_WRAPPER due to
   predication.  */
#define SVE_VECTOR_WRAPPER(scalar_func, vector_func)                          \
  extern VEC_TYPE vector_func (VEC_TYPE, svbool_t);                           \
  FLOAT scalar_func (FLOAT x)                                                 \
  {                                                                           \
    VEC_TYPE mx = svdup_n_f64 (x);                                            \
    VEC_TYPE mr = vector_func (mx, svptrue_b64 ());                           \
    return svlastb_f64 (svptrue_b64 (), mr);                                  \
  }

#define SVE_VECTOR_WRAPPER_ff(scalar_func, vector_func)                       \
  extern VEC_TYPE vector_func (VEC_TYPE, VEC_TYPE, svbool_t);                 \
  FLOAT scalar_func (FLOAT x, FLOAT y)                                        \
  {                                                                           \
    VEC_TYPE mx = svdup_n_f64 (x);                                            \
    VEC_TYPE my = svdup_n_f64 (y);                                            \
    VEC_TYPE mr = vector_func (mx, my, svptrue_b64 ());                       \
    return svlastb_f64 (svptrue_b64 (), mr);                                  \
  }

SVE_VECTOR_WRAPPER (acos_sve, _ZGVsMxv_acos)
SVE_VECTOR_WRAPPER (acosh_sve, _ZGVsMxv_acosh)
SVE_VECTOR_WRAPPER (asin_sve, _ZGVsMxv_asin)
SVE_VECTOR_WRAPPER (asinh_sve, _ZGVsMxv_asinh)
SVE_VECTOR_WRAPPER (atan_sve, _ZGVsMxv_atan)
SVE_VECTOR_WRAPPER (atanh_sve, _ZGVsMxv_atanh)
SVE_VECTOR_WRAPPER_ff (atan2_sve, _ZGVsMxvv_atan2)
SVE_VECTOR_WRAPPER (cbrt_sve, _ZGVsMxv_cbrt)
SVE_VECTOR_WRAPPER (cos_sve, _ZGVsMxv_cos)
SVE_VECTOR_WRAPPER (cosh_sve, _ZGVsMxv_cosh)
SVE_VECTOR_WRAPPER (erf_sve, _ZGVsMxv_erf)
SVE_VECTOR_WRAPPER (erfc_sve, _ZGVsMxv_erfc)
SVE_VECTOR_WRAPPER (exp_sve, _ZGVsMxv_exp)
SVE_VECTOR_WRAPPER (exp10_sve, _ZGVsMxv_exp10)
SVE_VECTOR_WRAPPER (exp2_sve, _ZGVsMxv_exp2)
SVE_VECTOR_WRAPPER (expm1_sve, _ZGVsMxv_expm1)
SVE_VECTOR_WRAPPER_ff (hypot_sve, _ZGVsMxvv_hypot)
SVE_VECTOR_WRAPPER (log_sve, _ZGVsMxv_log)
SVE_VECTOR_WRAPPER (log10_sve, _ZGVsMxv_log10)
SVE_VECTOR_WRAPPER (log1p_sve, _ZGVsMxv_log1p)
SVE_VECTOR_WRAPPER (log2_sve, _ZGVsMxv_log2)
SVE_VECTOR_WRAPPER_ff (pow_sve, _ZGVsMxvv_pow)
SVE_VECTOR_WRAPPER (sin_sve, _ZGVsMxv_sin)
SVE_VECTOR_WRAPPER (sinh_sve, _ZGVsMxv_sinh)
SVE_VECTOR_WRAPPER (tan_sve, _ZGVsMxv_tan)
SVE_VECTOR_WRAPPER (tanh_sve, _ZGVsMxv_tanh)
