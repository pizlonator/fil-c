/*  Idealized SIMD Operations with Altivec, SSE and MMX versions
    Copyright (C) 2006, Robert D. Cameron
    Licensed to International Characters Inc. 
       under the Academic Free License version 3.0.
       October 30, 2006 */

/*------------------------------------------------------------*/

#include <limits.h>

typedef vector unsigned short vUInt16;
typedef vector unsigned int vUInt32;
typedef vector unsigned char SIMD_type;


#define vec_lvsl1(x) vec_lvsl(x, (unsigned char *) 0)
#define vec_lvsr1(x) vec_lvsr(x, (unsigned char *) 0)


/*------------------------------------------------------------*/
/* I. SIMD bitwise logical operations */

#define simd_or(b1, b2) vec_or(b1, b2) 
#define simd_and(b1, b2) vec_and(b1, b2) 
#define simd_xor(b1, b2) vec_xor(b1, b2)
#define simd_nor(b1, b2) vec_nor(b1, b2)
#define simd_andc(b1, b2) vec_andc(b1, b2)
#define simd_if(cond, then_val, else_val) vec_sel(else_val, then_val, cond)
#define simd_not(x) vec_nor(x, x)


/* Idealized operations with direct implementation by built-in 
   operations for Altivec. */

#define simd_add_8(a, b) vec_vaddubm(a, b)
#define simd_add_16(a, b) (SIMD_type) vec_vadduhw((vUInt16) a, (vUInt16) b)
#define simd_add_32(a, b) (SIMD_type) vec_vadduwm((vUInt32) a, (vUInt32) b)
#define simd_sub_8(a, b) vec_vsububm(a, b)
#define simd_sub_16(a, b) (SIMD_type) vec_vsubuhm((vUInt16) a, (vUInt16) b)
#define simd_sub_32(a, b) (SIMD_type) vec_vsubuwm((vUInt32) a, (vUInt32) b)
#define simd_mult_16(a, b) (SIMD_type) vec_mladd((vUInt16) a, (vUInt16) b, (vUInt16) vec_splat_u8(0))
#define simd_mergeh_8(v1, v2) vec_vmrghb(v1, v2)
#define simd_mergeh_16(v1, v2) (SIMD_type) vec_vmrghh((vUInt16) v1, (vUInt16) v2)
#define simd_mergeh_32(v1, v2) (SIMD_type) vec_vmrghw((vUInt32) v1, (vUInt32) v2)
#define simd_mergel_8(v1, v2) vec_vmrglb(v1, v2)
#define simd_mergel_16(v1, v2) (SIMD_type) vec_vmrglh((vUInt16) v1, (vUInt16) v2)
#define simd_mergel_32(v1, v2) (SIMD_type) vec_vmrglw((vUInt32) v1, (vUInt32) v2)
#define simd_pack_16(v1, v2) vec_vpkuhum((vUInt16) v1, (vUInt16) v2)
#define simd_pack_32(v1, v2) (SIMD_type) vec_vpkuwum((vUInt32) v1, (vUInt32) v2)
#define simd_sll_8(r, shft) vec_vslb(r, shft)
#define simd_srl_8(r, shft) vec_vsrb(r, shft)
#define simd_sra_8(r, shft) vec_vsrab(r, shft)
#define simd_rotl_8(r, shft) vec_vrlb(r, shft)
#define simd_sll_16(r, shft) (SIMD_type) vec_vslh((vUInt16) r, (vUInt16) shft)
#define simd_srl_16(r, shft) (SIMD_type) vec_vsrh((vUInt16) r, (vUInt16) shft)
#define simd_sra_16(r, shft) (SIMD_type) vec_vsrah((vUInt16) r, (vUInt16) shft)
#define simd_rotl_16(r, shft) (SIMD_type) vec_vrlh((vUInt16) r, (vUInt16) shft)
#define simd_sll_32(r, shft) (SIMD_type) vec_vslw((vUInt32) r, (vUInt32) shft)
#define simd_srl_32(r, shft) (SIMD_type) vec_vsrw((vUInt32) r, (vUInt32) shft)
#define simd_sra_32(r, shft) (SIMD_type) vec_vsraw((vUInt32) r, (vUInt32) shft)
#define simd_rotl_32(r, shft) (SIMD_type) vec_vrlw((vUInt32) r, (vUInt32) shft)
#define simd_slli_8(r, shft) vec_vslb(r, vec_splat_u8(shft))
#define simd_srli_8(r, shft) vec_vsrb(r, vec_splat_u8(shft))
#define simd_srai_8(r, shft) vec_vsrab(r, vec_splat_u8(shft))
#define simd_rotli_8(r, shft) vec_vrlb(r, vec_splat_u8(shft))
/* For shifts of 16 or 32, the shift values could be loaded by
   vec_splat_u16 or vec_splat_32.  However, using vec_splat_u8
   works as well, as only the low 4 or 5 bits are used.  The
   vec_splat_u8 is used to increase the chance that the
   optimizer will find this value already in a register. */
