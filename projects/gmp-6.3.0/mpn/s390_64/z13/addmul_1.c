/* Addmul_1 / mul_1 for IBM z13 and later
   Contributed by Marius Hillenbrand

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
#  ifdef OPERATION_addmul_1
#    define ADD
#    define FUNCNAME inline_addmul_1
#  elif defined(OPERATION_mul_1)
#    define FUNCNAME inline_mul_1
#  endif

#else
#  ifdef OPERATION_addmul_1
#    define ADD
#    define FUNCNAME mpn_addmul_1
#  elif defined(OPERATION_mul_1)
#    define FUNCNAME mpn_mul_1
#  endif
#endif

#ifdef DO_INLINE
static inline mp_limb_t
FUNCNAME (mp_ptr rp, mp_srcptr s1p, mp_size_t n, mp_limb_t s2limb)
    __attribute__ ((always_inline));

static inline
#endif
mp_limb_t
FUNCNAME (mp_ptr rp, mp_srcptr s1p, mp_size_t n, mp_limb_t s2limb)
{
  ASSERT (n >= 1);
  ASSERT (MPN_SAME_OR_INCR_P(rp, s1p, n));

  /* Combine 64x64 multiplication into GPR pairs (MLGR) with 128-bit adds in
     VRs (using each VR as a single 128-bit accumulator).
     The inner loop is unrolled to four limbs, with two blocks of four
     multiplications each. Since the MLGR operation operates on even/odd GPR
     pairs, pin the products appropriately. */

  /* products as GPR pairs */
  register mp_limb_t p0_high asm("r0");
  register mp_limb_t p0_low asm("r1");

  register mp_limb_t p1_high asm("r8");
  register mp_limb_t p1_low asm("r9");

  register mp_limb_t p2_high asm("r6");
  register mp_limb_t p2_low asm("r7");

  register mp_limb_t p3_high asm("r10");
  register mp_limb_t p3_low asm("r11");

  /* carry flag for 128-bit add in VR for first carry chain */
  vec_t carry_vec0 = { .dw = vec_splat_u64 (0) };
  mp_limb_t carry_limb = 0;

#ifdef ADD
  /* 2nd carry flag for 2nd carry chain with addmul */
  vec_t carry_vec1 = { .dw = vec_splat_u64 (0) };
  vec_t sum0;
  vec_t rp0_addend, rp1_addend;
  rp0_addend.dw = vec_splat_u64 (0);
  rp1_addend.dw = vec_splat_u64 (0);
#endif
  vec_t sum1;

  vec_t carry_prod = { .dw = vec_splat_u64 (0) };

  /* The scalar multiplications compete with pointer and index increments for
   * issue ports. Thus, increment the loop index in the middle of the loop so
   * that the operations for the next iteration's multiplications can be
   * loaded in time (looks horrible, yet helps performance) and make sure we
   * use addressing with base reg + index reg + immediate displacement
   * (so that only the single index needs incrementing, instead of multiple
   * pointers). */
#undef LOOP_ADVANCE
#undef IDX_OFFSET

#define LOOP_ADVANCE 4 * sizeof (mp_limb_t)
#define IDX_OFFSET (LOOP_ADVANCE)
  register ssize_t idx = 0 - IDX_OFFSET;

  /*
   * branch-on-count implicitly hint to the branch prediction as taken, while
   * compare-and-branch hints as not taken. currently, using branch-on-count
   * has a performance advantage, but it is not clear that it is generally the
   * better choice (e.g., branch-on-count requires decrementing the separate
   * counter). so, allow switching the loop condition to enable either
   * category of branch instructions:
   * - idx is less than an upper bound, for compare-and-branch
   * - iteration counter greater than zero, for branch-on-count
   */
#define BRCTG
#ifdef BRCTG
  ssize_t iterations = (size_t)n / 4;
#else
  ssize_t const idx_bound = n * sizeof (mp_limb_t) - IDX_OFFSET;
