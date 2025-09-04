/* Scalar wrappers for single-precision SVE vector math functions.

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

#include "test-float-sve.h"

/* Wrapper from scalar to SVE function. Cannot just use VECTOR_WRAPPER due to
   predication.  */
#define SVE_VECTOR_WRAPPER(scalar_func, vector_func)                          \
  extern VEC_TYPE vector_func (VEC_TYPE, svbool_t);                           \
  FLOAT scalar_func (FLOAT x)                                                 \
  {                                                                           \
    VEC_TYPE mx = svdup_n_f32 (x);                                            \
    VEC_TYPE mr = vector_func (mx, svptrue_b32 ());                           \
    return svlastb_f32 (svptrue_b32 (), mr);                                  \
  }

#define SVE_VECTOR_WRAPPER_ff(scalar_func, vector_func)                       \
  extern VEC_TYPE vector_func (VEC_TYPE, VEC_TYPE, svbool_t);                 \
  FLOAT scalar_func (FLOAT x, FLOAT y)                                        \
  {                                                                           \
    VEC_TYPE mx = svdup_n_f32 (x);                                            \
    VEC_TYPE my = svdup_n_f32 (y);                                            \
    VEC_TYPE mr = vector_func (mx, my, svptrue_b32 ());                       \
    return svlastb_f32 (svptrue_b32 (), mr);                                  \
  }

SVE_VECTOR_WRAPPER (acosf_sve, _ZGVsMxv_acosf)
SVE_VECTOR_WRAPPER (acoshf_sve, _ZGVsMxv_acoshf)
SVE_VECTOR_WRAPPER (asinf_sve, _ZGVsMxv_asinf)
SVE_VECTOR_WRAPPER (asinhf_sve, _ZGVsMxv_asinhf)
SVE_VECTOR_WRAPPER (atanf_sve, _ZGVsMxv_atanf)
SVE_VECTOR_WRAPPER (atanhf_sve, _ZGVsMxv_atanhf)
SVE_VECTOR_WRAPPER_ff (atan2f_sve, _ZGVsMxvv_atan2f)
SVE_VECTOR_WRAPPER (cbrtf_sve, _ZGVsMxv_cbrtf)
SVE_VECTOR_WRAPPER (cosf_sve, _ZGVsMxv_cosf)
SVE_VECTOR_WRAPPER (coshf_sve, _ZGVsMxv_coshf)
SVE_VECTOR_WRAPPER (erff_sve, _ZGVsMxv_erff)
SVE_VECTOR_WRAPPER (erfcf_sve, _ZGVsMxv_erfcf)
SVE_VECTOR_WRAPPER (expf_sve, _ZGVsMxv_expf)
SVE_VECTOR_WRAPPER (exp10f_sve, _ZGVsMxv_exp10f)
SVE_VECTOR_WRAPPER (exp2f_sve, _ZGVsMxv_exp2f)
SVE_VECTOR_WRAPPER (expm1f_sve, _ZGVsMxv_expm1f)
SVE_VECTOR_WRAPPER_ff (hypotf_sve, _ZGVsMxvv_hypotf)
SVE_VECTOR_WRAPPER (logf_sve, _ZGVsMxv_logf)
SVE_VECTOR_WRAPPER (log10f_sve, _ZGVsMxv_log10f)
SVE_VECTOR_WRAPPER (log1pf_sve, _ZGVsMxv_log1pf)
SVE_VECTOR_WRAPPER (log2f_sve, _ZGVsMxv_log2f)
SVE_VECTOR_WRAPPER_ff (powf_sve, _ZGVsMxvv_powf)
SVE_VECTOR_WRAPPER (sinf_sve, _ZGVsMxv_sinf)
SVE_VECTOR_WRAPPER (sinhf_sve, _ZGVsMxv_sinhf)
SVE_VECTOR_WRAPPER (tanf_sve, _ZGVsMxv_tanf)
SVE_VECTOR_WRAPPER (tanhf_sve, _ZGVsMxv_tanhf)
