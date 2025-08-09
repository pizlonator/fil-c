/*  Idealized SIMD Operations with SSE versions
    Copyright (C) 2006, 2007, 2008, Robert D. Cameron
    Licensed to the public under the Open Software License 3.0.
    Licensed to International Characters Inc. 
       under the Academic Free License version 3.0.
*/
#ifndef SSE_SIMD_H
#define SSE_SIMD_H

/*------------------------------------------------------------*/
#ifndef _MSC_VER
#include <stdint.h>
#endif
#ifdef _MSC_VER
// Daniel Lemire (2022-05-17): the next few lines were slightly modified to get it to build under clangcl
#ifdef __clang__
#include <stdint.h>
#else
#include "stdint.h"
#endif

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif

#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif

#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

#endif
#include <limits.h>

#ifndef LONG_BIT
#if ULONG_MAX == 0xFFFFFFFF
#define LONG_BIT 32
#endif 
#if ULONG_MAX == 0xFFFFFFFFFFFFFFFF
#define LONG_BIT 64
#endif 
#endif

#include <emmintrin.h>
#ifdef USE_LDDQU
#include <pmmintrin.h>
#endif
typedef __m128i SIMD_type;



#ifdef SSSE3
#include <tmmintrin.h>

#define simd_permute(bytepack, indexes) _mm_shuffle_epi8(bytepack, indexes)
#endif

/*------------------------------------------------------------*/
/* I. SIMD bitwise logical operations */

#define simd_or(b1, b2) _mm_or_si128(b1, b2)
#define simd_and(b1, b2) _mm_and_si128(b1, b2)
#define simd_xor(b1, b2) _mm_xor_si128(b1, b2)
#define simd_andc(b1, b2) _mm_andnot_si128(b2, b1)
#define simd_if(cond, then_val, else_val) \
  simd_or(simd_and(then_val, cond), simd_andc(else_val, cond))
#define simd_not(b) (simd_xor(b, _mm_set1_epi32(0xFFFFFFFF)))
#define simd_nor(a,b) (simd_not(simd_or(a,b)))


/*  Specific constants. */
#define simd_himask_2 _mm_set1_epi32(0xAAAAAAAA)
#define simd_himask_4 _mm_set1_epi32(0xCCCCCCCC)
#define simd_himask_8 _mm_set1_epi32(0xF0F0F0F0)
/* Little-endian */
#define simd_himask_16 _mm_set1_epi32(0xFF00FF00)
#define simd_himask_32 _mm_set1_epi32(0xFFFF0000)
#define simd_himask_64 _mm_set_epi32(-1,0,-1,0)
#define simd_himask_128 _mm_set_epi32(-1,-1,0,0)

/* Idealized operations with direct implementation by built-in 
   operations for various target architectures. */

#define simd_add_8(a, b) _mm_add_epi8(a, b)
#define simd_add_16(a, b) _mm_add_epi16(a, b)
#define simd_add_32(a, b) _mm_add_epi32(a, b)
#define simd_add_64(a, b) _mm_add_epi64(a, b)
#define simd_sub_8(a, b) _mm_sub_epi8(a, b)
#define simd_sub_16(a, b) _mm_sub_epi16(a, b)
#define simd_sub_32(a, b) _mm_sub_epi32(a, b)
#define simd_sub_64(a, b) _mm_sub_epi64(a, b)
#define simd_mult_16(a, b) _mm_mullo_epi16(a, b)
#define simd_slli_16(r, shft) _mm_slli_epi16(r, shft)
#define simd_srli_16(r, shft) _mm_srli_epi16(r, shft)
#define simd_srai_16(r, shft) _mm_srai_epi16(r, shft)
#define simd_slli_32(r, shft) _mm_slli_epi32(r, shft)
#define simd_srli_32(r, shft) _mm_srli_epi32(r, shft)
#define simd_srai_32(r, shft) _mm_srai_epi32(r, shft)
#define simd_slli_64(r, shft) _mm_slli_epi64(r, shft)
#define simd_srli_64(r, shft) _mm_srli_epi64(r, shft)
#define simd_sll_64(r, shft_reg) _mm_sll_epi64(r, shft_reg)
#define simd_srl_64(r, shft_reg) _mm_srl_epi64(r, shft_reg)
#define simd_packus_16(a, b) _mm_packus_epi16(b, a)
#define simd_pack_16(a, b) \
  _mm_packus_epi16(simd_andc(b, simd_himask_16), simd_andc(a, simd_himask_16))
