/* Wrapper part of tests for SSE ISA versions of vector math functions.
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

#include "test-float-vlen4.h"
#include "test-math-vector-sincos.h"
#include <immintrin.h>

#define VEC_TYPE __m128

VECTOR_WRAPPER (WRAPPER_NAME (cosf), _ZGVbN4v_cosf)
VECTOR_WRAPPER (WRAPPER_NAME (sinf), _ZGVbN4v_sinf)
VECTOR_WRAPPER (WRAPPER_NAME (logf), _ZGVbN4v_logf)
VECTOR_WRAPPER (WRAPPER_NAME (expf), _ZGVbN4v_expf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (powf), _ZGVbN4vv_powf)
VECTOR_WRAPPER (WRAPPER_NAME (acosf), _ZGVbN4v_acosf)
VECTOR_WRAPPER (WRAPPER_NAME (atanf), _ZGVbN4v_atanf)
VECTOR_WRAPPER (WRAPPER_NAME (asinf), _ZGVbN4v_asinf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (hypotf), _ZGVbN4vv_hypotf)
VECTOR_WRAPPER (WRAPPER_NAME (exp2f), _ZGVbN4v_exp2f)
VECTOR_WRAPPER (WRAPPER_NAME (exp10f), _ZGVbN4v_exp10f)
VECTOR_WRAPPER (WRAPPER_NAME (coshf), _ZGVbN4v_coshf)
VECTOR_WRAPPER (WRAPPER_NAME (expm1f), _ZGVbN4v_expm1f)
VECTOR_WRAPPER (WRAPPER_NAME (sinhf), _ZGVbN4v_sinhf)
VECTOR_WRAPPER (WRAPPER_NAME (cbrtf), _ZGVbN4v_cbrtf)
VECTOR_WRAPPER_ff (WRAPPER_NAME (atan2f), _ZGVbN4vv_atan2f)
VECTOR_WRAPPER (WRAPPER_NAME (log10f), _ZGVbN4v_log10f)
VECTOR_WRAPPER (WRAPPER_NAME (log2f), _ZGVbN4v_log2f)
VECTOR_WRAPPER (WRAPPER_NAME (log1pf), _ZGVbN4v_log1pf)
VECTOR_WRAPPER (WRAPPER_NAME (atanhf), _ZGVbN4v_atanhf)
VECTOR_WRAPPER (WRAPPER_NAME (acoshf), _ZGVbN4v_acoshf)
VECTOR_WRAPPER (WRAPPER_NAME (erff), _ZGVbN4v_erff)
VECTOR_WRAPPER (WRAPPER_NAME (tanhf), _ZGVbN4v_tanhf)
VECTOR_WRAPPER (WRAPPER_NAME (asinhf), _ZGVbN4v_asinhf)
VECTOR_WRAPPER (WRAPPER_NAME (erfcf), _ZGVbN4v_erfcf)
VECTOR_WRAPPER (WRAPPER_NAME (tanf), _ZGVbN4v_tanf)

#define VEC_INT_TYPE __m128i

#ifndef __ILP32__
VECTOR_WRAPPER_fFF_3 (WRAPPER_NAME (sincosf), _ZGVbN4vvv_sincosf)
#else
VECTOR_WRAPPER_fFF_2 (WRAPPER_NAME (sincosf), _ZGVbN4vvv_sincosf)
#endif
