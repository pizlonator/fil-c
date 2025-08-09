/* Copyright (C) 2007 Robert D. Cameron, Dan Lin
   Licensed to International Characters Inc. and Simon Fraser University
              under the Academic Free License version 3.0.
   Licensed to the public under the Open Software License version 3.0.
*/
#ifndef _MSC_VER
#include <stdint.h>
#endif
#ifdef _MSC_VER
#include "stdint.h"
#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#include <limits.h>
#ifndef LONG_BIT
#define LONG_BIT (8* sizeof(unsigned long))
#endif 
#include "mmx_simd_built_in.h"
#include "mmx_simd_basic.h"
#include "mmx_simd_modified.h"

/* mmintrin.h does not provide access to pmaxub;
   xmmintrin.h does via _mm_max_pu8(a, b), but also
   requires SSE. */

static inline SIMD_type simd_max_8(SIMD_type a, SIMD_type b) {
  asm volatile(
    "pmaxub %[rb], %[ra]\n\t"
    : [ra] "+y" (a)
    : [rb] "y" (b));
  return a;
}

char mask_x55 [8] __attribute__ ((aligned(8))) = 
    {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55};
char mask_x33 [8] __attribute__ ((aligned(8))) = 
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33};
char mask_x0F [8] __attribute__ ((aligned(8))) = 
    {0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F};

static inline int simd_all_true_8(SIMD_type maskvector) {
/*  return _mm_movemask_epi8(maskvector) == 0xFF;  */
  int maskbyte;
  asm volatile("pmovmskb %[maskvector], %[maskbyte]\n"
               : [maskbyte] "=r" (maskbyte)
               : [maskvector] "y" (maskvector));
  return maskbyte == 0xFF; 
}
static inline int simd_any_true_8(SIMD_type maskvector) {
/*  return _mm_movemask_epi8(maskvector) =!= 0;  */
  int maskbyte;
  asm volatile("pmovmskb %[maskvector], %[maskbyte]\n"
               : [maskbyte] "=r" (maskbyte)
               : [maskvector] "y" (maskvector));
  return maskbyte != 0; 
}

static inline int simd_any_sign_bit_8(SIMD_type v) {
/*  return _mm_movemask_epi8(v) =!= 0;  */
  int signbyte;
  asm volatile("pmovmskb %[v], %[signbyte]\n"
               : [signbyte] "=r" (signbyte)
               : [v] "y" (v));
  return signbyte != 0; 
}

#define simd_all_eq_8(v1, v2) simd_all_true_8(_mm_cmpeq_pi8(v1, v2))
#define simd_all_le_8(v1, v2) \
  simd_all_eq_8(simd_max_8(v1, v2), v2)

#define simd_all_signed_gt_8(v1, v2) simd_all_true_8(_mm_cmpgt_pi8(v1, v2))

static inline int bitblock_has_bit(SIMD_type v) {
  return !simd_all_true_8(simd_eq_8(v, simd_const_8(0)));
}


#define bitblock_test_bit(blk, n) \
  sisd_to_int(sisd_srli(sisd_slli(blk, (BLOCKSIZE - 1) - (n)), BLOCKSIZE-1))

/*#define bitblock_test_bit(blk, n) \
((n) == 63 ? sisd_to_int(sisd_srli(blk, n)) : \
(sisd_to_int(sisd_srli(blk, n)) & 1)) */


void print_bit_block(char * var_name, SIMD_type v) {
  union {SIMD_type vec; unsigned char elems[8];} x;
  x.vec = v;
  unsigned char c, bit_reversed;
  int i;
  printf("%20s = ", var_name);
  for (i = 0; i < sizeof(SIMD_type); i++) {
    c = x.elems[i];
     printf("%02X ", c); 
  }
  printf("\n");
}

static inline int bitblock_bit_count(SIMD_type v) {

  SIMD_type cts_2 = simd_add_2_lh(v,v);
  SIMD_type cts_4 = simd_add_4_lh(cts_2,cts_2);
  SIMD_type cts_8 = simd_add_8_lh(cts_4,cts_4);
  SIMD_type r = simd_const_8(0);

  asm volatile("psadbw %[r_reg], %[cts8_reg]\n\t"
               : [cts8_reg] "+y" (cts_8)
               : [r_reg] "y" (r));	
  return sisd_to_int(cts_8);
}



static inline int count_forward_zeroes(SIMD_type bits) {
  union {SIMD_type vec; unsigned int elems[2];} v;
  v.vec = bits;
  if (v.elems[0] != 0) return __builtin_ctzl(v.elems[0]);
  else if (v.elems[1] != 0) return 32 + __builtin_ctzl(v.elems[1]);
  else return 64;
}


/* Scans for a 1 as long as it takes.  Use a sentinel to fence. */
static inline int bitstream_scan(SIMD_type * stream, int bit_posn) {
  unsigned int * bitstream_ptr = (unsigned int *) (((intptr_t) stream) + bit_posn/8);
  unsigned int bitstream_slice = *bitstream_ptr & (-1 << bit_posn % 8);
  unsigned int slice_scan;
  if (bitstream_slice == 0) {
    do {
      bitstream_ptr++;
      bitstream_slice = *bitstream_ptr;
    } while (bitstream_slice == 0);
  }
  slice_scan = __builtin_ctz(bitstream_slice);
  return 8*((intptr_t) bitstream_ptr - (intptr_t) stream) + slice_scan;
}