#define simd_mergeh_8(a, b) _mm_unpackhi_epi8(b, a)
#define simd_mergeh_16(a, b) _mm_unpackhi_epi16(b, a)
#define simd_mergeh_32(a, b) _mm_unpackhi_epi32(b, a)
#define simd_mergeh_64(a, b) _mm_unpackhi_epi64(b, a)
#define simd_mergel_8(a, b) _mm_unpacklo_epi8(b, a)
#define simd_mergel_16(a, b) _mm_unpacklo_epi16(b, a)
#define simd_mergel_32(a, b) _mm_unpacklo_epi32(b, a)
#define simd_mergel_64(a, b) _mm_unpacklo_epi64(b, a)
#define simd_eq_8(a, b) _mm_cmpeq_epi8(a, b)
#define simd_eq_16(a, b) _mm_cmpeq_epi16(a, b)
#define simd_eq_32(a, b) _mm_cmpeq_epi32(a, b)

#define simd_max_8(a, b) _mm_max_epu8(a, b)

#define simd_slli_128(r, shft) \
  ((shft) % 8 == 0 ? _mm_slli_si128(r, (shft)/8) : \
   (shft) >= 64 ? simd_slli_64(_mm_slli_si128(r, 8), (shft) - 64) : \
   simd_or(simd_slli_64(r, shft), _mm_slli_si128(simd_srli_64(r, 64-(shft)), 8)))

#define simd_srli_128(r, shft) \
  ((shft) % 8 == 0 ? _mm_srli_si128(r, (shft)/8) : \
   (shft) >= 64 ? simd_srli_64(_mm_srli_si128(r, 8), (shft) - 64) : \
   simd_or(simd_srli_64(r, shft), _mm_srli_si128(simd_slli_64(r, 64-(shft)), 8)))

#define simd_sll_128(r, shft) \
   simd_or(simd_sll_64(r, shft), \
           simd_or(_mm_slli_si128(simd_sll_64(r, simd_sub_32(shft, sisd_from_int(64))), 8), \
                   _mm_slli_si128(simd_srl_64(r, simd_sub_32(sisd_from_int(64), shft)), 8)))

#define simd_srl_128(r, shft) \
   simd_or(simd_srl_64(r, shft), \
           simd_or(_mm_srli_si128(simd_srl_64(r, simd_sub_32(shft, sisd_from_int(64))), 8), \
                   _mm_srli_si128(simd_sll_64(r, simd_sub_32(sisd_from_int(64), shft)), 8)))

#define sisd_sll(r, shft) simd_sll_128(r, shft)
#define sisd_srl(r, shft) simd_srl_128(r, shft)
#define sisd_slli(r, shft) simd_slli_128(r, shft)
#define sisd_srli(r, shft) simd_srli_128(r, shft)
#define sisd_add(a, b) simd_add_128(a, b)
#define sisd_sub(a, b) simd_sub_128(a, b)

#define sisd_store_aligned(r, addr) _mm_store_si128(addr, r)
#define sisd_store_unaligned(r, addr) _mm_storeu_si128(addr, r)
#define sisd_load_aligned(addr) _mm_load_si128(addr)
#ifndef USE_LDDQU
#define sisd_load_unaligned(addr) _mm_loadu_si128(addr)
#endif
#ifdef USE_LDDQU
#define sisd_load_unaligned(addr) _mm_lddqu_si128(addr)
#endif



#define simd_const_32(n) _mm_set1_epi32(n)
#define simd_const_16(n) _mm_set1_epi16(n)
#define simd_const_8(n) _mm_set1_epi8(n)
#define simd_const_4(n) _mm_set1_epi8((n)<<4|(n))
#define simd_const_2(n) simd_const_4((n)<<2|n)
#define simd_const_1(n) \
  (n==0 ? simd_const_8(0): simd_const_8(-1))

#define simd_pack_16_ll(a, b) simd_pack_16(a, b)
#define simd_pack_16_hh(a, b) \
  simd_pack_16(simd_srli_16(a, 8), simd_srli_16(b, 8))


static inline
SIMD_type simd_add_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_xor(a,b);
	 SIMD_type borrow = simd_and(a,b);
	 SIMD_type c2 = simd_xor(c1,(sisd_slli(borrow,1)));
	 return simd_if(simd_himask_2,c2,c1);
}
#define simd_add_4(a, b)\
	simd_if(simd_himask_8, simd_add_8(simd_and(a,simd_himask_8),simd_and(b,simd_himask_8))\
	,simd_add_8(simd_andc(a,simd_himask_8),simd_andc(b,simd_himask_8)))

