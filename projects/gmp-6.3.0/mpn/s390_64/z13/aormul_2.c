/* Addmul_2 / mul_2 for IBM z13 or later

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

#include "gmp-impl.h"

#include "s390_64/z13/common-vec.h"

#undef FUNCNAME

#ifdef DO_INLINE
#  ifdef OPERATION_addmul_2
#    define ADD
#    define FUNCNAME inline_addmul_2
#  elif defined(OPERATION_mul_2)
#    define FUNCNAME inline_mul_2
#  else
#    error Missing define for operation to perform
#  endif
#else
#  ifdef OPERATION_addmul_2
#    define ADD
#    define FUNCNAME mpn_addmul_2
#  elif defined(OPERATION_mul_2)
#    define FUNCNAME mpn_mul_2
#  else
#    error Missing define for operation to perform
#  endif
#endif

#ifdef DO_INLINE
static inline mp_limb_t
FUNCNAME (mp_limb_t *rp, const mp_limb_t *up, mp_size_t n, const mp_limb_t *vp)
    __attribute__ ((always_inline));

static inline
#endif
mp_limb_t
FUNCNAME (mp_limb_t *rp, const mp_limb_t *up, mp_size_t n,
          const mp_limb_t *vp)
{

  /* Combine 64x64 multiplication into GPR pairs (MLGR) with 128-bit adds in
     VRs (using each VR as a single 128-bit accumulator).
     The inner loop is unrolled to four limbs, with two blocks of four
     multiplications each. Since the MLGR operation operates on even/odd GPR
     pairs, pin the products appropriately. */

  register mp_limb_t p0_high asm("r0");
  register mp_limb_t p0_low asm("r1");

  register mp_limb_t p1_high asm("r8");
  register mp_limb_t p1_low asm("r9");

  register mp_limb_t p2_high asm("r6");
  register mp_limb_t p2_low asm("r7");

  register mp_limb_t p3_high asm("r10");
  register mp_limb_t p3_low asm("r11");

  vec_t carry_prod = { .dw = vec_splat_u64 (0) };
  vec_t zero = { .dw = vec_splat_u64 (0) };

  /* two carry-bits for the 128-bit VR adds - stored in VRs */
#ifdef ADD
  vec_t carry_vec0 = { .dw = vec_splat_u64 (0) };
#endif
  vec_t carry_vec1 = { .dw = vec_splat_u64 (0) };

  vec_t tmp;

  vec_t sum0, sum1;

  /* products transferred into VRs for accumulating there */
  vec_t pv0, pv3;
  vec_t pv1_low, pv1_high, pv2_low, pv2_high;
  vec_t low, middle, high;
#ifdef ADD
  vec_t rp0, rp1;
#endif

  register mp_limb_t v0 asm("r12");
  register mp_limb_t v1 asm("r5");
  v0 = vp[0];
  v1 = vp[1];

  /* The scalar multiplications compete with pointer and index increments for
   * issue ports. Thus, increment the loop index in the middle of the loop so
   * that the operations for the next iteration's multiplications can be
   * loaded in time (looks horrible, yet helps performance) and make sure we
   * use addressing with base reg + index reg + immediate displacement
   * (so that only the single index needs incrementing, instead of multiple
   * pointers). */
#undef LOOP_ADVANCE
#define LOOP_ADVANCE (4 * sizeof (mp_limb_t))
#define IDX_OFFSET (LOOP_ADVANCE)

  register ssize_t idx = 0 - IDX_OFFSET;
#ifdef BRCTG
  ssize_t iterations = (size_t)n / 4;
#else
  ssize_t const idx_bound = n * sizeof (mp_limb_t) - IDX_OFFSET;