#define simd_slli_16(r, shft) (SIMD_type) vec_vslh((vUInt16) r, (vUInt16) vec_splat_u8(shft))
#define simd_srli_16(r, shft) (SIMD_type) vec_vsrh((vUInt16) r, (vUInt16) vec_splat_u8(shft))
#define simd_srai_16(r, shft) (SIMD_type) vec_vsrah((vUInt16) r, (vUInt16) vec_splat_u8(shft))
#define simd_rotli_16(r, shft) (SIMD_type) vec_vrlh((vUInt16) r, (vUInt16) vec_splat_u8(shft))
/* Because only the least significant 5 bits are used in 32 bit
   shifts, shifts of 16 to 31 are equivalent to shifts of -16 to -1.
   Translating to the negative values allows the shift constant to be 
   loaded with a single vec_splat_u8. */ 
#define splat_shft(shft) vec_splat_u8((shft) >= 16 ? (shft)-32 : (shft))
#define simd_slli_32(r, shft) (SIMD_type) vec_vslw((vUInt32) r, (vUInt32) splat_shft(shft))
#define simd_srli_32(r, shft) (SIMD_type) vec_vsrw((vUInt32) r, (vUInt32) splat_shft(shft))
#define simd_srai_32(r, shft) (SIMD_type) vec_vsraw((vUInt32) r, (vUInt32) splat_shft(shft))
#define simd_rotli_32(r, shft) (SIMD_type) vec_vrlw((vUInt32) r, (vUInt32) splat_shft(shft))
#define simd_eq_8(a, b) (SIMD_type) vec_vcmpequb(a, b)
#define simd_eq_16(a, b) (SIMD_type) vec_vcmpequh((vUInt16) a, (vUInt16) b)
#define simd_eq_32(a, b) (SIMD_type) vec_vcmpequw((vUInt32) a, (vUInt32) b)

#define simd_max_8(a, b) vec_vmaxub(a, b)


#define simd_permute(a, b, c) vec_perm(a, b, c)

/* 64-bit and 128-bit add/sub */

#define simd_add_64(a, b) \
  (SIMD_type) vec_add(vec_add((vUInt32) a, (vUInt32) b), \
                     vec_andc(vec_sld(vec_addc((vUInt32) a, (vUInt32) b), vec_0, 4), \
                              (vUInt32) alt_words)) 
#define simd_sub_64(a, b) \
  (SIMD_type) vec_sub(vec_sub((vUInt32) a, (vUInt32) b), \
                     vec_add(vec_sld(vec_sub((vUInt32) vec_0, vec_subc((vUInt32) a, (vUInt32) b)), vec_0, 4), \
                              (vUInt32) alt_words)) 

static inline SIMD_type simd_add_128(SIMD_type a, SIMD_type b) {
  vUInt32 sum1 = vec_add((vUInt32) a, (vUInt32) b);
  vUInt32 carry1 = vec_sld(vec_addc((vUInt32) a, (vUInt32) b), (vUInt32) vec_splat_u8(0), 4);
  vUInt32 sum2 = vec_add(sum1, carry1);
  vUInt32 carry2 = vec_sld(vec_addc(sum1, carry1), (vUInt32) vec_splat_u8(0), 4);
  vUInt32 sum3 = vec_add(sum2, carry2);
  vUInt32 carry3 = vec_sld(vec_addc(sum2, carry2), (vUInt32) vec_splat_u8(0), 4);
  return vec_add(sum3, carry3);
}


/* Altivec has separate full register shift instructions for
   small shifts < 8 (vec_sll, vec_srl) and for shifts in 
   multiples of 8 (vec_slo, vec_sro, vec_sld).  The bytealign
   macros handle the mod 8 shift, while vec_sld is used for
   to complete the shift. */
