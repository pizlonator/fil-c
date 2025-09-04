/* Wrapper part of tests for AVX2 ISA versions of vector math functions.
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

#undef VEC_SUFF
#define VEC_SUFF _vlen8_avx2

#define VEC_TYPE __m256

VECTOR_WRAPPER (WRAPPER_NAME (cosf), _ZGVdN8v_cosf)
VECTOR_WRAPPER (WRAPPER_NAME (sinf), _ZGVdN8v_sinf)
VECTOR_WRAPPER (WRAPPER_NAME (logf), _ZGVdN8v_logf)
VECTOR_WRAPPER (WRAPPER_NAME (expf), _ZGVdN8v_expf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (powf), _ZGVdN8vv_powf)
VECTOR_WRAPPER (WRAPPER_NAME (acosf), _ZGVdN8v_acosf)
VECTOR_WRAPPER (WRAPPER_NAME (atanf), _ZGVdN8v_atanf)
VECTOR_WRAPPER (WRAPPER_NAME (asinf), _ZGVdN8v_asinf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (hypotf), _ZGVdN8vv_hypotf)
VECTOR_WRAPPER (WRAPPER_NAME (exp2f), _ZGVdN8v_exp2f)
VECTOR_WRAPPER (WRAPPER_NAME (exp10f), _ZGVdN8v_exp10f)
VECTOR_WRAPPER (WRAPPER_NAME (coshf), _ZGVdN8v_coshf)
VECTOR_WRAPPER (WRAPPER_NAME (expm1f), _ZGVdN8v_expm1f)
VECTOR_WRAPPER (WRAPPER_NAME (sinhf), _ZGVdN8v_sinhf)
VECTOR_WRAPPER (WRAPPER_NAME (cbrtf), _ZGVdN8v_cbrtf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (atan2f), _ZGVdN8vv_atan2f)
VECTOR_WRAPPER (WRAPPER_NAME (log10f), _ZGVdN8v_log10f)
VECTOR_WRAPPER (WRAPPER_NAME (log2f), _ZGVdN8v_log2f)
VECTOR_WRAPPER (WRAPPER_NAME (log1pf), _ZGVdN8v_log1pf)
VECTOR_WRAPPER (WRAPPER_NAME (atanhf), _ZGVdN8v_atanhf)
VECTOR_WRAPPER (WRAPPER_NAME (acoshf), _ZGVdN8v_acoshf)
VECTOR_WRAPPER (WRAPPER_NAME (erff), _ZGVdN8v_erff)
VECTOR_WRAPPER (WRAPPER_NAME (tanhf), _ZGVdN8v_tanhf)
VECTOR_WRAPPER (WRAPPER_NAME (asinhf), _ZGVdN8v_asinhf)
VECTOR_WRAPPER (WRAPPER_NAME (erfcf), _ZGVdN8v_erfcf)
VECTOR_WRAPPER (WRAPPER_NAME (tanf), _ZGVdN8v_tanf)

/* Redefinition of wrapper to be compatible with _ZGVdN8vvv_sincosf.  */
#undef VECTOR_WRAPPER_fFF

#define VEC_INT_TYPE __m256i

#ifndef __ILP32__
VECTOR_WRAPPER_fFF_3 (WRAPPER_NAME (sincosf), _ZGVdN8vvv_sincosf)
#else
VECTOR_WRAPPER_fFF_2 (WRAPPER_NAME (sincosf), _ZGVdN8vvv_sincosf)
#endif
