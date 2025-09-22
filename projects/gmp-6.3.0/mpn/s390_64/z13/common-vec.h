/* Common vector helpers and macros for IBM z13 and later

Copyright 2021 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of either:

  * the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your
    option) any later version.

or

  * the GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any
    later version.

or both in parallel, as here.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received copies of the GNU General Public License and the
GNU Lesser General Public License along with the GNU MP Library.  If not,
see https://www.gnu.org/licenses/.  */

#ifndef __S390_64_Z13_COMMON_VEC_H
#define __S390_64_Z13_COMMON_VEC_H

#include <unistd.h>
#include <vecintrin.h>

/*
 * Vector intrinsics use vector element types that kind-of make sense for the
 * specific operation (e.g., vec_permi permutes doublewords). To use VRs
 * interchangeably with different intrinsics, typedef the two variants and wrap
 * them in a union.
 */
#define VLEN_BYTES 16
typedef unsigned long long v2di __attribute__ ((vector_size (VLEN_BYTES)));
typedef unsigned char v16qi __attribute__ ((vector_size (VLEN_BYTES)));

/*
 * The Z vector intrinsics use vectors with different element types (e.g.,
 * v16qi for the 128-bit adds and v2di for vec_permi).
 */
union vec
{
  v2di dw;
  v16qi sw;
};

typedef union vec vec_t;

/*
 * single-instruction combine of two GPRs into a VR
 */
static inline v2di
vec_load_2di_as_pair (unsigned long a, unsigned long b)
{
  v2di res;
  __asm__("vlvgp\t%0,%1,%2" : "=v"(res) : "r"(a), "r"(b));
  return res;
}

/*
 * 64x64 mult where caller needs to care about proper register allocation:
 * multiply xl with m1, treating both as unsigned, and place the result in
 * xh:xl.
 * mlgr operates on register pairs, so xh must be an even gpr followed by xl
 */
#define s390_umul_ppmm(xh, xl, m1)                                              \
  do                                                                          \
    {                                                                         \
      asm("mlgr\t%0,%3" : "=r"(xh), "=r"(xl) : "%1"(xl), "r"(m1));            \
    }                                                                         \
  while (0);

/*
 * two 64x64 multiplications, scheduled so that they will dispatch and issue to
 * different sides: each mlgr is dispatched alone in an instruction group and
 * subsequent groups will issue on different execution sides.
 * there is a variant where both products use the same multiplicand and one
 * that uses two different multiplicands. constraints from s390_umul_ppmm apply
 * here.
 */
#define s390_double_umul_ppmm(X0H, X0L, X1H, X1L, MX)                           \
  do                                                                          \
    {                                                                         \
      asm("mlgr\t%[x0h],%[mx]\n\t"                                            \
          "mlgr\t%[x1h],%[mx]"                                                \
          : [x0h] "=&r"(X0H), [x0l] "=&r"(X0L), [x1h] "=r"(X1H),              \
            [x1l] "=r"(X1L)                                                   \
          : "[x0l]"(X0L), "[x1l]"(X1L), [mx] "r"(MX));                        \
    }                                                                         \
  while (0);

#define s390_double_umul_ppmm_distinct(X0H, X0L, X1H, X1L, MX0, MX1)            \
  do                                                                          \
    {                                                                         \
      asm("mlgr\t%[x0h],%[mx0]\n\t"                                           \
          "mlgr\t%[x1h],%[mx1]"                                               \
          : [x0h] "=&r"(X0H), [x0l] "=&r"(X0L), [x1h] "=r"(X1H),              \
            [x1l] "=r"(X1L)                                                   \
          : "[x0l]"(X0L), "[x1l]"(X1L), [mx0] "r"(MX0), [mx1] "r"(MX1));      \
    }                                                                         \
  while (0);

#define ASM_LOADGPR_BASE(DST, BASE, OFFSET)                                   \
  asm volatile("lg\t%[r],%[off](%[b])"                                        \
               : [r] "=r"(DST)                                                \
               : [b] "a"(BASE), [off] "L"(OFFSET)                             \
               : "memory");

#define ASM_LOADGPR(DST, BASE, INDEX, OFFSET)                                 \
  asm volatile("lg\t%[r],%[off](%[b],%[x])"                                   \
               : [r] "=r"(DST)                                                \
               : [b] "a"(BASE), [x] "a"(INDEX), [off] "L"(OFFSET)             \
               : "memory");

/*
 * Load a vector register from memory and swap the two 64-bit doubleword
 * elements.
 */
static inline vec_t
vec_load_elements_reversed_idx (mp_limb_t const *base, ssize_t const index,
                                ssize_t const offset)
{
  vec_t res;
  char *ptr = (char *)base;

  res.sw = *(v16qi *)(ptr + index + offset);
  res.dw = vec_permi (res.dw, res.dw, 2);

  return res;
}

static inline vec_t
vec_load_elements_reversed (mp_limb_t const *base, ssize_t const offset)
{
  return vec_load_elements_reversed_idx (base, 0, offset);
}

/*
 * Store a vector register to memory and swap the two 64-bit doubleword
 * elements.
 */
static inline void
vec_store_elements_reversed_idx (mp_limb_t *base, ssize_t const index,
                                 ssize_t const offset, vec_t vec)
{
  char *ptr = (char *)base;

  vec.dw = vec_permi (vec.dw, vec.dw, 2);
  *(v16qi *)(ptr + index + offset) = vec.sw;
}

static inline void
vec_store_elements_reversed (mp_limb_t *base, ssize_t const offset, vec_t vec)
{
  vec_store_elements_reversed_idx (base, 0, offset, vec);
}

#define ASM_VZERO(VEC)                                                        \
  do                                                                          \
    {                                                                         \
      asm("vzero\t%[vec]" : [vec] "=v"(VEC));                                 \
    }                                                                         \
  while (0)

#endif