#define sl_bytealign(r, shft) \
  ((shft) % 8 == 0 ? r : vec_sll(r, vec_splat_u8(shft)))
#define sr_bytealign(r, shft) \
  ((shft) % 8 == 0 ? r : vec_srl(r, vec_splat_u8(shft)))
#define simd_slli_128(r, shft) \
  ((shft) < 8 ? sl_bytealign(r, shft):\
   (shft) < 16 ? vec_slo(sl_bytealign(r, shft), vec_splat_u8(shft)) :\
   (shft) >= 112 ? vec_slo(sl_bytealign(r, (shft)-128), vec_splat_u8((shft)-128)):\
   vec_sld(sl_bytealign(r, (shft) % 8), vec_splat_u8(0), (shft) >> 3))
#define simd_srli_128(r, shft) \
  ((shft) < 8 ? sr_bytealign(r, shft):\
   (shft) < 16 ? vec_sro(sr_bytealign(r, shft), vec_splat_u8(shft)) :\
   (shft) >= 112 ? vec_sro(sr_bytealign(r, (shft)-128), vec_splat_u8((shft)-128)):\
   vec_sld(vec_splat_u8(0), sr_bytealign(r, (shft) % 8), 16 - ((shft) >> 3)))

/* The vec_splat(r2, 15) ensures that the shift constant is duplicated 
   in all bytes prior to vec_sll or vec_srl. */
#define simd_sll_128(r1, r2) vec_sll(vec_slo(r1, r2), vec_splat(r2, 15))
#define simd_srl_128(r1, r2) vec_srl(vec_sro(r1, r2), vec_splat(r2, 15))



#define sisd_store_aligned(r, addr) *((SIMD_type *) (addr)) = r
#define sisd_load_aligned(addr) ((SIMD_type) *((SIMD_type *) (addr)))


#define simd_pack_16_ll(a, b) simd_pack_16(a, b)

#ifndef ALTIVEC_USE_EVEN_INDICES
#define simd_pack_16_hh(a, b) \
  simd_pack_16(simd_srli_16(a, 8), simd_srli_16(b, 8))
#endif


#ifdef ALTIVEC_USE_EVEN_INDICES
#define even_byte_indices vec_add(vec_lvsl1(0), vec_lvsl1(0))
#define simd_pack_16_hh(a, b) vec_perm(a, b, even_byte_indices)
#endif


#define sisd_sll(r, shft) simd_sll_128(r, shft)
#define sisd_srl(r, shft) simd_srl_128(r, shft)
#define sisd_slli(r, shft) simd_slli_128(r, shft)
#define sisd_srli(r, shft) simd_srli_128(r, shft)
#define sisd_add(a, b) simd_add_128(a, b)
#define sisd_sub(a, b) simd_sub_128(a, b)





#define simd_himask_2 vec_or(vec_splat_u8(10), vec_sl(vec_splat_u8(10), vec_splat_u8(4)))
#define simd_himask_4 vec_or(vec_splat_u8(12), vec_sl(vec_splat_u8(12), vec_splat_u8(4)))
#define simd_himask_8 vec_splat_u8(-16)



#define simd_const_8(n) \
  ((n) >= -16 && (n) < 15 ? vec_splat_u8(n):\
   vec_or(vec_sl(vec_splat_u8((n)>>4), vec_splat_u8(4), vec_splat_u8((n)&15))))

#define simd_const_16(n) \
  ((SIMD_type) vec_splat_u16(n))

#define simd_const_32(n) \
  (SIMD_type) ((n) >= -16 && (n) < 15 ? vec_splat_u32(n):\
   vec_or(vec_sl(vec_splat_u32((n)>>4), vec_splat_u32(4), vec_splat_u32((n)&15))))

#define simd_const_4(n) \
   vec_or(vec_sl(vec_splat_u8(n), vec_splat_u8(4)), vec_splat_u8(n))

#define simd_const_2(n) \
   vec_or(vec_sl(vec_splat_u8(5*(n)), vec_splat_u8(4)), vec_splat_u8(5*(n)))

#define simd_const_1(n) \
  (n==0 ? simd_const_8(0): simd_const_8(-1))

#define sisd_const(n) vec_sld(vec_splat_u8(0), simd_const_8(n))


static inline int sisd_to_int(SIMD_type x) {
  union {vector signed int vec; signed int elems[4];} xunion;
  xunion.vec = (vector signed int) x;
  return xunion.elems[3];
}

