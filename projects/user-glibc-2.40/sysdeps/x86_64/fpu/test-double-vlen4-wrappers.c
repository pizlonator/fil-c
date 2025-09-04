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

#include "test-double-vlen4.h"
#include "test-math-vector-sincos.h"
#include <immintrin.h>

#define VEC_TYPE __m256d

VECTOR_WRAPPER (WRAPPER_NAME (cos), _ZGVcN4v_cos)
VECTOR_WRAPPER (WRAPPER_NAME (sin), _ZGVcN4v_sin)
VECTOR_WRAPPER (WRAPPER_NAME (log), _ZGVcN4v_log)
VECTOR_WRAPPER (WRAPPER_NAME (exp), _ZGVcN4v_exp)
VECTOR_WRAPPER_ff (WRAPPER_NAME (pow), _ZGVcN4vv_pow)
VECTOR_WRAPPER (WRAPPER_NAME (acos), _ZGVcN4v_acos)
VECTOR_WRAPPER (WRAPPER_NAME (atan), _ZGVcN4v_atan)
VECTOR_WRAPPER (WRAPPER_NAME (asin), _ZGVcN4v_asin)
VECTOR_WRAPPER_ff (WRAPPER_NAME (hypot), _ZGVcN4vv_hypot)
VECTOR_WRAPPER (WRAPPER_NAME (exp2), _ZGVcN4v_exp2)
VECTOR_WRAPPER (WRAPPER_NAME (exp10), _ZGVcN4v_exp10)
VECTOR_WRAPPER (WRAPPER_NAME (cosh), _ZGVcN4v_cosh)
VECTOR_WRAPPER (WRAPPER_NAME (expm1), _ZGVcN4v_expm1)
VECTOR_WRAPPER (WRAPPER_NAME (sinh), _ZGVcN4v_sinh)
VECTOR_WRAPPER (WRAPPER_NAME (cbrt), _ZGVcN4v_cbrt)
VECTOR_WRAPPER_ff (WRAPPER_NAME (atan2), _ZGVcN4vv_atan2)
VECTOR_WRAPPER (WRAPPER_NAME (log10), _ZGVcN4v_log10)
VECTOR_WRAPPER (WRAPPER_NAME (log2), _ZGVcN4v_log2)
VECTOR_WRAPPER (WRAPPER_NAME (log1p), _ZGVcN4v_log1p)
VECTOR_WRAPPER (WRAPPER_NAME (atanh), _ZGVcN4v_atanh)
VECTOR_WRAPPER (WRAPPER_NAME (acosh), _ZGVcN4v_acosh)
VECTOR_WRAPPER (WRAPPER_NAME (erf), _ZGVcN4v_erf)
VECTOR_WRAPPER (WRAPPER_NAME (tanh), _ZGVcN4v_tanh)
VECTOR_WRAPPER (WRAPPER_NAME (asinh), _ZGVcN4v_asinh)
VECTOR_WRAPPER (WRAPPER_NAME (erfc), _ZGVcN4v_erfc)
VECTOR_WRAPPER (WRAPPER_NAME (tan), _ZGVcN4v_tan)

#define VEC_INT_TYPE __m128i

#ifndef __ILP32__
VECTOR_WRAPPER_fFF_3 (WRAPPER_NAME (sincos), _ZGVcN4vvv_sincos)
#else
VECTOR_WRAPPER_fFF_2 (WRAPPER_NAME (sincos), _ZGVcN4vvv_sincos)
#endif