#endif

  /* products will be transferred into VRs before adding up.
   * see main loop below for comments on accumulation scheme. */
  vec_t product0, product1, product2;

  product0.dw = vec_splat_u64 (0);

  switch ((size_t)n % 4)
    {
    case 0:
      break;

    case 1:
      idx = 1 * sizeof (mp_limb_t) - IDX_OFFSET;

      p3_low = s1p[0];
      s390_umul_ppmm (p3_high, p3_low, s2limb);

#ifdef ADD
      rp0_addend.dw[1] = rp[0];
      product0.dw[1] = p3_low;

      sum0.sw = vec_add_u128 (product0.sw, rp0_addend.sw);
      carry_vec1.dw = vec_permi (sum0.dw, sum0.dw, 0);

      rp[0] = sum0.dw[1];
#else
      rp[0] = p3_low;
#endif

      carry_limb = p3_high;
      break;

    case 2:
      p0_low = s1p[0];
      p3_low = s1p[1];
      idx = 2 * sizeof (mp_limb_t) - IDX_OFFSET;

      s390_double_umul_ppmm (p0_high, p0_low, p3_high, p3_low, s2limb);

      carry_prod.dw[0] = p3_low;

      product0.dw = vec_load_2di_as_pair (p0_high, p0_low);

      carry_limb = p3_high;

#ifdef ADD
      rp0_addend = vec_load_elements_reversed (rp, 0);
      sum0.sw = vec_add_u128 (carry_prod.sw, rp0_addend.sw);
      carry_vec0.sw = vec_addc_u128 (carry_prod.sw, rp0_addend.sw);

      sum1.sw = vec_add_u128 (sum0.sw, product0.sw);
      carry_vec1.sw = vec_addc_u128 (sum0.sw, product0.sw);
#else
      sum1.sw = vec_add_u128 (carry_prod.sw, product0.sw);
      carry_vec0.sw = vec_addc_u128 (carry_prod.sw, product0.sw);
#endif

      vec_store_elements_reversed (rp, 0, sum1);

      break;

    case 3:
      idx = 3 * sizeof (mp_limb_t) - IDX_OFFSET;

      p0_low = s1p[0];
      s390_umul_ppmm (p0_high, p0_low, s2limb);

#ifdef ADD
      rp0_addend.dw[1] = rp[0];
      product0.dw[1] = p0_low;

      sum0.sw = vec_add_u128 (product0.sw, rp0_addend.sw);
      carry_vec1.dw = vec_permi (sum0.dw, sum0.dw, 0);

      rp[0] = sum0.dw[1];
#else
      rp[0] = p0_low;
#endif
      carry_limb = p0_high;

      p1_low = s1p[1];
      p3_low = s1p[2];

      s390_double_umul_ppmm (p1_high, p1_low, p3_high, p3_low, s2limb);

      carry_prod.dw = vec_load_2di_as_pair (p3_low, carry_limb);
      product1.dw = vec_load_2di_as_pair (p1_high, p1_low);
      carry_limb = p3_high;

#ifdef ADD
      rp0_addend = vec_load_elements_reversed (rp, 8);
      sum0.sw = vec_add_u128 (carry_prod.sw, rp0_addend.sw);
      carry_vec0.sw = vec_addc_u128 (carry_prod.sw, rp0_addend.sw);

      sum1.sw = vec_adde_u128 (sum0.sw, product1.sw, carry_vec1.sw);
      carry_vec1.sw = vec_addec_u128 (sum0.sw, product1.sw, carry_vec1.sw);
#else
      sum1.sw = vec_adde_u128 (carry_prod.sw, product1.sw, carry_vec0.sw);
      carry_vec0.sw
          = vec_addec_u128 (carry_prod.sw, product1.sw, carry_vec0.sw);
#endif
      vec_store_elements_reversed (rp, 8, sum1);
      break;
    }

