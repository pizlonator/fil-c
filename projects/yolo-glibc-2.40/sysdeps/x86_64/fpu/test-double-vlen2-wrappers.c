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

#include "test-double-vlen2.h"
#include "test-math-vector-sincos.h"
#include <immintrin.h>

#define VEC_TYPE __m128d

VECTOR_WRAPPER (WRAPPER_NAME (cos), _ZGVbN2v_cos)
VECTOR_WRAPPER (WRAPPER_NAME (sin), _ZGVbN2v_sin)
VECTOR_WRAPPER (WRAPPER_NAME (log), _ZGVbN2v_log)
VECTOR_WRAPPER (WRAPPER_NAME (exp), _ZGVbN2v_exp)
VECTOR_WRAPPER_ff (WRAPPER_NAME (pow), _ZGVbN2vv_pow)
VECTOR_WRAPPER (WRAPPER_NAME (acos), _ZGVbN2v_acos)
VECTOR_WRAPPER (WRAPPER_NAME (atan), _ZGVbN2v_atan)
VECTOR_WRAPPER (WRAPPER_NAME (asin), _ZGVbN2v_asin)
VECTOR_WRAPPER_ff (WRAPPER_NAME (hypot), _ZGVbN2vv_hypot)
VECTOR_WRAPPER (WRAPPER_NAME (exp2), _ZGVbN2v_exp2)
VECTOR_WRAPPER (WRAPPER_NAME (exp10), _ZGVbN2v_exp10)
VECTOR_WRAPPER (WRAPPER_NAME (cosh), _ZGVbN2v_cosh)
VECTOR_WRAPPER (WRAPPER_NAME (expm1), _ZGVbN2v_expm1)
VECTOR_WRAPPER (WRAPPER_NAME (sinh), _ZGVbN2v_sinh)
VECTOR_WRAPPER (WRAPPER_NAME (cbrt), _ZGVbN2v_cbrt)
VECTOR_WRAPPER_ff (WRAPPER_NAME (atan2), _ZGVbN2vv_atan2)
VECTOR_WRAPPER (WRAPPER_NAME (log10), _ZGVbN2v_log10)
VECTOR_WRAPPER (WRAPPER_NAME (log2), _ZGVbN2v_log2)
VECTOR_WRAPPER (WRAPPER_NAME (log1p), _ZGVbN2v_log1p)
VECTOR_WRAPPER (WRAPPER_NAME (atanh), _ZGVbN2v_atanh)
VECTOR_WRAPPER (WRAPPER_NAME (acosh), _ZGVbN2v_acosh)
VECTOR_WRAPPER (WRAPPER_NAME (erf), _ZGVbN2v_erf)
VECTOR_WRAPPER (WRAPPER_NAME (tanh), _ZGVbN2v_tanh)
VECTOR_WRAPPER (WRAPPER_NAME (asinh), _ZGVbN2v_asinh)
VECTOR_WRAPPER (WRAPPER_NAME (erfc), _ZGVbN2v_erfc)
VECTOR_WRAPPER (WRAPPER_NAME (tan), _ZGVbN2v_tan)

#define VEC_INT_TYPE __m128i

VECTOR_WRAPPER_fFF_2 (WRAPPER_NAME (sincos), _ZGVbN2vvv_sincos)
