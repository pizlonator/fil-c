/*  Idealized SIMD Operations with Cell BE SPE implementations.
    Copyright (C) 2008, Robert D. Cameron
    Licensed to International Characters Inc. 
       under the Academic Free License version 3.0.
*/

/*------------------------------------------------------------*/

#include <limits.h>
#include <stdio.h>
#include "spu_intrinsics.h"


//typedef qword SIMD_type;
#define SIMD_type qword


/*------------------------------------------------------------*/
/* I. SIMD bitwise logical operations */

#define simd_or(b1, b2) si_or(b1, b2)
#define simd_and(b1, b2) si_and(b1, b2)
#define simd_xor(b1, b2) si_xor(b1, b2)
#define simd_nor(b1, b2) si_nor(b1, b2)
#define simd_andc(b1, b2) si_andc(b1, b2)
#define simd_if(cond, then_val, else_val) si_selb(else_val, then_val, cond)
#define simd_not(x) si_nor(x, x)


#define simd_eq_8(a, b) si_ceqb(a, b)
#define simd_eq_16(a, b) si_ceqh(a, b)
#define simd_eq_32(a, b) si_ceq(a, b)

/* Unsigned gt comparisons  */
#define simd_gt_8(a, b) si_clgtb(a, b)
#define simd_gt_16(a, b) si_clgth(a, b)
#define simd_gt_32(a, b) si_clgt(a, b)



/* splat constants */

#define simd_const_8(n) ((SIMD_type) spu_splats((signed char) (n)))

#define simd_const_16(n) ((SIMD_type) spu_splats((signed short) (n)))

#define simd_const_32(n) ((SIMD_type) spu_splats((signed int) (n)))

#define simd_const_4(n) ((SIMD_type) spu_splats((unsigned char) ((n&0xF)*0x11)))

#define simd_const_2(n) ((SIMD_type) spu_splats((unsigned char) ((n&0x3)*0x55)))

#define simd_const_1(n) (n==0 ? simd_const_8(0): simd_const_8(-1))

#define simd_himask_2 simd_const_2(2)
#define simd_himask_4 simd_const_4(0xC)
#define simd_himask_8 simd_const_8(-16)
#define simd_himask_16 simd_const_16(-256)


/* Idealized arithmetic operations with direct implementation by built-in 
   operations for SPU. */

static inline SIMD_type simd_add_16(SIMD_type a, SIMD_type b) {
	return (SIMD_type) si_ah(a, b);
}

#define simd_add_32(a, b) si_a(a,b)
#define simd_sub_16(a, b) si_sfh(b,a)
#define simd_sub_32(a, b) si_sf(b,a)

/* Additional arithmetic operations. */

static inline SIMD_type simd_add_8(SIMD_type a, SIMD_type b) {
	return simd_if(simd_himask_16,
		       simd_add_16(a, simd_and(b, simd_himask_16)),
		       simd_add_16(a, b));
}

#define simd_sub_8(a, b) \
	simd_if(simd_himask_16,\
		simd_sub_16(a, si_and(b, simd_himask_16)),\
		simd_sub_16(a, b))


/* Idealized shift operations with direct implementation by built-in 
   operations for SPU. */

/*Technically, the original simd_sll specification required the following masking,
but we provide more efficient versions without masking and rely on the programmer
not to exceed the fieldwidth.
#define simd_sll_16(r, shft) si_shlh(r, simd_and(simd_const_16(0xF),shft))
#define simd_sll_32(r, shft) si_shl(r, simd_and(simd_const_32(0x1F),shft))*/
#define simd_sll_16(r, shft) si_shlh(r, shft)
#define simd_sll_32(r, shft) si_shl(r, shft)
#define simd_slli_16(r, shft) si_shlhi(r, shft)
#define simd_slli_32(r, shft) si_shli(r, shft)

#define simd_rotl_16(r, shft) si_roth(r, shft)
#define simd_rotl_32(r, shft) si_rot(r, shft)
#define simd_rotli_16(r, shft) si_rothi(r, shft)
#define simd_rotli_32(r, shft) si_roti((r, shft)


#define simd_srl_16(r, shft) si_rothm(r, -shft)
#define simd_srl_32(r, shft) si_rotm(r, -shft)
#define simd_srli_16(r, shft) si_rothmi(r, -shft)
#define simd_srli_32(r, shft) si_rotmi(r, -shft)


#ifndef simd_sll_8
inline SIMD_type simd_sll_8(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_16, simd_sll_16(simd_and(a,simd_himask_16),simd_and(simd_const_8(7),simd_srli_16(b,8)))
	,simd_sll_16(a,simd_and(b,simd_const_16(7))));}