static inline SIMD_type sisd_from_int(unsigned int x) {
  union {SIMD_type vec; unsigned int elems[4];} y;
  y.elems[0] = 0;
  y.elems[1] = 0;
  y.elems[2] = 0;
  y.elems[3] = x;
  return y.vec;
}

#define bitblock_has_bit(blk) vec_any_ne(blk, vec_splat_u8(0))

#define simd_all_le_8(v1, v2) vec_vcmpleub(v1, v2)
static inline int simd_all_signed_gt_8(SIMD_type v1, SIMD_type v2) {
//#define simd_all_signed_gt_8(v1, v2) \
   return vec_all_gt((vector signed char) v1, (vector signed char) v2);
}


#define simd_any_sign_bit_8(v) \
  vec_any_lt((vector signed char) v, (vector signed char) vec_splat_u8(0))

static inline vector unsigned char bits_per_nybble_table() {
  vector unsigned char zeroes = vec_splat_u8(0);
  vector unsigned char ones = vec_splat_u8(1);
  return simd_add_8
	   (simd_add_8(simd_pack_16(zeroes, ones),     // 0000000011111111
                       simd_mergeh_32(zeroes, ones)),  // 0000111100001111    
            simd_add_8(simd_mergeh_16(zeroes, ones),   // 0011001100110011
		       simd_mergeh_8(zeroes, ones)));  // 0101010101010101
  //                                                      ----------------
  //                                                      0112122312232334
}
static inline int bitblock_bit_count(SIMD_type v) {
  union {vector signed int vec; signed int elems[4];} result_count;
  SIMD_type bit_count_tbl = bits_per_nybble_table();
/*   SIMD_type bit_count_tbl = u8u16_control_vector[bits_per_nybble_tbl]; */
  SIMD_type byte_counts;
  byte_counts = vec_add(vec_perm(bit_count_tbl, bit_count_tbl, vec_sr(v, vec_splat_u8(4))),
                        vec_perm(bit_count_tbl, bit_count_tbl, v));
  vector unsigned int acc = vec_sum4s(byte_counts, vec_splat_u32(0));
  result_count.vec = vec_sums((vector signed int) acc, vec_splat_s32(0));
  return result_count.elems[3];
}  

#define bitblock_test_bit(blk, n) \
   sisd_to_int(sisd_srli(sisd_slli(blk, (n)), BLOCKSIZE-1))



static inline int count_fwd_zeroes(SIMD_type v) {
  int zeroes;
  union {SIMD_type vec; signed int elems[4];} vu;
  vu.vec = v;
  asm volatile("cntlzw %0, %1\n" : "=r" (zeroes) : "r" (vu.elems[0]));
  if (zeroes < 32) return zeroes;
  asm volatile("cntlzw %0, %1\n" : "=r" (zeroes) : "r" (vu.elems[1]));
  if (zeroes < 32) return zeroes+32;
  asm volatile("cntlzw %0, %1\n" : "=r" (zeroes) : "r" (vu.elems[2]));
  if (zeroes < 32) return zeroes+64;
  asm volatile("cntlzw %0, %1\n" : "=r" (zeroes) : "r" (vu.elems[3]));
  return zeroes+96;
}


static inline int count_forward_zeroes(SIMD_type bits) {
  union {SIMD_type vec; unsigned long elems[sizeof(SIMD_type)/LONG_BIT];} v;
  v.vec = bits;
  if (v.elems[0] != 0) return __builtin_clzl(v.elems[0]);
  else if (v.elems[1] != 0) return LONG_BIT + __builtin_clzl(v.elems[1]);
#if LONG_BIT < 64
  else if (v.elems[2] != 0) return 2*LONG_BIT + __builtin_clzl(v.elems[2]);
  else if (v.elems[3] != 0) return 3*LONG_BIT + __builtin_clzl(v.elems[3]);
#endif
  else return 8*sizeof(SIMD_type);
}




void print_bit_block(char * var_name, SIMD_type v) {
  union {SIMD_type vec; unsigned char elems[16];} x;
  x.vec = v;
  int i;
  printf("%20s = ", var_name);
  for (i = 0; i < 16; i++) {
    printf("%02X ", x.elems[i]);
  }
  printf("\n");
}


static inline SIMD_type simd_add_2(SIMD_type a, SIMD_type b)
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
	 simd_and(sisd_srli(r,sh),simd_const_2(3>>sh))