#define simd_srli_2(r, sh)\
	 simd_and(simd_srli_32(r,sh),simd_const_2(3>>sh))

#define simd_srli_4(r, sh)\
	 simd_and(simd_srli_32(r,sh),simd_const_4(15>>sh))
#define simd_srli_8(r, sh)\
	 simd_and(simd_srli_32(r,sh),simd_const_8(255>>sh))

#define simd_slli_2(r, sh)\
	 simd_and(simd_slli_32(r,sh),simd_const_2((3<<sh)&3))

#define simd_slli_4(r, sh)\
	 simd_and(simd_slli_32(r,sh),simd_const_4((15<<sh)&15))
#define simd_slli_8(r, sh)\
	 (sh == 1 ? simd_add_8(r,r):\
          simd_and(simd_slli_32(r,sh),simd_const_8(char((255<<sh) &255))))




#define simd_mergeh_4(a,b)\
	simd_mergeh_8(simd_if(simd_himask_8,a,simd_srli_8(b,4)),\
        simd_if(simd_himask_8,simd_slli_8(a,4),b))
#define simd_mergel_4(a,b)\
	simd_mergel_8(simd_if(simd_himask_8,a,simd_srli_8(b,4)),\
        simd_if(simd_himask_8,simd_slli_8(a,4),b))
#define simd_mergeh_2(a,b)\
	simd_mergeh_4(simd_if(simd_himask_4,a,simd_srli_4(b,2)),\
	simd_if(simd_himask_4,simd_slli_4(a,2),b))
#define simd_mergel_2(a,b)\
	simd_mergel_4(simd_if(simd_himask_4,a,simd_srli_4(b,2)),\
	simd_if(simd_himask_4,simd_slli_4(a,2),b))
#define simd_mergeh_1(a,b)\
	simd_mergeh_2(simd_if(simd_himask_2,a,simd_srli_2(b,1)),\
	simd_if(simd_himask_2,simd_slli_2(a,1),b))
#define simd_mergel_1(a,b)\
	simd_mergel_2(simd_if(simd_himask_2,a,simd_srli_2(b,1)),\
	simd_if(simd_himask_2,simd_slli_2(a,1),b))

#define sisd_to_int(x) _mm_cvtsi128_si32(x)

#define sisd_from_int(n) _mm_cvtsi32_si128(n)

static inline int simd_all_true_8(SIMD_type v) {
  return _mm_movemask_epi8(v) == 0xFFFF;
}

static inline int simd_any_true_8(SIMD_type v) {
  return _mm_movemask_epi8(v) != 0;
}

static inline int simd_any_sign_bit_8(SIMD_type v) {
  return _mm_movemask_epi8(v) != 0;
}

#define simd_all_eq_8(v1, v2) simd_all_true_8(_mm_cmpeq_epi8(v1, v2))
#define simd_all_le_8(v1, v2) \
  simd_all_eq_8(simd_max_8(v1, v2), v2)

#define simd_all_signed_gt_8(v1, v2) simd_all_true_8(_mm_cmpgt_epi8(v1, v2))

#define simd_cmpgt_8(v1,v2) _mm_cmpgt_epi8(v1, v2)

static inline int bitblock_has_bit(SIMD_type v) {
  return !simd_all_true_8(simd_eq_8(v, simd_const_8(0)));
}

/*  Packed test operation of SSE4. */
static inline int sisd_ptest(SIMD_type v, SIMD_type mask) {
  return !simd_all_true_8(simd_eq_8(simd_and(v, mask), simd_const_8(0)));
}



#define bitblock_test_bit(blk, n) \
   sisd_to_int(sisd_srli(sisd_slli(blk, ((BLOCKSIZE-1)-(n))), BLOCKSIZE-1))

#define simd_pack_2(a,b)\
	simd_pack_4(simd_if(simd_himask_2,sisd_srli(a,1),a),\
	simd_if(simd_himask_2,sisd_srli(b,1),b))
#define simd_pack_4(a,b)\
	simd_pack_8(simd_if(simd_himask_4,sisd_srli(a,2),a),\
	simd_if(simd_himask_4,sisd_srli(b,2),b))
#define simd_pack_8(a,b)\
	simd_pack_16(simd_if(simd_himask_8,sisd_srli(a,4),a),\
	simd_if(simd_himask_8,sisd_srli(b,4),b))

#ifndef simd_add_2_xx
#define simd_add_2_xx(v1, v2) simd_add_2(v1, v2)
#endif

