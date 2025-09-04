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

#include "test-double-vlen4.h"
#include "test-math-vector-sincos.h"
#include <immintrin.h>

#undef VEC_SUFF
#define VEC_SUFF _vlen4_avx2

#define VEC_TYPE __m256d

VECTOR_WRAPPER (WRAPPER_NAME (cos), _ZGVdN4v_cos)
VECTOR_WRAPPER (WRAPPER_NAME (sin), _ZGVdN4v_sin)
VECTOR_WRAPPER (WRAPPER_NAME (log), _ZGVdN4v_log)
VECTOR_WRAPPER (WRAPPER_NAME (exp), _ZGVdN4v_exp)
VECTOR_WRAPPER_ff (WRAPPER_NAME (pow), _ZGVdN4vv_pow)
VECTOR_WRAPPER (WRAPPER_NAME (acos), _ZGVdN4v_acos)
VECTOR_WRAPPER (WRAPPER_NAME (atan), _ZGVdN4v_atan)
VECTOR_WRAPPER (WRAPPER_NAME (asin), _ZGVdN4v_asin)
VECTOR_WRAPPER_ff (WRAPPER_NAME (hypot), _ZGVdN4vv_hypot)
VECTOR_WRAPPER (WRAPPER_NAME (exp2), _ZGVdN4v_exp2)
VECTOR_WRAPPER (WRAPPER_NAME (exp10), _ZGVdN4v_exp10)
VECTOR_WRAPPER (WRAPPER_NAME (cosh), _ZGVdN4v_cosh)
VECTOR_WRAPPER (WRAPPER_NAME (expm1), _ZGVdN4v_expm1)
VECTOR_WRAPPER (WRAPPER_NAME (sinh), _ZGVdN4v_sinh)
VECTOR_WRAPPER (WRAPPER_NAME (cbrt), _ZGVdN4v_cbrt)
VECTOR_WRAPPER_ff (WRAPPER_NAME (atan2), _ZGVdN4vv_atan2)
VECTOR_WRAPPER (WRAPPER_NAME (log10), _ZGVdN4v_log10)
VECTOR_WRAPPER (WRAPPER_NAME (log2), _ZGVdN4v_log2)
VECTOR_WRAPPER (WRAPPER_NAME (log1p), _ZGVdN4v_log1p)
VECTOR_WRAPPER (WRAPPER_NAME (atanh), _ZGVdN4v_atanh)
VECTOR_WRAPPER (WRAPPER_NAME (acosh), _ZGVdN4v_acosh)
VECTOR_WRAPPER (WRAPPER_NAME (erf), _ZGVdN4v_erf)
VECTOR_WRAPPER (WRAPPER_NAME (tanh), _ZGVdN4v_tanh)
VECTOR_WRAPPER (WRAPPER_NAME (asinh), _ZGVdN4v_asinh)
VECTOR_WRAPPER (WRAPPER_NAME (erfc), _ZGVdN4v_erfc)
VECTOR_WRAPPER (WRAPPER_NAME (tan), _ZGVdN4v_tan)

#ifndef __ILP32__
# define VEC_INT_TYPE __m256i
#else
# define VEC_INT_TYPE __m128i
#endif

VECTOR_WRAPPER_fFF_2 (WRAPPER_NAME (sincos), _ZGVdN4vvv_sincos)