#define simd_srli_4(r, sh)\
	 simd_and(sisd_srli(r,sh),simd_const_4(15>>sh))


#define simd_add_2_xx(a, b) simd_add_2(a, b)
#define simd_add_2_xl(a, b) simd_add_2(a, simd_andc(b, simd_himask_2))
#define simd_add_2_xh(a, b) simd_add_2(a, simd_srli_2(b, 1))
#define simd_add_2_lx(a, b) simd_add_2(simd_andc(a, simd_himask_2), b)
#define simd_add_2_ll(a, b) simd_add_2(simd_andc(a, simd_himask_2), simd_andc(b, simd_himask_2))
#define simd_add_2_lh(a, b) simd_add_2(simd_andc(a, simd_himask_2), simd_srli_2(b, 1))
#define simd_add_2_hx(a, b) simd_add_2(simd_srli_2(a, 1), b)
#define simd_add_2_hl(a, b) simd_add_2(simd_srli_2(a, 1), simd_andc(b, simd_himask_2))
#define simd_add_2_hh(a, b) simd_add_2(simd_srli_2(a, 1), simd_srli_2(b, 1))
#define simd_add_4_xx(a, b) simd_add_4(a, b)
#define simd_add_4_xl(a, b) simd_add_4(a, simd_andc(b, simd_himask_4))
#define simd_add_4_xh(a, b) simd_add_4(a, simd_srli_4(b, 2))
#define simd_add_4_lx(a, b) simd_add_4(simd_andc(a, simd_himask_4), b)
#define simd_add_4_ll(a, b) simd_add_4(simd_andc(a, simd_himask_4), simd_andc(b, simd_himask_4))
#define simd_add_4_lh(a, b) simd_add_4(simd_andc(a, simd_himask_4), simd_srli_4(b, 2))
#define simd_add_4_hx(a, b) simd_add_4(simd_srli_4(a, 2), b)
#define simd_add_4_hl(a, b) simd_add_4(simd_srli_4(a, 2), simd_andc(b, simd_himask_4))
#define simd_add_4_hh(a, b) simd_add_4(simd_srli_4(a, 2), simd_srli_4(b, 2))
#define simd_add_8_xx(a, b) simd_add_8(a, b)
#define simd_add_8_xl(a, b) simd_add_8(a, simd_andc(b, simd_himask_8))
#define simd_add_8_xh(a, b) simd_add_8(a, simd_srli_8(b, 4))
#define simd_add_8_lx(a, b) simd_add_8(simd_andc(a, simd_himask_8), b)
#define simd_add_8_ll(a, b) simd_add_8(simd_andc(a, simd_himask_8), simd_andc(b, simd_himask_8))
#define simd_add_8_lh(a, b) simd_add_8(simd_andc(a, simd_himask_8), simd_srli_8(b, 4))
#define simd_add_8_hx(a, b) simd_add_8(simd_srli_8(a, 4), b)
#define simd_add_8_hl(a, b) simd_add_8(simd_srli_8(a, 4), simd_andc(b, simd_himask_8))
#define simd_add_8_hh(a, b) simd_add_8(simd_srli_8(a, 4), simd_srli_8(b, 4))

#define simd_pack_2(a,b)\
	simd_pack_4(simd_if(simd_himask_2,a,sisd_srli(a,1)),\
	simd_if(simd_himask_2,b,sisd_srli(b,1)))
#define simd_pack_4(a,b)\
	simd_pack_8(simd_if(simd_himask_4,a,sisd_srli(a,2)),\
	simd_if(simd_himask_4,b,sisd_srli(b,2)))
#define simd_pack_8(a,b)\
	simd_pack_16(simd_if(simd_himask_8,a,sisd_srli(a,4)),\
	simd_if(simd_himask_8,b,sisd_srli(b,4)))
