/* Scalar wrappers for single-precision Advanced SIMD vector math functions.

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

#include <arm_neon.h>

#include "test-float-advsimd.h"

#define VEC_TYPE float32x4_t

VPCS_VECTOR_WRAPPER (acosf_advsimd, _ZGVnN4v_acosf)
VPCS_VECTOR_WRAPPER (acoshf_advsimd, _ZGVnN4v_acoshf)
VPCS_VECTOR_WRAPPER (asinf_advsimd, _ZGVnN4v_asinf)
VPCS_VECTOR_WRAPPER (asinhf_advsimd, _ZGVnN4v_asinhf)
VPCS_VECTOR_WRAPPER (atanf_advsimd, _ZGVnN4v_atanf)
VPCS_VECTOR_WRAPPER (atanhf_advsimd, _ZGVnN4v_atanhf)
VPCS_VECTOR_WRAPPER_ff (atan2f_advsimd, _ZGVnN4vv_atan2f)
VPCS_VECTOR_WRAPPER (cbrtf_advsimd, _ZGVnN4v_cbrtf)
VPCS_VECTOR_WRAPPER (cosf_advsimd, _ZGVnN4v_cosf)
VPCS_VECTOR_WRAPPER (coshf_advsimd, _ZGVnN4v_coshf)
VPCS_VECTOR_WRAPPER (erff_advsimd, _ZGVnN4v_erff)
VPCS_VECTOR_WRAPPER (erfcf_advsimd, _ZGVnN4v_erfcf)
VPCS_VECTOR_WRAPPER (expf_advsimd, _ZGVnN4v_expf)
VPCS_VECTOR_WRAPPER (exp10f_advsimd, _ZGVnN4v_exp10f)
VPCS_VECTOR_WRAPPER (exp2f_advsimd, _ZGVnN4v_exp2f)
VPCS_VECTOR_WRAPPER (expm1f_advsimd, _ZGVnN4v_expm1f)
VPCS_VECTOR_WRAPPER_ff (hypotf_advsimd, _ZGVnN4vv_hypotf)
VPCS_VECTOR_WRAPPER (logf_advsimd, _ZGVnN4v_logf)
VPCS_VECTOR_WRAPPER (log10f_advsimd, _ZGVnN4v_log10f)
VPCS_VECTOR_WRAPPER (log1pf_advsimd, _ZGVnN4v_log1pf)
VPCS_VECTOR_WRAPPER (log2f_advsimd, _ZGVnN4v_log2f)
VPCS_VECTOR_WRAPPER_ff (powf_advsimd, _ZGVnN4vv_powf)
VPCS_VECTOR_WRAPPER (sinf_advsimd, _ZGVnN4v_sinf)
VPCS_VECTOR_WRAPPER (sinhf_advsimd, _ZGVnN4v_sinhf)
VPCS_VECTOR_WRAPPER (tanf_advsimd, _ZGVnN4v_tanf)
VPCS_VECTOR_WRAPPER (tanhf_advsimd, _ZGVnN4v_tanhf)
