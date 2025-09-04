/* Wrapper part of tests for AVX-512 versions of vector math functions.
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

#include "test-double-vlen8.h"
#include "test-math-vector-sincos.h"
#include <immintrin.h>

#define VEC_TYPE __m512d

VECTOR_WRAPPER (WRAPPER_NAME (cos), _ZGVeN8v_cos)
VECTOR_WRAPPER (WRAPPER_NAME (sin), _ZGVeN8v_sin)
VECTOR_WRAPPER (WRAPPER_NAME (log), _ZGVeN8v_log)
VECTOR_WRAPPER (WRAPPER_NAME (exp), _ZGVeN8v_exp)
VECTOR_WRAPPER_ff (WRAPPER_NAME (pow), _ZGVeN8vv_pow)
VECTOR_WRAPPER (WRAPPER_NAME (acos), _ZGVeN8v_acos)
VECTOR_WRAPPER (WRAPPER_NAME (atan), _ZGVeN8v_atan)
VECTOR_WRAPPER (WRAPPER_NAME (asin), _ZGVeN8v_asin)
VECTOR_WRAPPER_ff (WRAPPER_NAME (hypot), _ZGVeN8vv_hypot)
VECTOR_WRAPPER (WRAPPER_NAME (exp2), _ZGVeN8v_exp2)
VECTOR_WRAPPER (WRAPPER_NAME (exp10), _ZGVeN8v_exp10)
VECTOR_WRAPPER (WRAPPER_NAME (cosh), _ZGVeN8v_cosh)
VECTOR_WRAPPER (WRAPPER_NAME (expm1), _ZGVeN8v_expm1)
VECTOR_WRAPPER (WRAPPER_NAME (sinh), _ZGVeN8v_sinh)
VECTOR_WRAPPER (WRAPPER_NAME (cbrt), _ZGVeN8v_cbrt)
VECTOR_WRAPPER_ff (WRAPPER_NAME (atan2), _ZGVeN8vv_atan2)
VECTOR_WRAPPER (WRAPPER_NAME (log10), _ZGVeN8v_log10)
VECTOR_WRAPPER (WRAPPER_NAME (log2), _ZGVeN8v_log2)
VECTOR_WRAPPER (WRAPPER_NAME (log1p), _ZGVeN8v_log1p)
VECTOR_WRAPPER (WRAPPER_NAME (atanh), _ZGVeN8v_atanh)
VECTOR_WRAPPER (WRAPPER_NAME (acosh), _ZGVeN8v_acosh)
VECTOR_WRAPPER (WRAPPER_NAME (erf), _ZGVeN8v_erf)
VECTOR_WRAPPER (WRAPPER_NAME (tanh), _ZGVeN8v_tanh)
VECTOR_WRAPPER (WRAPPER_NAME (asinh), _ZGVeN8v_asinh)
VECTOR_WRAPPER (WRAPPER_NAME (erfc), _ZGVeN8v_erfc)
VECTOR_WRAPPER (WRAPPER_NAME (tan), _ZGVeN8v_tan)

#ifndef __ILP32__
# define VEC_INT_TYPE __m512i
#else
# define VEC_INT_TYPE __m256i
#endif

VECTOR_WRAPPER_fFF_2 (WRAPPER_NAME (sincos), _ZGVeN8vvv_sincos)