#ifndef simd_add_2_xl
#define simd_add_2_xl(v1, v2) simd_add_2(v1, simd_andc(v2, simd_himask_2))
#endif

#ifndef simd_add_2_xh
#define simd_add_2_xh(v1, v2) simd_add_2(v1, simd_srli_2(v2, 1))
#endif

#ifndef simd_add_2_lx
#define simd_add_2_lx(v1, v2) simd_add_2(simd_andc(v1, simd_himask_2), v2)
#endif

#ifndef simd_add_2_ll
#define simd_add_2_ll(v1, v2) simd_add_8(simd_andc(v1, simd_himask_2), simd_andc(v2, simd_himask_2))
#endif

#ifndef simd_add_2_lh
#define simd_add_2_lh(v1, v2) simd_add_8(simd_andc(v1, simd_himask_2), simd_srli_2(v2, 1))
#endif

#ifndef simd_add_2_hx
#define simd_add_2_hx(v1, v2) simd_add_2(simd_srli_2(v1, 1), v2)
#endif

#ifndef simd_add_2_hl
#define simd_add_2_hl(v1, v2) simd_add_8(simd_srli_2(v1, 1), simd_andc(v2, simd_himask_2))
#endif

#ifndef simd_add_2_hh
#define simd_add_2_hh(v1, v2) simd_add_8(simd_srli_2(v1, 1), simd_srli_2(v2, 1))
#endif

#ifndef simd_add_4_xx
#define simd_add_4_xx(v1, v2) simd_add_4(v1, v2)
#endif

#ifndef simd_add_4_xl
#define simd_add_4_xl(v1, v2) simd_add_4(v1, simd_andc(v2, simd_himask_4))
#endif

#ifndef simd_add_4_xh
#define simd_add_4_xh(v1, v2) simd_add_4(v1, simd_srli_4(v2, 2))
#endif

#ifndef simd_add_4_lx
#define simd_add_4_lx(v1, v2) simd_add_4(simd_andc(v1, simd_himask_4), v2)
#endif

#ifndef simd_add_4_ll
#define simd_add_4_ll(v1, v2) simd_add_8(simd_andc(v1, simd_himask_4), simd_andc(v2, simd_himask_4))
#endif

#ifndef simd_add_4_lh
#define simd_add_4_lh(v1, v2) simd_add_8(simd_andc(v1, simd_himask_4), simd_srli_4(v2, 2))
#endif

#ifndef simd_add_4_hx
#define simd_add_4_hx(v1, v2) simd_add_4(simd_srli_4(v1, 2), v2)
#endif

#ifndef simd_add_4_hl
#define simd_add_4_hl(v1, v2) simd_add_8(simd_srli_4(v1, 2), simd_andc(v2, simd_himask_4))
#endif

#ifndef simd_add_4_hh
#define simd_add_4_hh(v1, v2) simd_add_8(simd_srli_4(v1, 2), simd_srli_4(v2, 2))
#endif

#ifndef simd_add_8_xx
#define simd_add_8_xx(v1, v2) simd_add_8(v1, v2)
#endif

#ifndef simd_add_8_xl
#define simd_add_8_xl(v1, v2) simd_add_8(v1, simd_andc(v2, simd_himask_8))
#endif

#ifndef simd_add_8_xh
#define simd_add_8_xh(v1, v2) simd_add_8(v1, simd_srli_8(v2, 4))
#endif

#ifndef simd_add_8_lx
#define simd_add_8_lx(v1, v2) simd_add_8(simd_andc(v1, simd_himask_8), v2)
#endif

#ifndef simd_add_8_ll
#define simd_add_8_ll(v1, v2) simd_add_8(simd_andc(v1, simd_himask_8), simd_andc(v2, simd_himask_8))
#endif

#ifndef simd_add_8_lh
#define simd_add_8_lh(v1, v2) simd_add_8(simd_andc(v1, simd_himask_8), simd_srli_8(v2, 4))
#endif

#ifndef simd_add_8_hx
#define simd_add_8_hx(v1, v2) simd_add_8(simd_srli_8(v1, 4), v2)
#endif

#ifndef simd_add_8_hl
#define simd_add_8_hl(v1, v2) simd_add_8(simd_srli_8(v1, 4), simd_andc(v2, simd_himask_8))
#endif

#ifndef simd_add_8_hh
#define simd_add_8_hh(v1, v2) simd_add_8(simd_srli_8(v1, 4), simd_srli_8(v2, 4))
#endif

