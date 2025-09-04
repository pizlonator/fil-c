/* Wrapper part of tests for AVX ISA versions of vector math functions.
   Copyright (C) 2014-2024 Free Software Foundation, Inc.
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

#include "test-float-vlen8.h"
#include "test-math-vector-sincos.h"
#include <immintrin.h>

#define VEC_TYPE __m256

VECTOR_WRAPPER (WRAPPER_NAME (cosf), _ZGVcN8v_cosf)
VECTOR_WRAPPER (WRAPPER_NAME (sinf), _ZGVcN8v_sinf)
VECTOR_WRAPPER (WRAPPER_NAME (logf), _ZGVcN8v_logf)
VECTOR_WRAPPER (WRAPPER_NAME (expf), _ZGVcN8v_expf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (powf), _ZGVcN8vv_powf)
VECTOR_WRAPPER (WRAPPER_NAME (acosf), _ZGVcN8v_acosf)
VECTOR_WRAPPER (WRAPPER_NAME (atanf), _ZGVcN8v_atanf)
VECTOR_WRAPPER (WRAPPER_NAME (asinf), _ZGVcN8v_asinf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (hypotf), _ZGVcN8vv_hypotf)
VECTOR_WRAPPER (WRAPPER_NAME (exp2f), _ZGVcN8v_exp2f)
VECTOR_WRAPPER (WRAPPER_NAME (exp10f), _ZGVcN8v_exp10f)
VECTOR_WRAPPER (WRAPPER_NAME (coshf), _ZGVcN8v_coshf)
VECTOR_WRAPPER (WRAPPER_NAME (expm1f), _ZGVcN8v_expm1f)
VECTOR_WRAPPER (WRAPPER_NAME (sinhf), _ZGVcN8v_sinhf)
VECTOR_WRAPPER (WRAPPER_NAME (cbrtf), _ZGVcN8v_cbrtf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (atan2f), _ZGVcN8vv_atan2f)
VECTOR_WRAPPER (WRAPPER_NAME (log10f), _ZGVcN8v_log10f)
VECTOR_WRAPPER (WRAPPER_NAME (log2f), _ZGVcN8v_log2f)
VECTOR_WRAPPER (WRAPPER_NAME (log1pf), _ZGVcN8v_log1pf)
VECTOR_WRAPPER (WRAPPER_NAME (atanhf), _ZGVcN8v_atanhf)
VECTOR_WRAPPER (WRAPPER_NAME (acoshf), _ZGVcN8v_acoshf)
VECTOR_WRAPPER (WRAPPER_NAME (erff), _ZGVcN8v_erff)
VECTOR_WRAPPER (WRAPPER_NAME (tanhf), _ZGVcN8v_tanhf)
VECTOR_WRAPPER (WRAPPER_NAME (asinhf), _ZGVcN8v_asinhf)
VECTOR_WRAPPER (WRAPPER_NAME (erfcf), _ZGVcN8v_erfcf)
VECTOR_WRAPPER (WRAPPER_NAME (tanf), _ZGVcN8v_tanf)

#define VEC_INT_TYPE __m128i

#ifndef __ILP32__
VECTOR_WRAPPER_fFF_4 (WRAPPER_NAME (sincosf), _ZGVcN8vvv_sincosf)
#else
VECTOR_WRAPPER_fFF_3 (WRAPPER_NAME (sincosf), _ZGVcN8vvv_sincosf)
#endif