#ifdef BRCTG
  for (; iterations > 0; iterations--)
    {
#else
  while (idx < idx_bound)
    {
#endif
      vec_t overlap_addend0;
      vec_t overlap_addend1;

      /* The 64x64->128 MLGR multiplies two factors in GPRs and stores the
       * result in a GPR pair. One of the factors is taken from the GPR pair
       * and overwritten.
       * To reuse factors, it turned out cheaper to load limbs multiple times
       * than copying GPR contents. Enforce that and the use of addressing by
       * base + index gpr + immediate displacement via inline asm.
       */
      ASM_LOADGPR (p0_low, s1p, idx, 0 + IDX_OFFSET);
      ASM_LOADGPR (p1_low, s1p, idx, 8 + IDX_OFFSET);
      ASM_LOADGPR (p2_low, s1p, idx, 16 + IDX_OFFSET);
      ASM_LOADGPR (p3_low, s1p, idx, 24 + IDX_OFFSET);

      /*
       * accumulate products as follows (for addmul):
       *                       | rp[i+3] | rp[i+2] | rp[i+1] | rp[i]   |
       *                                             p0_high | p0_low  |
       *                                   p1_high | p1_low  | carry-limb in
       *                         p2_high | p2_low  |
       * c-limb out <- p3_high | p3_low  |
       *                       | <    128-bit VR   > <   128-bit VR    >
       *
       *                         <   rp1_addend    > <  rp0_addend     >
       *     carry-chain 0  <-   +           <-      +  <- carry_vec0[127]
       *                         <   product1      > <  product0       >
       *     carry-chain 1  <-   +           <-      +  <- carry_vec1[127]
       *                         < overlap_addend1 > < overlap_addend0 >
       *
       * note that a 128-bit add with carry in + out is built from two insns
       * - vec_adde_u128 (vacq) provides sum
       * - vec_addec_u128 (vacccq) provides the new carry bit
       */

      s390_double_umul_ppmm (p0_high, p0_low, p1_high, p1_low, s2limb);

      /*
       * "barrier" to enforce scheduling loads for all limbs and first round
       * of MLGR before anything else.
       */
      asm volatile("");

      product0.dw = vec_load_2di_as_pair (p0_high, p0_low);

#ifdef ADD
      rp0_addend = vec_load_elements_reversed_idx (rp, idx, 0 + IDX_OFFSET);
      rp1_addend = vec_load_elements_reversed_idx (rp, idx, 16 + IDX_OFFSET);
#endif
      /* increment loop index to unblock dependant loads of limbs for the next
       * iteration (see above at #define LOOP_ADVANCE) */
      idx += LOOP_ADVANCE;

      s390_double_umul_ppmm (p2_high, p2_low, p3_high, p3_low, s2limb);

      overlap_addend0.dw = vec_load_2di_as_pair (p1_low, carry_limb);
      asm volatile("");

#ifdef ADD
      sum0.sw = vec_adde_u128 (product0.sw, rp0_addend.sw, carry_vec0.sw);
      sum1.sw = vec_adde_u128 (sum0.sw, overlap_addend0.sw, carry_vec1.sw);

      carry_vec0.sw
          = vec_addec_u128 (product0.sw, rp0_addend.sw, carry_vec0.sw);
      carry_vec1.sw
          = vec_addec_u128 (sum0.sw, overlap_addend0.sw, carry_vec1.sw);
#else
      sum1.sw = vec_adde_u128 (product0.sw, overlap_addend0.sw, carry_vec0.sw);
      carry_vec0.sw
          = vec_addec_u128 (product0.sw, overlap_addend0.sw, carry_vec0.sw);
#endif

      asm volatile("");
      product2.dw = vec_load_2di_as_pair (p2_high, p2_low);
      overlap_addend1.dw = vec_load_2di_as_pair (p3_low, p1_high);

      vec_t sum4;

#ifdef ADD
      vec_t sum3;
      sum3.sw = vec_adde_u128 (product2.sw, rp1_addend.sw, carry_vec0.sw);
      sum4.sw = vec_adde_u128 (sum3.sw, overlap_addend1.sw, carry_vec1.sw);

      carry_vec0.sw
          = vec_addec_u128 (product2.sw, rp1_addend.sw, carry_vec0.sw);
      carry_vec1.sw
          = vec_addec_u128 (sum3.sw, overlap_addend1.sw, carry_vec1.sw);
#else
      sum4.sw = vec_adde_u128 (product2.sw, overlap_addend1.sw, carry_vec0.sw);
      carry_vec0.sw
          = vec_addec_u128 (product2.sw, overlap_addend1.sw, carry_vec0.sw);
#endif
      vec_store_elements_reversed_idx (rp, idx, IDX_OFFSET - LOOP_ADVANCE,
                                       sum1);
      vec_store_elements_reversed_idx (rp, idx, 16 + IDX_OFFSET - LOOP_ADVANCE,
                                       sum4);

      carry_limb = p3_high;
    }

#ifdef ADD
  carry_vec0.dw += carry_vec1.dw;
  carry_limb += carry_vec0.dw[1];
#else
  carry_limb += carry_vec0.dw[1];
#endif

  return carry_limb;
}

#undef OPERATION_addmul_1
#undef OPERATION_mul_1
#undef FUNCNAME
#undef ADD