#ifndef simd_add_16_xx
#define simd_add_16_xx(v1, v2) simd_add_16(v1, v2)
#endif

#ifndef simd_add_16_xl
#define simd_add_16_xl(v1, v2) simd_add_16(v1, simd_andc(v2, simd_himask_16))
#endif

#ifndef simd_add_16_xh
#define simd_add_16_xh(v1, v2) simd_add_16(v1, simd_srli_16(v2, 8))
#endif

#ifndef simd_add_16_lx
#define simd_add_16_lx(v1, v2) simd_add_16(simd_andc(v1, simd_himask_16), v2)
#endif

#ifndef simd_add_16_ll
#define simd_add_16_ll(v1, v2) simd_add_16(simd_andc(v1, simd_himask_16), simd_andc(v2, simd_himask_16))
#endif

#ifndef simd_add_16_lh
#define simd_add_16_lh(v1, v2) simd_add_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8))
#endif

#ifndef simd_add_16_hx
#define simd_add_16_hx(v1, v2) simd_add_16(simd_srli_16(v1, 8), v2)
#endif

#ifndef simd_add_16_hl
#define simd_add_16_hl(v1, v2) simd_add_16(simd_srli_16(v1, 8), simd_andc(v2, simd_himask_16))
#endif

#ifndef simd_add_16_hh
#define simd_add_16_hh(v1, v2) simd_add_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8))
#endif

#ifndef simd_add_32_xx
#define simd_add_32_xx(v1, v2) simd_add_32(v1, v2)
#endif

#ifndef simd_add_32_xl
#define simd_add_32_xl(v1, v2) simd_add_32(v1, simd_andc(v2, simd_himask_32))
#endif

#ifndef simd_add_32_xh
#define simd_add_32_xh(v1, v2) simd_add_32(v1, simd_srli_32(v2, 16))
#endif

#ifndef simd_add_32_lx
#define simd_add_32_lx(v1, v2) simd_add_32(simd_andc(v1, simd_himask_32), v2)
#endif

#ifndef simd_add_32_ll
#define simd_add_32_ll(v1, v2) simd_add_32(simd_andc(v1, simd_himask_32), simd_andc(v2, simd_himask_32))
#endif

#ifndef simd_add_32_lh
#define simd_add_32_lh(v1, v2) simd_add_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16))
#endif

#ifndef simd_add_32_hx
#define simd_add_32_hx(v1, v2) simd_add_32(simd_srli_32(v1, 16), v2)
#endif

#ifndef simd_add_32_hl
#define simd_add_32_hl(v1, v2) simd_add_32(simd_srli_32(v1, 16), simd_andc(v2, simd_himask_32))
#endif

#ifndef simd_add_32_hh
#define simd_add_32_hh(v1, v2) simd_add_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16))
#endif

#ifndef simd_add_64_xx
#define simd_add_64_xx(v1, v2) simd_add_64(v1, v2)
#endif

#ifndef simd_add_64_xl
#define simd_add_64_xl(v1, v2) simd_add_64(v1, simd_andc(v2, simd_himask_64))
#endif

#ifndef simd_add_64_xh
#define simd_add_64_xh(v1, v2) simd_add_64(v1, simd_srli_64(v2, 32))
#endif

#ifndef simd_add_64_lx
#define simd_add_64_lx(v1, v2) simd_add_64(simd_andc(v1, simd_himask_64), v2)
#endif

#ifndef simd_add_64_ll
#define simd_add_64_ll(v1, v2) simd_add_64(simd_andc(v1, simd_himask_64), simd_andc(v2, simd_himask_64))
#endif

#ifndef simd_add_64_lh
#define simd_add_64_lh(v1, v2) simd_add_64(simd_andc(v1, simd_himask_64), simd_srli_64(v2, 32))
#endif

#ifndef simd_add_64_hx
#define simd_add_64_hx(v1, v2) simd_add_64(simd_srli_64(v1, 32), v2)
#endif

#ifndef simd_add_64_hl
#define simd_add_64_hl(v1, v2) simd_add_64(simd_srli_64(v1, 32), simd_andc(v2, simd_himask_64))
#endif

#ifndef simd_add_64_hh
#define simd_add_64_hh(v1, v2) simd_add_64(simd_srli_64(v1, 32), simd_srli_64(v2, 32))
#endif

#ifndef simd_add_128_xx
#define simd_add_128_xx(v1, v2) simd_add_128(v1, v2)
#endif