#endif

  /*
   * To minimize latency in the carry chain, accumulate in VRs with 128-bit
   * adds with carry in and out. As a downside, these require two insns for
   * each add - one to calculate the sum, one to deliver the carry out.
   * To reduce the overall number of insns to execute, combine adding up
   * product limbs such that there cannot be a carry out and one (for mul) or
   * two (for addmul) adds with carry chains.
   *
   * Since (2^64-1) * (2^64-1) = (2^128-1) - 2 * (2^64-1), we can add two
   * limbs into each 128-bit product without causing carry out.
   *
   * For each block of 2 limbs * 2 limbs
   *
   *                             |  u[i] * v[0] (p2) |
   *                   |  u[i] * v[1] (p0) |
   *                   | u[i+1] * v[0](p1) |
   *         | u[i+1] * v[1](p3) |
   *         <     128 bits     > <    128 bits      >
   *
   * we can begin accumulating with "simple" carry-oblivious 128-bit adds:
   * - p0 + low limb of p1
   *      + high limb of p2
   *      and combine resulting low limb with p2's low limb
   * - p3 + high limb of p1
   *      + high limb of sum above
   * ... which will will result in two 128-bit limbs to be fed into the carry
   * chain(s).
   * Overall, that scheme saves instructions and improves performance, despite
   * slightly increasing latency between multiplications and carry chain (yet
   * not in the carry chain).
   */

#define LOAD_LOW_LIMB(VEC, LIMB)                                              \
  do                                                                          \
    {                                                                         \
      asm("vzero\t%[vec]\n\t"                                                 \
          "vlvgg\t%[vec],%[limb],1"                                           \
          : [vec] "=v"(VEC)                                                   \
          : [limb] "r"(LIMB));                                                \
    }                                                                         \
  while (0)

  /* for the 128-bit adds in the carry chain, to calculate a + b + carry-in we
   * need paired vec_adde_u128 (delivers sum) and vec_addec_u128 (delivers new
   * carry) */
