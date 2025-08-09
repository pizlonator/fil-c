/* Copyright (C) 2007 Dan Lin, Robert D. Cameron
   Licensed to International Characters Inc. and Simon Fraser University
              under the Academic Free License version 3.0.
   Licensed to the public under the Open Software License version 3.0.
*/

/*------------------------------------------------------------*/

#include <mmintrin.h>
typedef __m64 SIMD_type;


/*------------------------------------------------------------*/
/* I. SIMD bitwise logical operations */

#define simd_or(b1, b2) _mm_or_si64(b1, b2)
#define simd_and(b1, b2) _mm_and_si64(b1, b2)
#define simd_xor(b1, b2) _mm_xor_si64(b1, b2)
#define simd_andc(b1, b2) _mm_andnot_si64(b2, b1)
#define simd_if(cond, then_val, else_val) \
  simd_or(simd_and(then_val, cond), simd_andc(else_val, cond))
#define simd_not(b) (~b)
#define simd_nor(a,b) (~simd_or(a,b))


/*  Specific constants. */
#define simd_himask_2 _mm_set1_pi8(0xAA)
#define simd_himask_4 _mm_set1_pi8(0xCC)
#define simd_himask_8 _mm_set1_pi8(0xF0)
#define simd_himask_16 _mm_set1_pi16(0xFF00)
#define simd_himask_32 _mm_set1_pi32(0xFFFF0000)
#define simd_himask_64 _mm_set_pi32(-1,0)
/*  Assigned constants. */
#define simd_const_32(n) _mm_set1_pi32(n)
#define simd_const_16(n) _mm_set1_pi16(n)
#define simd_const_8(n) _mm_set1_pi8(n)
#define simd_const_4(n) _mm_set1_pi8((n)<<4|(n))
#define simd_const_2(n) simd_const_4((n)<<2|(n))
#define simd_const_1(n) simd_const_2((n)<<1|(n))

/*  Operations: add, subtract, multiply, shift, merge, pack
               in different field width*/

#define simd_add_8(a, b) _mm_add_pi8(a, b)
#define simd_add_16(a, b) _mm_add_pi16(a, b)
#define simd_add_32(a, b) _mm_add_pi32(a, b)
#define simd_add_64(a, b) _mm_add_si64(a, b)
#define simd_sub_8(a, b) _mm_sub_pi8(a, b)
#define simd_sub_16(a, b) _mm_sub_pi16(a, b)
#define simd_sub_32(a, b) _mm_sub_pi32(a, b)
#define simd_sub_64(a, b) _mm_sub_si64(a, b)
#define simd_mult_16(a, b) _mm_mullo_pi16(a, b)
#define simd_slli_16(r, shft) _mm_slli_pi16(r, shft)
#define simd_srli_16(r, shft) _mm_srli_pi16(r, shft)
#define simd_srai_16(r, shft) _mm_srai_pi16(r, shft)
#define simd_slli_32(r, shft) _mm_slli_pi32(r, shft)
#define simd_srli_32(r, shft) _mm_srli_pi32(r, shft)
#define simd_srai_32(r, shft) _mm_srai_pi32(r, shft)
#define simd_slli_64(r, shft) _mm_slli_si64(r, shft)
#define simd_srli_64(r, shft) _mm_srli_si64(r, shft)
#define simd_sll_64(r, shft_reg) _mm_sll_si64(r, shft_reg)
#define simd_srl_64(r, shft_reg) _mm_srl_si64(r, shft_reg)
#define simd_mergeh_8(a, b) _mm_unpackhi_pi8(b, a)
#define simd_mergeh_16(a, b) _mm_unpackhi_pi16(b, a)
#define simd_mergeh_32(a, b) _mm_unpackhi_pi32(b, a)
#define simd_mergel_8(a, b) _mm_unpacklo_pi8(b, a)
#define simd_mergel_16(a, b) _mm_unpacklo_pi16(b, a)
#define simd_mergel_32(a, b) _mm_unpacklo_pi32(b, a)
#define simd_pack_16(a, b) \
  _mm_packs_pu16(simd_andc(b, simd_himask_16), simd_andc(a, simd_himask_16))
static inline SIMD_type simd_pack_32(SIMD_type a, SIMD_type b)
{
   SIMD_type a1, b1;
   asm volatile("pshufw $8, %[a], %[a1]\n\t"
                : [a1] "=y" (a1)
                : [a] "y" (a));
   asm volatile("pshufw $8, %[b], %[b1]\n\t"
		: [b1] "=y" (b1)
		: [b] "y" (b));
   /* a1 = _mm_shuffle_pi16(a,8);
      b1 = _mm_shuffle_pi16(b,8); */
   return simd_mergel_32(a1,b1);
}
#define simd_pack_64(a, b) \
  simd_mergel_32_(a, b)
#define simd_eq_8(a, b) _mm_cmpeq_pi8(a, b)
#define simd_eq_16(a, b) _mm_cmpeq_pi16(a, b)
#define simd_eq_32(a, b) _mm_cmpeq_pi32(a, b)

/*  Full block operations */
#define sisd_sll(r, shft) simd_sll_64(r, shft)
#define sisd_srl(r, shft) simd_srl_64(r, shft)
#define sisd_slli(r, shft) simd_slli_64(r, shft)
#define sisd_srli(r, shft) simd_srli_64(r, shft)
#define sisd_add(a, b) simd_add_64(a, b)
#define sisd_sub(a, b) simd_sub_64(a, b)


#define sisd_store_aligned(r, addr) *((SIMD_type *) (addr)) = r
#define sisd_store_unaligned(r, addr) *((SIMD_type *) (addr)) = r
#define sisd_load_aligned(addr) ((SIMD_type) *((SIMD_type *) (addr)))
#define sisd_load_unaligned(addr) ((SIMD_type) *((SIMD_type *) (addr)))

#define sisd_to_int(x) _mm_cvtsi64_si32(x)
#define sisd_from_int(n) _mm_cvtsi32_si64(n)