#ifndef simd_add_128_xl
#define simd_add_128_xl(v1, v2) simd_add_128(v1, simd_andc(v2, simd_himask_128))
#endif

#ifndef simd_add_128_xh
#define simd_add_128_xh(v1, v2) simd_add_128(v1, simd_srli_128(v2, 64))
#endif

#ifndef simd_add_128_lx
#define simd_add_128_lx(v1, v2) simd_add_128(simd_andc(v1, simd_himask_128), v2)
#endif

#ifndef simd_add_128_ll
#define simd_add_128_ll(v1, v2) simd_add_128(simd_andc(v1, simd_himask_128), simd_andc(v2, simd_himask_128))
#endif

#ifndef simd_add_128_lh
#define simd_add_128_lh(v1, v2) simd_add_128(simd_andc(v1, simd_himask_128), simd_srli_128(v2, 64))
#endif

#ifndef simd_add_128_hx
#define simd_add_128_hx(v1, v2) simd_add_128(simd_srli_128(v1, 64), v2)
#endif

#ifndef simd_add_128_hl
#define simd_add_128_hl(v1, v2) simd_add_128(simd_srli_128(v1, 64), simd_andc(v2, simd_himask_128))
#endif

#ifndef simd_add_128_hh
#define simd_add_128_hh(v1, v2) simd_add_128(simd_srli_128(v1, 64), simd_srli_128(v2, 64))
#endif

#ifndef simd_pack_2_xx
#define simd_pack_2_xx(v1, v2) simd_pack_2(v1, v2)
#endif

#ifndef simd_pack_2_xl
#define simd_pack_2_xl(v1, v2) simd_pack_2(v1, v2)
#endif

#ifndef simd_pack_2_xh
#define simd_pack_2_xh(v1, v2) simd_pack_2(v1, simd_srli_16(v2, 1))
#endif

#ifndef simd_pack_2_lx
#define simd_pack_2_lx(v1, v2) simd_pack_2(v1, v2)
#endif

#ifndef simd_pack_2_ll
#define simd_pack_2_ll(v1, v2) simd_pack_2(v1, v2)
#endif

#ifndef simd_pack_2_lh
#define simd_pack_2_lh(v1, v2) simd_pack_2(v1, simd_srli_16(v2, 1))
#endif

#ifndef simd_pack_2_hx
#define simd_pack_2_hx(v1, v2) simd_pack_2(simd_srli_16(v1, 1), v2)
#endif

#ifndef simd_pack_2_hl
#define simd_pack_2_hl(v1, v2) simd_pack_2(simd_srli_16(v1, 1), v2)
#endif

#ifndef simd_pack_2_hh
#define simd_pack_2_hh(v1, v2) simd_pack_2(simd_srli_16(v1, 1), simd_srli_16(v2, 1))
#endif

#ifndef simd_pack_4_xx
#define simd_pack_4_xx(v1, v2) simd_pack_4(v1, v2)
#endif

#ifndef simd_pack_4_xl
#define simd_pack_4_xl(v1, v2) simd_pack_4(v1, v2)
#endif

#ifndef simd_pack_4_xh
#define simd_pack_4_xh(v1, v2) simd_pack_4(v1, simd_srli_16(v2, 2))
#endif

#ifndef simd_pack_4_lx
#define simd_pack_4_lx(v1, v2) simd_pack_4(v1, v2)
#endif

#ifndef simd_pack_4_ll
#define simd_pack_4_ll(v1, v2) simd_pack_4(v1, v2)
#endif

#ifndef simd_pack_4_lh
#define simd_pack_4_lh(v1, v2) simd_pack_4(v1, simd_srli_16(v2, 2))
#endif

#ifndef simd_pack_4_hx
#define simd_pack_4_hx(v1, v2) simd_pack_4(simd_srli_16(v1, 2), v2)
#endif

#ifndef simd_pack_4_hl
#define simd_pack_4_hl(v1, v2) simd_pack_4(simd_srli_16(v1, 2), v2)
#endif

#ifndef simd_pack_4_hh
#define simd_pack_4_hh(v1, v2) simd_pack_4(simd_srli_16(v1, 2), simd_srli_16(v2, 2))
#endif

#ifndef simd_pack_8_xx
#define simd_pack_8_xx(v1, v2) simd_pack_8(v1, v2)
#endif

#ifndef simd_pack_8_xl
#define simd_pack_8_xl(v1, v2) simd_pack_8(v1, v2)
#endif