#endif
#ifndef simd_srl_8
inline SIMD_type simd_srl_8(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_16, simd_srl_16(a,simd_and(simd_const_8(7),simd_srli_16(b,8)))
	,simd_srl_16(simd_andc(a,simd_himask_16),simd_and(b,simd_const_16(7))));}
#endif
#ifndef simd_rotl_8
inline SIMD_type simd_rotl_8(SIMD_type a,SIMD_type b){
	return simd_or(simd_sll_8(a,b),simd_srl_8(a,simd_sub_8(simd_const_8(8),b)));}
#endif

/* Additional shift operations. */

#define sl_bytealign(r, shft) \
  ((shft) % 8 == 0 ? r : si_shlqbii(r, shft))

#define simd_slli_128(r, shft) \
  ((shft) < 8 ? sl_bytealign(r, shft):\
   si_shlqbyi(sl_bytealign(r, (shft) % 8), (shft) >> 3))

#define sr_bytealign(r, shft) \
  ((shft) % 8 == 0 ? r : si_rotqmbii(r, 8-shft))

#define simd_srli_128(r, shft) \
  ((shft) < 8 ? sr_bytealign(r, shft):\
   si_rotqmbyi(sr_bytealign(r, (shft) % 8), (16 - ((shft) >> 3))))


#define simd_sll_128(r, shft) si_shlqbybi(si_shlqbi(r, shft), shft)

static inline SIMD_type simd_srl_128(SIMD_type r, SIMD_type shft) {
	return si_rotqmbi(si_rotqmbybi(r, si_from_int(7-si_to_int(shft))), si_from_int(-si_to_int(shft)));
}


#define simd_slli_8(r, shft) simd_and(simd_slli_16(r, shft), simd_const_8((255<<shft)&255))
#define simd_srli_8(r, shft) simd_and(simd_srli_16(r, shft), simd_const_8(255>>shft))


#define sisd_sll(r, shft) simd_sll_128(r, shft)
#define sisd_srl(r, shft) simd_srl_128(r, shft)
#define sisd_slli(r, shft) simd_slli_128(r, shft)
#define sisd_srli(r, shft) simd_srli_128(r, shft)



#define simd_mergeh_8(v1, v2) si_shufb(v1, v2, (qword) ((vec_uchar16){0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23}))
#define simd_mergel_8(v1, v2) si_shufb(v1, v2, (qword) ((vec_uchar16){8,24,9,25,10,26,11,27,12,28,13,29,14,30,15,31}))

#define simd_pack_16(v1, v2) si_shufb(v1, v2, (qword) ((vec_uchar16){1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,31}))
#define simd_pack_32(v1, v2) si_shufb(v1, v2, (qword) ((vec_uchar16){2,3,6,7,10,11,14,15,18,19,22,23,26,27,30,31}))

#define simd_pack_16_ll(a, b) simd_pack_16(a, b)


#ifndef ALTIVEC_USE_EVEN_INDICES
#define simd_pack_16_hh(a, b) \
  simd_pack_16(simd_srli_16(a, 8), simd_srli_16(b, 8))
#endif


#ifdef ALTIVEC_USE_EVEN_INDICES
#define even_byte_indices ((qword) ((vec_uchar16){0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30}))
#define simd_pack_16_hh(a, b) si_shufb(a, b, even_byte_indices)
#endif



#define sisd_from_int(n) si_from_int(n)
#define sisd_to_int(n) si_to_int(n)




#define sisd_store_aligned(r, addr) *((SIMD_type *) (addr)) = r
#define sisd_load_aligned(addr) ((SIMD_type) *((SIMD_type *) (addr)))




#define last_byte_per_word ((SIMD_type) ((vec_uchar16){3,7,11,15,0,0,0,0,0,0,0,0,0,0,0,0}))

static inline int bitblock_bit_count(SIMD_type v) {
	SIMD_type bits_per_byte = si_cntb(v);
	SIMD_type bits_per_word = si_sumb(bits_per_byte, simd_const_8(0));
	return sisd_to_int(si_sumb(si_shufb(bits_per_word, simd_const_8(0), last_byte_per_word),
				   simd_const_8(0)));
}

static inline int bitblock_has_bit(SIMD_type v) {
	int mask = sisd_to_int(si_gbb(simd_eq_8(v, simd_const_8(0))));
	return mask != 0xFFFF;
}

static inline int simd_any_sign_bit_8(SIMD_type v) {
	return sisd_to_int(si_gbb(si_cgtb(simd_const_8(0), v))) != 0;
}

static inline int simd_all_eq_8 (SIMD_type v1, SIMD_type v2){
	int mask = sisd_to_int(si_gbb(simd_eq_8(v1,v2)));
	return mask == 0xFFFF;
}

