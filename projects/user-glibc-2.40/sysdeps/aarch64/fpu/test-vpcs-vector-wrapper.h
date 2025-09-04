/* Scalar wrapper for vpcs-enabled Advanced SIMD vector math functions.

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

#define VPCS_VECTOR_WRAPPER(scalar_func, vector_func)                         \
  extern __attribute__ ((aarch64_vector_pcs))                                 \
  VEC_TYPE vector_func (VEC_TYPE);                                            \
  FLOAT scalar_func (FLOAT x)                                                 \
  {                                                                           \
    int i;                                                                    \
    VEC_TYPE mx;                                                              \
    INIT_VEC_LOOP (mx, x, VEC_LEN);                                           \
    VEC_TYPE mr = vector_func (mx);                                           \
    TEST_VEC_LOOP (mr, VEC_LEN);                                              \
    return ((FLOAT) mr[0]);                                                   \
  }

#define VPCS_VECTOR_WRAPPER_ff(scalar_func, vector_func)                      \
  extern __attribute__ ((aarch64_vector_pcs))                                 \
  VEC_TYPE vector_func (VEC_TYPE, VEC_TYPE);                                  \
  FLOAT scalar_func (FLOAT x, FLOAT y)                                        \
  {                                                                           \
    int i;                                                                    \
    VEC_TYPE mx, my;                                                          \
    INIT_VEC_LOOP (mx, x, VEC_LEN);                                           \
    INIT_VEC_LOOP (my, y, VEC_LEN);                                           \
    VEC_TYPE mr = vector_func (mx, my);                                       \
    TEST_VEC_LOOP (mr, VEC_LEN);                                              \
    return ((FLOAT) mr[0]);                                                   \
  }