#ifndef simd_pack_8_xh
#define simd_pack_8_xh(v1, v2) simd_pack_8(v1, simd_srli_16(v2, 4))
#endif

#ifndef simd_pack_8_lx
#define simd_pack_8_lx(v1, v2) simd_pack_8(v1, v2)
#endif

#ifndef simd_pack_8_ll
#define simd_pack_8_ll(v1, v2) simd_pack_8(v1, v2)
#endif

#ifndef simd_pack_8_lh
#define simd_pack_8_lh(v1, v2) simd_pack_8(v1, simd_srli_16(v2, 4))
#endif

#ifndef simd_pack_8_hx
#define simd_pack_8_hx(v1, v2) simd_pack_8(simd_srli_16(v1, 4), v2)
#endif

#ifndef simd_pack_8_hl
#define simd_pack_8_hl(v1, v2) simd_pack_8(simd_srli_16(v1, 4), v2)
#endif

#ifndef simd_pack_8_hh
#define simd_pack_8_hh(v1, v2) simd_pack_8(simd_srli_16(v1, 4), simd_srli_16(v2, 4))
#endif

#ifndef simd_pack_16_xx
#define simd_pack_16_xx(v1, v2) simd_pack_16(v1, v2)
#endif

#ifndef simd_pack_16_xl
#define simd_pack_16_xl(v1, v2) simd_pack_16(v1, v2)
#endif

#ifndef simd_pack_16_xh
#define simd_pack_16_xh(v1, v2) simd_pack_16(v1, simd_srli_16(v2, 8))
#endif

#ifndef simd_pack_16_lx
#define simd_pack_16_lx(v1, v2) simd_pack_16(v1, v2)
#endif

#ifndef simd_pack_16_ll
#define simd_pack_16_ll(v1, v2) simd_pack_16(v1, v2)
#endif

#ifndef simd_pack_16_lh
#define simd_pack_16_lh(v1, v2) simd_pack_16(v1, simd_srli_16(v2, 8))
#endif

#ifndef simd_pack_16_hx
#define simd_pack_16_hx(v1, v2) simd_pack_16(simd_srli_16(v1, 8), v2)
#endif

#ifndef simd_pack_16_hl
#define simd_pack_16_hl(v1, v2) simd_pack_16(simd_srli_16(v1, 8), v2)
#endif

#ifndef simd_pack_16_hh
//#define simd_pack_16_hh(v1, v2) simd_pack_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8))
//Masking performned by simd_pack_16 is unnecessary.
#define simd_pack_16_hh(v1, v2) _mm_packus_epi16(simd_srli_16(v1, 8), simd_srli_16(v2, 8))
#endif


// Splat the first 16-bit int into all positions.
static inline SIMD_type simd_splat_16(SIMD_type x) {
  SIMD_type t = _mm_shufflelo_epi16(x,0);
  return _mm_shuffle_epi32(t,0);
}

// Splat the first 32-bit int into all positions.
static inline SIMD_type simd_splat_32(SIMD_type x) {
  return _mm_shuffle_epi32(x,0);
}


void print_bit_block(char * var_name, SIMD_type v) {
  union {SIMD_type vec; unsigned char elems[8];} x;
  x.vec = v;
  unsigned char c;//, bit_reversed;
  size_t i;
  printf("%20s = ", var_name);
  for (i = 0; i < sizeof(SIMD_type); i++) {
    c = x.elems[i];
     printf("%02X ", c); 
  }
  printf("\n");
}

static inline int bitblock_bit_count(SIMD_type v) {
  //int bit_count = 0;
  SIMD_type cts_2 = simd_add_2_lh(v, v);
  SIMD_type cts_4 = simd_add_4_lh(cts_2, cts_2);
  SIMD_type cts_8 = simd_add_8_lh(cts_4, cts_4);
  SIMD_type cts_64 = _mm_sad_epu8(cts_8, simd_const_8(0));
  /* SIMD_type cts_128 = simd_add_128_lh(cts_64, cts_64) */;
  SIMD_type cts_128 = simd_add_64(cts_64, sisd_srli(cts_64,64));
  return (int) sisd_to_int(cts_128);
}

#define sb_op(x, n) ((x)>>(n))
#define sf_op(x, n) ((x)<<(n))
#ifdef __GNUC__
#define cfzl __builtin_ctzl
#endif
#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward)
//  precondition: x > 0
static inline unsigned long cfzl(unsigned long x) {
	unsigned long zeroes;
	_BitScanForward(&zeroes, x);
	return zeroes;
}
#endif