#define ADD_UP2_CARRY_INOUT(SUMIDX, CARRYIDX, ADDEND1, ADDEND2)               \
  do                                                                          \
    {                                                                         \
      sum##SUMIDX.sw                                                          \
          = vec_adde_u128 (ADDEND1.sw, ADDEND2.sw, carry_vec##CARRYIDX.sw);   \
      carry_vec##CARRYIDX.sw                                                  \
          = vec_addec_u128 (ADDEND1.sw, ADDEND2.sw, carry_vec##CARRYIDX.sw);  \
    }                                                                         \
  while (0)

#define ADD_UP_CARRY_INOUT(SUMIDX, ADDEND1, ADDEND2)                          \
  ADD_UP2_CARRY_INOUT (SUMIDX, SUMIDX, ADDEND1, ADDEND2)

  /* variant without carry-in for prologue */
#define ADD_UP2_CARRY_OUT(SUMIDX, CARRYIDX, ADDEND1, ADDEND2)                 \
  do                                                                          \
    {                                                                         \
      sum##SUMIDX.sw = vec_add_u128 (ADDEND1.sw, ADDEND2.sw);                 \
      carry_vec##CARRYIDX.sw = vec_addc_u128 (ADDEND1.sw, ADDEND2.sw);        \
    }                                                                         \
  while (0)

#define ADD_UP_CARRY_OUT(SUMIDX, ADDEND1, ADDEND2)                            \
  ADD_UP2_CARRY_OUT (SUMIDX, SUMIDX, ADDEND1, ADDEND2)

  /* prologue for 4x-unrolled main loop */
  switch ((size_t)n % 4)
    {
    case 1:
      ASM_LOADGPR_BASE (p0_low, up, 0);
      ASM_LOADGPR_BASE (p1_low, up, 0);
      s390_double_umul_ppmm_distinct (p0_high, p0_low, p1_high, p1_low, v0, v1);
      carry_prod.dw = vec_load_2di_as_pair (p1_high, p1_low);

/* gcc tries to be too clever and vlr from a reg that is already zero. vzero is
 * cheaper. */
#  define NEW_CARRY(VEC, LIMB)                                                \
    do                                                                        \
      {                                                                       \
        asm("vzero\t%[vec]\n\t"                                               \
            "vlvgg\t%[vec],%[limb],1"                                         \
            : [vec] "=v"(VEC)                                                 \
            : [limb] "r"(LIMB));                                              \
      }                                                                       \
    while (0)

      NEW_CARRY (tmp, p0_high);

      carry_prod.sw = vec_add_u128 (carry_prod.sw, tmp.sw);
#ifdef ADD
      carry_vec1.dw[1] = __builtin_add_overflow (rp[0], p0_low, rp);
#else
      rp[0] = p0_low;
#endif
      idx += sizeof (mp_limb_t);
      break;

    case 2:
      ASM_LOADGPR_BASE (p0_low, up, 0);
      ASM_LOADGPR_BASE (p1_low, up, 8);
      ASM_LOADGPR_BASE (p2_low, up, 0);
      ASM_LOADGPR_BASE (p3_low, up, 8);

      asm(""
          : "=r"(p0_low), "=r"(p2_low)
          : "r"(p3_low), "0"(p0_low), "r"(p1_low), "1"(p2_low));
      s390_double_umul_ppmm_distinct (p0_high, p0_low, p1_high, p1_low, v1, v0);
      s390_double_umul_ppmm_distinct (p2_high, p2_low, p3_high, p3_low, v0, v1);

      pv0.dw = vec_load_2di_as_pair (p0_high, p0_low);
      LOAD_LOW_LIMB (pv1_low, p1_low);
      LOAD_LOW_LIMB (pv1_high, p1_high);
      pv0.sw = vec_add_u128 (pv0.sw, pv1_low.sw);
      LOAD_LOW_LIMB (pv2_high, p2_high);
      pv3.dw = vec_load_2di_as_pair (p3_high, p3_low);
      LOAD_LOW_LIMB (pv2_low, p2_low);
      pv3.sw = vec_add_u128 (pv3.sw, pv1_high.sw);
      middle.sw = vec_add_u128 (pv0.sw, pv2_high.sw);
      low.dw = vec_permi (middle.dw, pv2_low.dw, 3);
      middle.dw = vec_permi (zero.dw, middle.dw, 0);
      high.sw = vec_add_u128 (middle.sw, pv3.sw);
#ifdef ADD
      rp0 = vec_load_elements_reversed (rp, 0);
      ADD_UP_CARRY_OUT (0, rp0, carry_prod);
#else
      sum0 = carry_prod;
#endif
      ADD_UP_CARRY_OUT (1, sum0, low);
      vec_store_elements_reversed (rp, 0, sum1);
      carry_prod = high;

      idx += 2 * sizeof (mp_limb_t);
      break;

    case 3:
      ASM_LOADGPR_BASE (p0_low, up, 0);
      ASM_LOADGPR_BASE (p1_low, up, 0);
      s390_double_umul_ppmm_distinct (p0_high, p0_low, p1_high, p1_low, v0, v1);
      carry_prod.dw = vec_load_2di_as_pair (p1_high, p1_low);
      NEW_CARRY (tmp, p0_high);
      carry_prod.sw = vec_add_u128 (carry_prod.sw, tmp.sw);

#ifdef ADD
      carry_vec1.dw[1] = __builtin_add_overflow (rp[0], p0_low, rp);
#else
      rp[0] = p0_low;
#endif

      ASM_LOADGPR_BASE (p0_low, up, 8);
      ASM_LOADGPR_BASE (p1_low, up, 16);
      ASM_LOADGPR_BASE (p2_low, up, 8);
      ASM_LOADGPR_BASE (p3_low, up, 16);

      asm(""
          : "=r"(p0_low), "=r"(p2_low)
          : "r"(p3_low), "0"(p0_low), "r"(p1_low), "1"(p2_low));
      s390_double_umul_ppmm_distinct (p0_high, p0_low, p1_high, p1_low, v1, v0);
      s390_double_umul_ppmm_distinct (p2_high, p2_low, p3_high, p3_low, v0, v1);

      pv0.dw = vec_load_2di_as_pair (p0_high, p0_low);

      LOAD_LOW_LIMB (pv1_low, p1_low);
      LOAD_LOW_LIMB (pv1_high, p1_high);

      pv0.sw = vec_add_u128 (pv0.sw, pv1_low.sw);
      LOAD_LOW_LIMB (pv2_high, p2_high);
      pv3.dw = vec_load_2di_as_pair (p3_high, p3_low);

      LOAD_LOW_LIMB (pv2_low, p2_low);

      pv3.sw = vec_add_u128 (pv3.sw, pv1_high.sw);
      middle.sw = vec_add_u128 (pv0.sw, pv2_high.sw);

      low.dw = vec_permi (middle.dw, pv2_low.dw, 3);
      middle.dw = vec_permi (zero.dw, middle.dw, 0);
      high.sw = vec_add_u128 (middle.sw, pv3.sw);

#ifdef ADD
      vec_t rp0 = vec_load_elements_reversed (rp, 8);
      ADD_UP_CARRY_OUT (0, rp0, carry_prod);
#else
      sum0 = carry_prod;
#endif
      ADD_UP_CARRY_INOUT (1, sum0, low);

      vec_store_elements_reversed (rp, 8, sum1);

      carry_prod = high;

      idx += 3 * sizeof (mp_limb_t);
      break;
    }

    /*
     * branch-on-count implicitly hint to the branch prediction as taken, while
     * compare-and-branch hints as not taken. currently, using branch-on-count
     * has a performance advantage, but it is not clear that it is generally
     * the better choice (e.g., branch-on-count requires decrementing the
     * separate counter). so, allow switching the loop condition to enable
     * either category of branch instructions:
     * - idx is less than an upper bound, for compare-and-branch
     * - iteration counter greater than zero, for branch-on-count
     */
#ifdef BRCTG
  for (; iterations > 0; iterations--)
    {
#else
  while (idx < idx_bound)
    {
#endif
      /* The 64x64->128 MLGR multiplies two factors in GPRs and stores the
       * result in a GPR pair. One of the factors is taken from the GPR pair
       * and overwritten.
       * To reuse factors, it turned out cheaper to load limbs multiple times
       * than copying GPR contents. Enforce that and the use of addressing by
       * base + index gpr + immediate displacement via inline asm.
       */
      ASM_LOADGPR (p0_low, up, idx, 0 + IDX_OFFSET);
      ASM_LOADGPR (p1_low, up, idx, 8 + IDX_OFFSET);
      ASM_LOADGPR (p2_low, up, idx, 0 + IDX_OFFSET);
      ASM_LOADGPR (p3_low, up, idx, 8 + IDX_OFFSET);

      s390_double_umul_ppmm_distinct (p0_high, p0_low, p1_high, p1_low, v1, v0);

      pv0.dw = vec_load_2di_as_pair (p0_high, p0_low);

      LOAD_LOW_LIMB (pv1_low, p1_low);
      LOAD_LOW_LIMB (pv1_high, p1_high);

      s390_double_umul_ppmm_distinct (p2_high, p2_low, p3_high, p3_low, v0, v1);

      pv0.sw = vec_add_u128 (pv0.sw, pv1_low.sw);
      LOAD_LOW_LIMB (pv2_high, p2_high);
      pv3.dw = vec_load_2di_as_pair (p3_high, p3_low);

      LOAD_LOW_LIMB (pv2_low, p2_low);

      ASM_LOADGPR (p0_low, up, idx, 16 + IDX_OFFSET);
      ASM_LOADGPR (p1_low, up, idx, 24 + IDX_OFFSET);
      ASM_LOADGPR (p2_low, up, idx, 16 + IDX_OFFSET);
      ASM_LOADGPR (p3_low, up, idx, 24 + IDX_OFFSET);

      idx += LOOP_ADVANCE;

      /*
       * "barrier" to enforce scheduling the index increment before the second
       * block of multiplications. not required for clang.
       */
#ifndef __clang__
      asm(""
          : "=r"(idx), "=r"(p0_high), "=r"(p2_high)
          : "0"(idx), "1"(p0_high), "2"(p2_high));
#endif

      s390_double_umul_ppmm_distinct (p0_high, p0_low, p1_high, p1_low, v1, v0);
      s390_double_umul_ppmm_distinct (p2_high, p2_low, p3_high, p3_low, v0, v1);

      /*
       * "barrier" to enforce scheduling all MLGRs first, before any adding
       * up. note that clang produces better code without.
       */
#ifndef __clang__
      asm(""
          : "=v"(pv0.sw), "=v"(pv3.sw)
          : "1"(pv3.sw), "0"(pv0.sw), "r"(p0_high), "r"(p2_high));
#endif

      pv3.sw = vec_add_u128 (pv3.sw, pv1_high.sw);
      middle.sw = vec_add_u128 (pv0.sw, pv2_high.sw);

      low.dw = vec_permi (middle.dw, pv2_low.dw,
                          3); /* least-significant doubleword from both vectors */
      middle.dw = vec_permi (zero.dw, middle.dw, 0);
      high.sw = vec_add_u128 (middle.sw, pv3.sw);

#ifdef ADD
      rp0 = vec_load_elements_reversed_idx (rp, idx,
                                            0 + IDX_OFFSET - LOOP_ADVANCE);
      ADD_UP_CARRY_INOUT (0, rp0, carry_prod);
#else
      sum0 = carry_prod;
#endif
      ADD_UP_CARRY_INOUT (1, sum0, low);

      vec_store_elements_reversed_idx (rp, idx, 0 + IDX_OFFSET - LOOP_ADVANCE,
                                       sum1);

      carry_prod = high;

      vec_t pv0_2, pv3_2;
      vec_t pv1_low_2, pv1_high_2, pv2_low_2, pv2_high_2;
      vec_t low_2, middle_2, high_2;
      vec_t sum2, sum3;

      pv0_2.dw = vec_load_2di_as_pair (p0_high, p0_low);
      LOAD_LOW_LIMB (pv1_low_2, p1_low);
      LOAD_LOW_LIMB (pv1_high_2, p1_high);

      pv0_2.sw = vec_add_u128 (pv0_2.sw, pv1_low_2.sw);
      LOAD_LOW_LIMB (pv2_high_2, p2_high);
      pv3_2.dw = vec_load_2di_as_pair (p3_high, p3_low);
      pv3_2.sw = vec_add_u128 (pv3_2.sw, pv1_high_2.sw);
      middle_2.sw = vec_add_u128 (pv0_2.sw, pv2_high_2.sw);

      LOAD_LOW_LIMB (pv2_low_2, p2_low);
      low_2.dw
          = vec_permi (middle_2.dw, pv2_low_2.dw,
                       3); /* least-significant doubleword from both vectors */
      middle_2.dw = vec_permi (zero.dw, middle_2.dw, 0);
      high_2.sw = vec_add_u128 (middle_2.sw, pv3_2.sw);

      /*
       * another "barrier" to influence scheduling. (also helps in clang)
       */
      asm("" : : "v"(pv0_2.sw), "r"(p2_high), "r"(p3_high), "v"(pv3_2.sw));

#ifdef ADD
      rp1 = vec_load_elements_reversed_idx (rp, idx,
                                            16 + IDX_OFFSET - LOOP_ADVANCE);
      ADD_UP2_CARRY_INOUT (2, 0, rp1, carry_prod);
#else
      sum2 = carry_prod;
#endif
      ADD_UP2_CARRY_INOUT (3, 1, sum2, low_2);

      vec_store_elements_reversed_idx (rp, idx, 16 + IDX_OFFSET - LOOP_ADVANCE,
                                       sum3);

      carry_prod = high_2;
    }

#ifdef ADD
  sum0.sw = vec_adde_u128 (carry_prod.sw, carry_vec0.sw, carry_vec1.sw);
#else
  sum0.sw = vec_add_u128 (carry_prod.sw, carry_vec1.sw);
#endif

  *(mp_ptr) (((char *)rp) + idx + 0 + IDX_OFFSET) = (mp_limb_t)sum0.dw[1];

  return (mp_limb_t)sum0.dw[0];
}