static inline int count_forward_zeroes(SIMD_type v) {
	SIMD_type u32_zeroes = si_clz(v);
	/* Compress the 4 counts of zeroes into the first 4 bytes. */
	SIMD_type u8_zeroes = si_shufb(u32_zeroes, u32_zeroes, last_byte_per_word);
	/* Which of the words were all zeroes (counts = 32)? */
	SIMD_type u8_mask = simd_gt_8(simd_const_8(32), u8_zeroes);
	SIMD_type cnt_initial = si_clz(u8_mask);
	return sisd_to_int(si_sumb(simd_srl_32(u8_zeroes, sisd_from_int(24 - sisd_to_int(cnt_initial))), simd_const_8(0)));
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
	simd_pack_4(simd_if(simd_himask_2,sisd_srli(a,1),a),\
	simd_if(simd_himask_2,sisd_srli(b,1),b))
#define simd_pack_4(a,b)\
	simd_pack_8(simd_if(simd_himask_4,sisd_srli(a,2),a),\
	simd_if(simd_himask_4,sisd_srli(b,2),b))
#define simd_pack_8(a,b)\
	simd_pack_16(simd_if(simd_himask_8,sisd_srli(a,4),a),\
	simd_if(simd_himask_8,sisd_srli(b,4),b))
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


#define simd_permute(a, b, index) si_shufb(a, b, simd_and(index, simd_const_8(31)))

#define LONG_BIT 32

#define cfzl __builtin_clzl

// Future improvement: use the si_clz for simultaneous counting.

/*
static inline int count_forward_zeroes(SIMD_type bits) {
  union {SIMD_type vec; unsigned long elems[sizeof(SIMD_type)/sizeof(long)];} v;
  v.vec = bits;
  if (v.elems[0] != 0) return cfzl(v.elems[0]);
  else if (v.elems[1] != 0) return LONG_BIT + cfzl(v.elems[1]);
#if LONG_BIT < 64
  else if (v.elems[2] != 0) return 2*LONG_BIT + cfzl(v.elems[2]);
  else if (v.elems[3] != 0) return 3*LONG_BIT + cfzl(v.elems[3]);
#endif
  else return 8*sizeof(SIMD_type);
}
*/

static inline SIMD_type vec_lvsl(int a, unsigned char *b)
{
  return (SIMD_type) simd_add_16((SIMD_type)spu_splats((unsigned char)((a + (int)(b)) & 0xF)), 
			        (SIMD_type)((vec_uchar16){0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}));
}

static inline SIMD_type vec_lvsr(int a, unsigned char *b)
{
  return (SIMD_type) simd_sub_16((SIMD_type)((vec_uchar16){16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31}),
				(SIMD_type)spu_splats((unsigned char)((a + (int)(b)) & 0xF)));
}

static inline SIMD_type vec_ld(int a, unsigned char *b)
{
  return (*((SIMD_type *)(((int)b)+a)));
}

static inline void vec_st(SIMD_type a, int b, SIMD_type *c)
{
  *((SIMD_type *)((unsigned char *)(c)+b)) = a;
}


#define vec_lvsl1(x) vec_lvsl(x, (unsigned char *) 0)
#define vec_lvsr1(x) vec_lvsr(x, (unsigned char *) 0)

#define vec_splat(r, i) spu_splats(spu_extract(r, i))

#define vec_stl vec_st


static inline
SIMD_type simd_sub_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_xor(a,b);
	 SIMD_type borrow = simd_andc(b,a);
	 SIMD_type c2 = simd_xor(c1,(sisd_slli(borrow,1)));
	 return simd_if(simd_himask_2,c2,c1);
}

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

#ifndef simd_sub_4
inline SIMD_type simd_sub_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_sub_8(simd_and(a,simd_himask_8),simd_and(b,simd_himask_8))
	,simd_sub_8(simd_andc(a,simd_himask_8),simd_andc(b,simd_himask_8)));}
#endif

static inline
SIMD_type simd_rotl_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_or((simd_andc(a,b)),(simd_and(b,sisd_srli(a,1))));
	 SIMD_type c2 = simd_or((simd_andc(a,(sisd_slli(b,1)))),(simd_and((sisd_slli(b,1)),(sisd_slli(a,1)))));
	 return simd_if(simd_himask_2,c2,c1);
}

#ifndef simd_rotl_4
inline SIMD_type simd_rotl_4(SIMD_type a,SIMD_type b){
	return simd_or(simd_sll_4(a,b),simd_srl_4(a,simd_sub_4(simd_const_4(4),b)));}
#endif