static inline int count_forward_zeroes(SIMD_type bits) {
  union {SIMD_type vec; unsigned long elems[sizeof(SIMD_type)/sizeof(long)];} v;
  v.vec = bits;
  if (v.elems[0] != 0) return cfzl(v.elems[0]);
  else if (v.elems[1] != 0) return LONG_BIT + cfzl(v.elems[1]);
#ifdef _MSC_VER
  else if (v.elems[2] != 0) return 2*LONG_BIT + cfzl(v.elems[2]);
  else if (v.elems[3] != 0) return 3*LONG_BIT + cfzl(v.elems[3]);
#endif
#ifndef _MSC_VER
#if LONG_BIT < 64
  else if (v.elems[2] != 0) return 2*LONG_BIT + cfzl(v.elems[2]);
  else if (v.elems[3] != 0) return 3*LONG_BIT + cfzl(v.elems[3]);
#endif
#endif
  else return 8*sizeof(SIMD_type);
}

#ifdef ADC_128_VIA_GEN_REG
// 128-bit add with carry
// (rslt, carryout) = x + y + carryin
// where x = (x2, x1), y = (y2, y1), rslt = (rslt2, rslt1)
#define double_int64_adc(x1, x2, y1, y2, rslt1, rslt2, carry) \
  __asm__  ("sahf\n\t" \
	    "adc %[e1], %[z1]\n\t" \
	    "adc %[e2], %[z2]\n\t" \
	    "lahf\n\t" \
	 : [z1] "=r" (rslt1), [z2] "=r" (rslt2), [carry] "=a" (carry) \
         : "[z1]" (x1), "[z2]" (x2), \
           [e1] "r" (y1), [e2] "r" (y2), \
           "[carry]" (carry) \
         : "cc")

static inline SIMD_type adc128(SIMD_type first, SIMD_type second, int &carry)
{
  union {__m128i bitblock;
         uint64_t int64[2];} rslt;

  union {__m128i bitblock;
         uint64_t int64[2];} x;

  union {__m128i bitblock;
         uint64_t int64[2];} y;

  x.bitblock = first;
  y.bitblock = second;

  double_int64_adc(x.int64[0], x.int64[1], y.int64[0], y.int64[1], 
                   rslt.int64[0], rslt.int64[1], carry);

  return rslt.bitblock;
}
#endif

#ifndef ADC_128_VIA_GEN_REG
static inline void adc128(SIMD_type x, SIMD_type y, SIMD_type * carry, SIMD_type *sum)
{
  /* Carries are always generated if both high bits are 1. */
  SIMD_type gen = simd_and(x, y);

  /* Carries may propagate if either high bit is 1. */
  SIMD_type prop = simd_or(x, y);

  /* Partial add without carry for high 64. */
  SIMD_type partial = simd_add_64(simd_add_64(x, y), *carry);

  /* Carry for high 64 */
  SIMD_type c1 = sisd_slli(simd_srli_64(simd_or(gen, simd_andc(prop, partial)), 63), 64);

  /* Final sum */
  *sum = simd_add_64(c1, partial);

  /* Carry out */
  *carry = sisd_srli(simd_or(gen, simd_andc(prop, *sum)), 127);

}

static inline void sbb128(SIMD_type x, SIMD_type y, SIMD_type *borrow, SIMD_type *difference)
{
  /* Borrows are always generated if the high bit of x is 0 and the high bit of y is 1. */
  SIMD_type gen = simd_andc(y, x);

  /* Borrows may propagate if the high bits of both x and y are 0 or if the high bits of both x and y are 1 . */
  SIMD_type prop = simd_not(simd_xor(x, y));

  /* Partial sub with borrow for high 64. */
  SIMD_type partial = simd_sub_64(simd_sub_64(x, y), *borrow);

  /* Borrow for high 64 */
  SIMD_type b1 = sisd_slli(simd_srli_64(simd_or(gen, simd_and(prop, partial)), 63), 64);

  /* Final difference */
  *difference = simd_sub_64(partial, b1);

  /* Borrow out */
  *borrow = sisd_srli(simd_or(gen, simd_and(prop, *difference)), 127);

}
#endif

static inline SIMD_type advance_with_carry(SIMD_type cursor, SIMD_type &carry)
{
  SIMD_type shift_out = simd_srli_64(cursor, 63);
  SIMD_type low_bits = simd_mergel_64(shift_out, carry);
  carry = sisd_srli(shift_out, 64);
  return simd_or(simd_add_64(cursor, cursor), low_bits);
}




#endif