#define simd_pack_2_xx(a, b) simd_pack_2(a, b)
#define simd_pack_2_xl(a, b) simd_pack_2(a, b)
#define simd_pack_2_xh(a, b) simd_pack_2(a, simd_srli_2(b, 1))
#define simd_pack_2_lx(a, b) simd_pack_2(a, b)
#define simd_pack_2_ll(a, b) simd_pack_2(a, b)
#define simd_pack_2_lh(a, b) simd_pack_2(a, simd_srli_2(b, 1))
#define simd_pack_2_hx(a, b) simd_pack_2(simd_srli_2(a, 1), b)
#define simd_pack_2_hl(a, b) simd_pack_2(simd_srli_2(a, 1), b)
#define simd_pack_2_hh(a, b) simd_pack_2(simd_srli_2(a, 1), simd_srli_2(b, 1))
#define simd_pack_4_xx(a, b) simd_pack_4(a, b)
#define simd_pack_4_xl(a, b) simd_pack_4(a, b)
#define simd_pack_4_xh(a, b) simd_pack_4(a, simd_srli_4(b, 2))
#define simd_pack_4_lx(a, b) simd_pack_4(a, b)
#define simd_pack_4_ll(a, b) simd_pack_4(a, b)
#define simd_pack_4_lh(a, b) simd_pack_4(a, simd_srli_4(b, 2))
#define simd_pack_4_hx(a, b) simd_pack_4(simd_srli_4(a, 2), b)
#define simd_pack_4_hl(a, b) simd_pack_4(simd_srli_4(a, 2), b)
#define simd_pack_4_hh(a, b) simd_pack_4(simd_srli_4(a, 2), simd_srli_4(b, 2))
#define simd_pack_8_xx(a, b) simd_pack_8(a, b)
#define simd_pack_8_xl(a, b) simd_pack_8(a, b)
#define simd_pack_8_xh(a, b) simd_pack_8(a, simd_srli_8(b, 4))
#define simd_pack_8_lx(a, b) simd_pack_8(a, b)
#define simd_pack_8_ll(a, b) simd_pack_8(a, b)
#define simd_pack_8_lh(a, b) simd_pack_8(a, simd_srli_8(b, 4))
#define simd_pack_8_hx(a, b) simd_pack_8(simd_srli_8(a, 4), b)
#define simd_pack_8_hl(a, b) simd_pack_8(simd_srli_8(a, 4), b)
#define simd_pack_8_hh(a, b) simd_pack_8(simd_srli_8(a, 4), simd_srli_8(b, 4))

static inline
SIMD_type simd_sub_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_xor(a,b);
	 SIMD_type borrow = simd_andc(b,a);
	 SIMD_type c2 = simd_xor(c1,(sisd_slli(borrow,1)));
	 return simd_if(simd_himask_2,c2,c1);
}
static inline
SIMD_type simd_srl_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_or((simd_andc(a,b)),(simd_and(b,sisd_srli(a,1))));
	 SIMD_type c2 = simd_andc(a,sisd_slli(b,1));
	 return simd_if(simd_himask_2,c2,c1);
}
static inline
SIMD_type simd_sra_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_or((simd_andc(a,b)),(simd_and(b,sisd_srli(a,1))));
	 return simd_if(simd_himask_2,a,c1);
}
static inline
SIMD_type simd_sll_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_andc(a,b);
	 SIMD_type c2 = simd_or((simd_andc(a,(sisd_slli(b,1)))),(simd_and((sisd_slli(b,1)),(sisd_slli(a,1)))));
	 return simd_if(simd_himask_2,c2,c1);
}
static inline
SIMD_type simd_rotl_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_or((simd_andc(a,b)),(simd_and(b,sisd_srli(a,1))));
	 SIMD_type c2 = simd_or((simd_andc(a,(sisd_slli(b,1)))),(simd_and((sisd_slli(b,1)),(sisd_slli(a,1)))));
	 return simd_if(simd_himask_2,c2,c1);
}

#ifndef simd_sub_4
inline SIMD_type simd_sub_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_sub_8(simd_and(a,simd_himask_8),simd_and(b,simd_himask_8))
	,simd_sub_8(simd_andc(a,simd_himask_8),simd_andc(b,simd_himask_8)));}
#endif

#ifndef simd_sll_4
inline SIMD_type simd_sll_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_sll_8(simd_and(a,simd_himask_8),simd_and(simd_const_4(3),simd_srli_8(b,4)))
	,simd_sll_8(a,simd_and(b,simd_const_8(3))));}
#endif

#ifndef simd_srl_4
inline SIMD_type simd_srl_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_srl_8(a,simd_and(simd_const_4(3),simd_srli_8(b,4)))
	,simd_srl_8(simd_andc(a,simd_himask_8),simd_and(b,simd_const_8(3))));}
#endif

#ifndef simd_rotl_4
inline SIMD_type simd_rotl_4(SIMD_type a,SIMD_type b){
	return simd_or(simd_sll_4(a,b),simd_srl_4(a,simd_sub_4(simd_const_4(4),b)));}
#endif



