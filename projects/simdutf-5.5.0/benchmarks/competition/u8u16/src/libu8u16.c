/*3:*/
#line 197 "libu8u16.w"


size_t
u8u16(char**inbuf,size_t*inbytesleft,char**outbuf,size_t*outbytesleft);

/*:3*//*4:*/
#line 222 "libu8u16.w"

size_t
buffered_u8u16(char**inbuf,size_t*inbytesleft,char**outbuf,size_t*outbytesleft);

/*:4*//*5:*/
#line 253 "libu8u16.w"

#include <stdlib.h> 
#include <errno.h> 
#include <stdint.h> 
#include <string.h> 
/*2:*/
#line 175 "libu8u16.w"

#ifndef U8U16_TARGET
#error "No U8U16_TARGET defined."
#endif

/*:2*//*75:*/
#line 1694 "libu8u16.w"

#if (U8U16_TARGET == ALTIVEC_TARGET)
#include "../lib/altivec_simd.h"
#endif


/*:75*//*88:*/
#line 2020 "libu8u16.w"

#if (U8U16_TARGET == SPU_TARGET)
#include "spu_simd.h"
#include "vmx2spu.h"
#endif




/*:88*//*96:*/
#line 2118 "libu8u16.w"

#if (U8U16_TARGET == MMX_TARGET)
#include "../lib/mmx_simd.h"
#endif

/*:96*//*104:*/
#line 2258 "libu8u16.w"

#if (U8U16_TARGET == SSE_TARGET)
#include "../lib/sse_simd.h"
#endif


/*:104*/
#line 258 "libu8u16.w"

/*8:*/
#line 303 "libu8u16.w"

typedef SIMD_type BytePack;
typedef SIMD_type BitBlock;


/*:8*/
#line 259 "libu8u16.w"

#define align_ceil(addr) ((addr+PACKSIZE-1) &-PACKSIZE)  \

#define PACKSIZE sizeof(SIMD_type) 
#define BLOCKSIZE (sizeof(SIMD_type) *8) 
#define s2p_ideal(s0,s1,s2,s3,s4,s5,s6,s7,p0,p1,p2,p3,p4,p5,p6,p7)  \
{ \
BitBlock bit0123_0,bit0123_1,bit0123_2,bit0123_3, \
bit4567_0,bit4567_1,bit4567_2,bit4567_3; \
BitBlock bit01_0,bit01_1,bit23_0,bit23_1,bit45_0,bit45_1,bit67_0,bit67_1; \
bit0123_0= simd_pack_8_hh(s0,s1) ; \
bit0123_1= simd_pack_8_hh(s2,s3) ; \
bit0123_2= simd_pack_8_hh(s4,s5) ; \
bit0123_3= simd_pack_8_hh(s6,s7) ; \
bit4567_0= simd_pack_8_ll(s0,s1) ; \
bit4567_1= simd_pack_8_ll(s2,s3) ; \
bit4567_2= simd_pack_8_ll(s4,s5) ; \
bit4567_3= simd_pack_8_ll(s6,s7) ; \
bit01_0= simd_pack_4_hh(bit0123_0,bit0123_1) ; \
bit01_1= simd_pack_4_hh(bit0123_2,bit0123_3) ; \
bit23_0= simd_pack_4_ll(bit0123_0,bit0123_1) ; \
bit23_1= simd_pack_4_ll(bit0123_2,bit0123_3) ; \
bit45_0= simd_pack_4_hh(bit4567_0,bit4567_1) ; \
bit45_1= simd_pack_4_hh(bit4567_2,bit4567_3) ; \
bit67_0= simd_pack_4_ll(bit4567_0,bit4567_1) ; \
bit67_1= simd_pack_4_ll(bit4567_2,bit4567_3) ; \
p0= simd_pack_2_hh(bit01_0,bit01_1) ; \
p1= simd_pack_2_ll(bit01_0,bit01_1) ; \
p2= simd_pack_2_hh(bit23_0,bit23_1) ; \
p3= simd_pack_2_ll(bit23_0,bit23_1) ; \
p4= simd_pack_2_hh(bit45_0,bit45_1) ; \
p5= simd_pack_2_ll(bit45_0,bit45_1) ; \
p6= simd_pack_2_hh(bit67_0,bit67_1) ; \
p7= simd_pack_2_ll(bit67_0,bit67_1) ; \
} \

#define p2s_ideal(p0,p1,p2,p3,p4,p5,p6,p7,s0,s1,s2,s3,s4,s5,s6,s7)  \
{ \
BitBlock bit01_r0,bit01_r1,bit23_r0,bit23_r1,bit45_r0,bit45_r1,bit67_r0,bit67_r1; \
BitBlock bit0123_r0,bit0123_r1,bit0123_r2,bit0123_r3, \
bit4567_r0,bit4567_r1,bit4567_r2,bit4567_r3; \
bit01_r0= simd_mergeh_1(p0,p1) ; \
bit01_r1= simd_mergel_1(p0,p1) ; \
bit23_r0= simd_mergeh_1(p2,p3) ; \
bit23_r1= simd_mergel_1(p2,p3) ; \
bit45_r0= simd_mergeh_1(p4,p5) ; \
bit45_r1= simd_mergel_1(p4,p5) ; \
bit67_r0= simd_mergeh_1(p6,p7) ; \
bit67_r1= simd_mergel_1(p6,p7) ; \
bit0123_r0= simd_mergeh_2(bit01_r0,bit23_r0) ; \
bit0123_r1= simd_mergel_2(bit01_r0,bit23_r0) ; \
bit0123_r2= simd_mergeh_2(bit01_r1,bit23_r1) ; \
bit0123_r3= simd_mergel_2(bit01_r1,bit23_r1) ; \
bit4567_r0= simd_mergeh_2(bit45_r0,bit67_r0) ; \
bit4567_r1= simd_mergel_2(bit45_r0,bit67_r0) ; \
bit4567_r2= simd_mergeh_2(bit45_r1,bit67_r1) ; \
bit4567_r3= simd_mergel_2(bit45_r1,bit67_r1) ; \
s0= simd_mergeh_4(bit0123_r0,bit4567_r0) ; \
s1= simd_mergel_4(bit0123_r0,bit4567_r0) ; \
s2= simd_mergeh_4(bit0123_r1,bit4567_r1) ; \
s3= simd_mergel_4(bit0123_r1,bit4567_r1) ; \
s4= simd_mergeh_4(bit0123_r2,bit4567_r2) ; \
s5= simd_mergel_4(bit0123_r2,bit4567_r2) ; \
s6= simd_mergeh_4(bit0123_r3,bit4567_r3) ; \
s7= simd_mergel_4(bit0123_r3,bit4567_r3) ; \
} \

#define p2s_567_ideal(p5,p6,p7,s0,s1,s2,s3,s4,s5,s6,s7)  \
{ \
BitBlock bit45_r0,bit45_r1,bit67_r0,bit67_r1; \
BitBlock bit4567_r0,bit4567_r1,bit4567_r2,bit4567_r3; \
bit45_r0= simd_mergeh_1(simd_const_8(0) ,p5) ; \
bit45_r1= simd_mergel_1(simd_const_8(0) ,p5) ; \
bit67_r0= simd_mergeh_1(p6,p7) ; \
bit67_r1= simd_mergel_1(p6,p7) ; \
bit4567_r0= simd_mergeh_2(bit45_r0,bit67_r0) ; \
bit4567_r1= simd_mergel_2(bit45_r0,bit67_r0) ; \
bit4567_r2= simd_mergeh_2(bit45_r1,bit67_r1) ; \
bit4567_r3= simd_mergel_2(bit45_r1,bit67_r1) ; \
s0= simd_mergeh_4(simd_const_8(0) ,bit4567_r0) ; \
s1= simd_mergel_4(simd_const_8(0) ,bit4567_r0) ; \
s2= simd_mergeh_4(simd_const_8(0) ,bit4567_r1) ; \
s3= simd_mergel_4(simd_const_8(0) ,bit4567_r1) ; \
s4= simd_mergeh_4(simd_const_8(0) ,bit4567_r2) ; \
s5= simd_mergel_4(simd_const_8(0) ,bit4567_r2) ; \
s6= simd_mergeh_4(simd_const_8(0) ,bit4567_r3) ; \
s7= simd_mergel_4(simd_const_8(0) ,bit4567_r3) ; \
} \

#define is_prefix_byte(byte) (byte>=0xC0) 
#define is_prefix3or4_byte(byte) (byte>=0xE0) 
#define is_prefix4_byte(byte) (byte>=0xF0)  \

#define pack_base_addr(addr) ((BytePack*) (((intptr_t) (addr) ) &(-PACKSIZE) ) )  \

#define align_offset(addr) (((intptr_t) addr) &(PACKSIZE-1) ) 
#define unaligned_output_step(reg,bytes)  \
sisd_store_unaligned(reg,(BytePack*) &U16out[u16advance]) ; \
u16advance+= bytes; \

#define is_suffix_byte(byte) (byte>=0x80&&byte<=0xBF)  \

#define s2p_step(s0,s1,hi_mask,shift,p0,p1)  \
{ \
BitBlock t0,t1; \
t0= simd_pack_16_hh(s0,s1) ; \
t1= simd_pack_16_ll(s0,s1) ; \
p0= simd_if(hi_mask,t0,simd_srli_16(t1,shift) ) ; \
p1= simd_if(hi_mask,simd_slli_16(t0,shift) ,t1) ; \
} \

#define s2p_bytepack(s0,s1,s2,s3,s4,s5,s6,s7,p0,p1,p2,p3,p4,p5,p6,p7)  \
{BitBlock bit00224466_0,bit00224466_1,bit00224466_2,bit00224466_3; \
BitBlock bit11335577_0,bit11335577_1,bit11335577_2,bit11335577_3; \
BitBlock bit00004444_0,bit22226666_0,bit00004444_1,bit22226666_1; \
BitBlock bit11115555_0,bit33337777_0,bit11115555_1,bit33337777_1; \
s2p_step(s0,s1,mask_2,1,bit00224466_0,bit11335577_0)  \
s2p_step(s2,s3,mask_2,1,bit00224466_1,bit11335577_1)  \
s2p_step(s4,s5,mask_2,1,bit00224466_2,bit11335577_2)  \
s2p_step(s6,s7,mask_2,1,bit00224466_3,bit11335577_3)  \
s2p_step(bit00224466_0,bit00224466_1,mask_4,2,bit00004444_0,bit22226666_0)  \
s2p_step(bit00224466_2,bit00224466_3,mask_4,2,bit00004444_1,bit22226666_1)  \
s2p_step(bit11335577_0,bit11335577_1,mask_4,2,bit11115555_0,bit33337777_0)  \
s2p_step(bit11335577_2,bit11335577_3,mask_4,2,bit11115555_1,bit33337777_1)  \
s2p_step(bit00004444_0,bit00004444_1,mask_8,4,p0,p4)  \
s2p_step(bit11115555_0,bit11115555_1,mask_8,4,p1,p5)  \
s2p_step(bit22226666_0,bit22226666_1,mask_8,4,p2,p6)  \
s2p_step(bit33337777_0,bit33337777_1,mask_8,4,p3,p7)  \
} \

#define p2s_step(p0,p1,hi_mask,shift,s0,s1)  \
{ \
BitBlock t0,t1; \
t0= simd_if(hi_mask,p0,simd_srli_16(p1,shift) ) ; \
t1= simd_if(hi_mask,simd_slli_16(p0,shift) ,p1) ; \
s0= simd_mergeh_8(t0,t1) ; \
s1= simd_mergel_8(t0,t1) ; \
} \

#define p2s_bytemerge(p0,p1,p2,p3,p4,p5,p6,p7,s0,s1,s2,s3,s4,s5,s6,s7)  \
{ \
BitBlock bit00004444_0,bit22226666_0,bit00004444_1,bit22226666_1; \
BitBlock bit11115555_0,bit33337777_0,bit11115555_1,bit33337777_1; \
BitBlock bit00224466_0,bit00224466_1,bit00224466_2,bit00224466_3; \
BitBlock bit11335577_0,bit11335577_1,bit11335577_2,bit11335577_3; \
p2s_step(p0,p4,simd_himask_8,4,bit00004444_0,bit00004444_1)  \
p2s_step(p1,p5,simd_himask_8,4,bit11115555_0,bit11115555_1)  \
p2s_step(p2,p6,simd_himask_8,4,bit22226666_0,bit22226666_1)  \
p2s_step(p3,p7,simd_himask_8,4,bit33337777_0,bit33337777_1)  \
p2s_step(bit00004444_0,bit22226666_0,simd_himask_4,2,bit00224466_0,bit00224466_1)  \
p2s_step(bit11115555_0,bit33337777_0,simd_himask_4,2,bit11335577_0,bit11335577_1)  \
p2s_step(bit00004444_1,bit22226666_1,simd_himask_4,2,bit00224466_2,bit00224466_3)  \
p2s_step(bit11115555_1,bit33337777_1,simd_himask_4,2,bit11335577_2,bit11335577_3)  \
p2s_step(bit00224466_0,bit11335577_0,simd_himask_2,1,s0,s1)  \
p2s_step(bit00224466_1,bit11335577_1,simd_himask_2,1,s2,s3)  \
p2s_step(bit00224466_2,bit11335577_2,simd_himask_2,1,s4,s5)  \
p2s_step(bit00224466_3,bit11335577_3,simd_himask_2,1,s6,s7)  \
} \

#define p2s_halfstep(p1,hi_mask,shift,s0,s1)  \
{ \
BitBlock t0,t1; \
t0= simd_andc(sisd_srli(p1,shift) ,hi_mask) ; \
t1= simd_andc(p1,hi_mask) ; \
s0= simd_mergeh_8(t0,t1) ; \
s1= simd_mergel_8(t0,t1) ; \
}
#define p2s_567_bytemerge(p5,p6,p7,s0,s1,s2,s3,s4,s5,s6,s7)  \
{ \
BitBlock bit22226666_0,bit22226666_1; \
BitBlock bit11115555_0,bit33337777_0,bit11115555_1,bit33337777_1; \
BitBlock bit00224466_0,bit00224466_1,bit00224466_2,bit00224466_3; \
BitBlock bit11335577_0,bit11335577_1,bit11335577_2,bit11335577_3; \
p2s_halfstep(p5,simd_himask_8,4,bit11115555_0,bit11115555_1)  \
p2s_halfstep(p6,simd_himask_8,4,bit22226666_0,bit22226666_1)  \
p2s_halfstep(p7,simd_himask_8,4,bit33337777_0,bit33337777_1)  \
p2s_halfstep(bit22226666_0,simd_himask_4,2,bit00224466_0,bit00224466_1)  \
p2s_step(bit11115555_0,bit33337777_0,simd_himask_4,2,bit11335577_0,bit11335577_1)  \
p2s_halfstep(bit22226666_1,simd_himask_4,2,bit00224466_2,bit00224466_3)  \
p2s_step(bit11115555_1,bit33337777_1,simd_himask_4,2,bit11335577_2,bit11335577_3)  \
p2s_step(bit00224466_0,bit11335577_0,simd_himask_2,1,s0,s1)  \
p2s_step(bit00224466_1,bit11335577_1,simd_himask_2,1,s2,s3)  \
p2s_step(bit00224466_2,bit11335577_2,simd_himask_2,1,s4,s5)  \
p2s_step(bit00224466_3,bit11335577_3,simd_himask_2,1,s6,s7)  \
} \

#define min(x,y) ((x) <(y) ?(x) :(y) )  \

#define unpack_packed_permutation(packed,high_perm,low_perm)  \
{ \
BitBlock even_perms= simd_srli_8(packed,4) ; \
BitBlock odd_perms= simd_andc(packed,simd_himask_8) ; \
high_perm= simd_mergeh_8(even_perms,odd_perms) ; \
low_perm= simd_mergel_8(even_perms,odd_perms) ; \
} \

#define output_step(vec,vec_num)  \
{ \
BitBlock rshift,lshift; \
rshift= vec_lvsr(u16advance,U16out) ; \
vec_stl(simd_permute(pending,vec,rshift) ,u16advance,U16out) ; \
lshift= simd_add_8(vec_0__15,vec_splat(u16_bytes_8,vec_num) ) ; \
pending= simd_permute(pending,vec,lshift) ; \
u16advance+= dbyte_count[vec_num]; \
} \
 \

#define do_right4_shifts(vec,rshift1,rshift2)  \
{BitBlock s2; \
vec= simd_sub_8(vec,sisd_srli(simd_and(rshift1,vec) ,1) ) ; \
s2= simd_and(rshift2,vec) ; \
vec= simd_or(sisd_srli(s2,2) ,simd_xor(vec,s2) ) ; \
}
#define do_right8_shifts(vec,rshift1,rshift2,rshift4)  \
{BitBlock s2; \
vec= simd_sub_8(vec,simd_srli_16(simd_and(rshift1,vec) ,1) ) ; \
s2= simd_and(rshift2,vec) ; \
vec= simd_or(simd_srli_16(s2,2) ,simd_xor(vec,s2) ) ; \
s2= simd_and(rshift4,vec) ; \
vec= simd_or(simd_srli_16(s2,4) ,simd_xor(vec,s2) ) ; \
}

#line 260 "libu8u16.w"

/*16:*/
#line 387 "libu8u16.w"

#if BYTE_ORDER == BIG_ENDIAN
#define sisd_sfl(blk, n) sisd_srl(blk, n)
#define sisd_sbl(blk, n) sisd_sll(blk, n)
#define sisd_sfli(blk, n) sisd_srli(blk, n)
#define sisd_sbli(blk, n) sisd_slli(blk, n)
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
#define sisd_sfl(blk, n) sisd_sll(blk, n)
#define sisd_sbl(blk, n) sisd_srl(blk, n)
#define sisd_sfli(blk, n) sisd_slli(blk, n)
#define sisd_sbli(blk, n) sisd_srli(blk, n)
#endif

#define bitblock_sfl(blk, n) sisd_sfl(blk, n)
#define bitblock_sbl(blk, n) sisd_sbl(blk, n)
#define bitblock_sfli(blk, n) sisd_sfli(blk, n)
#define bitblock_sbli(blk, n) sisd_sbli(blk, n)

/*:16*//*17:*/
#line 414 "libu8u16.w"

#if BYTE_ORDER == BIG_ENDIAN
#ifdef UTF16_LE
#define u16_merge0(a, b) simd_mergeh_8(b, a)
#define u16_merge1(a, b) simd_mergel_8(b, a)
#endif
#ifndef UTF16_LE
#define u16_merge0(a, b) simd_mergeh_8(a, b)
#define u16_merge1(a, b) simd_mergel_8(a, b)
#endif
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#ifdef UTF16_LE
#define u16_merge0(a, b) simd_mergel_8(a, b)
#define u16_merge1(a, b) simd_mergeh_8(a, b)
#endif
#ifndef UTF16_LE
#define u16_merge0(a, b) simd_mergel_8(b, a)
#define u16_merge1(a, b) simd_mergeh_8(b, a)
#endif
#endif

/*:17*/
#line 261 "libu8u16.w"

size_t
u8u16(char**inbuf,size_t*inbytesleft,char**outbuf,size_t*outbytesleft){
/*9:*/
#line 313 "libu8u16.w"

BytePack U8s0,U8s1,U8s2,U8s3,U8s4,U8s5,U8s6,U8s7;
BitBlock u8bit0,u8bit1,u8bit2,u8bit3,u8bit4,u8bit5,u8bit6,u8bit7;

/*:9*//*10:*/
#line 332 "libu8u16.w"

BitBlock u16hi0,u16hi1,u16hi2,u16hi3,u16hi4,u16hi5,u16hi6,u16hi7;
BitBlock u16lo0,u16lo1,u16lo2,u16lo3,u16lo4,u16lo5,u16lo6,u16lo7;
BytePack U16h0,U16h1,U16h2,U16h3,U16h4,U16h5,U16h6,U16h7;
BytePack U16l0,U16l1,U16l2,U16l3,U16l4,U16l5,U16l6,U16l7;
BytePack U16s0,U16s1,U16s2,U16s3,U16s4,U16s5,U16s6,U16s7,
U16s8,U16s9,U16s10,U16s11,U16s12,U16s13,U16s14,U16s15;

/*:10*//*12:*/
#line 354 "libu8u16.w"

BitBlock input_select_mask;

/*:12*//*13:*/
#line 365 "libu8u16.w"

BitBlock error_mask;

/*:13*//*14:*/
#line 373 "libu8u16.w"

BitBlock delmask;

/*:14*//*25:*/
#line 699 "libu8u16.w"

intptr_t u8advance,u16advance;

/*:25*//*37:*/
#line 1003 "libu8u16.w"

BitBlock u8unibyte,u8prefix,u8suffix,u8prefix2,u8prefix3or4,u8prefix3,u8prefix4;

/*:37*//*42:*/
#line 1046 "libu8u16.w"

BitBlock u8scope22,u8scope32,u8scope33,u8scope42,u8scope43,u8scope44;
BitBlock u8lastsuffix,u8lastbyte,u8surrogate;

/*:42*//*46:*/
#line 1097 "libu8u16.w"

BitBlock suffix_required_scope;

/*:46*//*59:*/
#line 1330 "libu8u16.w"

#ifdef __GNUC__
unsigned char u16_bytes_per_reg[16]__attribute__((aligned(16)));
#endif
#ifdef _MSC_VER
__declspec(align(16))unsigned char u16_bytes_per_reg[16];
#endif
#if ((DOUBLEBYTE_DELETION == FROM_LEFT8) || (BIT_DELETION == ROTATION_TO_LEFT8))
BitBlock delcounts_2,delcounts_4,delcounts_8;
#endif
#if (BIT_DELETION == ROTATION_TO_LEFT8)
BitBlock rotl_2,rotl_4,sll_8;
#endif

/*:59*//*80:*/
#line 1889 "libu8u16.w"

#if ((U8U16_TARGET == ALTIVEC_TARGET) || (U8U16_TARGET == SPU_TARGET))
BitBlock bits_per_nybble_tbl= 
(BitBlock){0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
#endif
#if (BYTE_DELETION == BYTE_DEL_BY_PERMUTE_TO_LEFT8)
BitBlock packed_identity= 
(BitBlock){0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,
0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF};
BitBlock del2_4_shift_tbl= 
(BitBlock){0,0,4,4,0x40,0x40,0x44,0x44,0,0,4,4,0x40,0x40,0x44,0x44};
BitBlock del4_8_rshift_tbl= 
(BitBlock){0,0xFC,0xFC,0xF8,0,0xFC,0xFC,0xF8,
0,0xFC,0xFC,0xF8,0,0xFC,0xFC,0xF8};
BitBlock del4_8_lshift_tbl= 
(BitBlock){0,0,0,0,4,4,4,4,4,4,4,4,8,8,8,8};
BitBlock del8_shift_tbl= 
(BitBlock){0,4,4,8,4,8,8,12,4,8,8,12,8,12,12,16};
BitBlock l8perm0,l8perm1,l8perm2,l8perm3;
#endif
#if ((BIT_DELETION != ROTATION_TO_LEFT8) && (DOUBLEBYTE_DELETION == ALTIVEC_FROM_LEFT8))
BitBlock delcounts_8,u16_bytes_8;
#endif

/*:80*//*86:*/
#line 2009 "libu8u16.w"

#if ((U8U16_TARGET == ALTIVEC_TARGET) || (U8U16_TARGET == SPU_TARGET))
BitBlock start_of_output_existing= vec_ld(0,(vector unsigned char*)*outbuf);
BitBlock pending= simd_permute(start_of_output_existing,
start_of_output_existing,
vec_lvsl(0,(unsigned char*)*outbuf));
#endif


/*:86*//*91:*/
#line 2053 "libu8u16.w"

#if (BIT_DELETION == SHIFT_TO_RIGHT4)
BitBlock del4_rshift1,del4_trans2,del4_rshift2;
#endif

/*:91*//*94:*/
#line 2093 "libu8u16.w"

#if (DOUBLEBYTE_DELETION == FROM_LEFT4)
BitBlock delcounts_2,delcounts_4,u16_bytes_4;
#endif

/*:94*//*100:*/
#line 2166 "libu8u16.w"

#if ((BIT_DELETION == SHIFT_TO_RIGHT8) || (BIT_DELETION == PERMUTE_INDEX_TO_RIGHT8))
BitBlock del8_rshift1,del8_trans2,del8_rshift2,del8_trans4,del8_rshift4;
#endif

/*:100*/
#line 264 "libu8u16.w"

if(inbuf&&*inbuf&&outbuf&&*outbuf){
if(/*6:*/
#line 285 "libu8u16.w"

(intptr_t)*outbuf+*outbytesleft> align_ceil((intptr_t)*outbuf+2*(*inbytesleft))



/*:6*/
#line 266 "libu8u16.w"
)
/*24:*/
#line 669 "libu8u16.w"

{
unsigned char*U8data= (unsigned char*)*inbuf;
unsigned char*U16out= (unsigned char*)*outbuf;
size_t inbytes= *inbytesleft;
while(inbytes> 0){
/*36:*/
#line 921 "libu8u16.w"

#ifndef NO_ASCII_OPTIMIZATION
BitBlock vec_0= simd_const_8(0);
if(inbytes> PACKSIZE){
U8s0= sisd_load_unaligned((BytePack*)U8data);
if(!simd_any_sign_bit_8(U8s0)){
intptr_t fill_to_align= PACKSIZE-align_offset(U16out);
U16s0= u16_merge0(vec_0,U8s0);
sisd_store_unaligned(U16s0,(BytePack*)U16out);
u8advance= fill_to_align/2;
u16advance= fill_to_align;
/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 932 "libu8u16.w"

while(inbytes> 4*PACKSIZE){
BytePack*U8pack= (BytePack*)U8data;
BytePack*U16pack= (BytePack*)U16out;
U8s0= sisd_load_unaligned(U8pack);
U8s1= sisd_load_unaligned(&U8pack[1]);
U8s2= sisd_load_unaligned(&U8pack[2]);
U8s3= sisd_load_unaligned(&U8pack[3]);
if(simd_any_sign_bit_8(simd_or(simd_or(U8s0,U8s1),simd_or(U8s2,U8s3))))break;
sisd_store_aligned(u16_merge0(vec_0,U8s0),U16pack);
sisd_store_aligned(u16_merge1(vec_0,U8s0),&U16pack[1]);
sisd_store_aligned(u16_merge0(vec_0,U8s1),&U16pack[2]);
sisd_store_aligned(u16_merge1(vec_0,U8s1),&U16pack[3]);
sisd_store_aligned(u16_merge0(vec_0,U8s2),&U16pack[4]);
sisd_store_aligned(u16_merge1(vec_0,U8s2),&U16pack[5]);
sisd_store_aligned(u16_merge0(vec_0,U8s3),&U16pack[6]);
sisd_store_aligned(u16_merge1(vec_0,U8s3),&U16pack[7]);
u8advance= 4*PACKSIZE;
u16advance= 8*PACKSIZE;
/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 951 "libu8u16.w"

}
while(inbytes> PACKSIZE){
BytePack*U16pack= (BytePack*)U16out;
U8s0= sisd_load_unaligned((BytePack*)U8data);
if(simd_any_sign_bit_8(U8s0))break;
sisd_store_aligned(u16_merge0(vec_0,U8s0),U16pack);
sisd_store_aligned(u16_merge1(vec_0,U8s0),&U16pack[1]);
u8advance= PACKSIZE;
u16advance= 2*PACKSIZE;
/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 961 "libu8u16.w"

}
}
}
if(inbytes<=PACKSIZE){
intptr_t U8data_offset= ((intptr_t)U8data)&(PACKSIZE-1);
if(U8data_offset+inbytes<=PACKSIZE){

U8s0= sisd_sbl(sisd_load_aligned((BytePack*)pack_base_addr((intptr_t)U8data)),
sisd_from_int(8*U8data_offset));
}
else U8s0= sisd_load_unaligned((BytePack*)U8data);
U8s0= simd_and(U8s0,sisd_sbl(simd_const_8(-1),
sisd_from_int(8*(PACKSIZE-inbytes))));
if(!simd_any_sign_bit_8(U8s0)){
sisd_store_unaligned(u16_merge0(vec_0,U8s0),(BytePack*)U16out);
if(inbytes> PACKSIZE/2)
sisd_store_unaligned(u16_merge1(vec_0,U8s0),(BytePack*)&U16out[PACKSIZE]);
u8advance= inbytes;
u16advance= 2*inbytes;
/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 981 "libu8u16.w"

/*29:*/
#line 763 "libu8u16.w"

*outbytesleft-= (intptr_t)U16out-(intptr_t)*outbuf;
*inbuf= (char*)U8data;
*inbytesleft= inbytes;
*outbuf= (char*)U16out;
/*97:*/
#line 2123 "libu8u16.w"

#if (U8U16_TARGET == MMX_TARGET)
_mm_empty();
#endif

/*:97*/
#line 768 "libu8u16.w"
;
if(inbytes==0)return(size_t)0;
else return(size_t)-1;


/*:29*/
#line 982 "libu8u16.w"

}
}
#endif



/*:36*//*78:*/
#line 1763 "libu8u16.w"

#if ((U8U16_TARGET == ALTIVEC_TARGET) || (U8U16_TARGET == SPU_TARGET))
BitBlock vec_0= simd_const_8(0);
if(inbytes> PACKSIZE){
BitBlock r0,r1,r2,r3,r4;
BitBlock input_shiftl= vec_lvsl(0,U8data);
U8s0= simd_permute(vec_ld(0,U8data),vec_ld(15,U8data),input_shiftl);
if(!simd_any_sign_bit_8(U8s0)){
int fill_to_align= PACKSIZE-align_offset(U16out);
U16s0= u16_merge0(vec_0,U8s0);
pending= simd_permute(pending,U16s0,vec_lvsr(0,U16out));
vec_st(pending,0,U16out);
u8advance= fill_to_align/2;
u16advance= fill_to_align;
/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 1777 "libu8u16.w"

input_shiftl= vec_lvsl(0,U8data);
r0= vec_ld(0,U8data);
while(inbytes> 4*PACKSIZE){
BytePack*U16pack= (BytePack*)U16out;
r1= vec_ld(16,U8data);
r2= vec_ld(32,U8data);
U8s0= simd_permute(r0,r1,input_shiftl);
r3= vec_ld(48,U8data);
U8s1= simd_permute(r1,r2,input_shiftl);
r4= vec_ld(64,U8data);
U8s2= simd_permute(r2,r3,input_shiftl);
U8s3= simd_permute(r3,r4,input_shiftl);
if(simd_any_sign_bit_8(simd_or(simd_or(U8s0,U8s1),simd_or(U8s2,U8s3))))break;
sisd_store_aligned(u16_merge0(vec_0,U8s0),U16pack);
sisd_store_aligned(u16_merge1(vec_0,U8s0),&U16pack[1]);
sisd_store_aligned(u16_merge0(vec_0,U8s1),&U16pack[2]);
sisd_store_aligned(u16_merge1(vec_0,U8s1),&U16pack[3]);
sisd_store_aligned(u16_merge0(vec_0,U8s2),&U16pack[4]);
sisd_store_aligned(u16_merge1(vec_0,U8s2),&U16pack[5]);
sisd_store_aligned(u16_merge0(vec_0,U8s3),&U16pack[6]);
pending= u16_merge1(vec_0,U8s3);
sisd_store_aligned(pending,&U16pack[7]);
u8advance= 4*PACKSIZE;
u16advance= 8*PACKSIZE;
/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 1802 "libu8u16.w"

r0= r4;
}
while(inbytes> PACKSIZE){
BytePack*U16pack= (BytePack*)U16out;
r1= vec_ld(16,U8data);
U8s0= simd_permute(r0,r1,input_shiftl);
if(simd_any_sign_bit_8(U8s0))break;
sisd_store_aligned(u16_merge0(vec_0,U8s0),U16pack);
pending= u16_merge1(vec_0,U8s0);
sisd_store_aligned(pending,&U16pack[1]);
u8advance= PACKSIZE;
u16advance= 2*PACKSIZE;
/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 1815 "libu8u16.w"

r0= r1;
}
}
}
#endif


/*:78*/
#line 675 "libu8u16.w"
;
/*31:*/
#line 776 "libu8u16.w"

if(inbytes<BLOCKSIZE){
input_select_mask= sisd_sbl(simd_const_8(-1),sisd_from_int(BLOCKSIZE-inbytes));
/*35:*/
#line 839 "libu8u16.w"

#ifdef INBUF_READ_NONALIGNED
{
BytePack*U8pack= (BytePack*)U8data;
size_t full_packs= inbytes/PACKSIZE;
size_t excess_bytes= inbytes%PACKSIZE;
intptr_t U8data_offset= ((intptr_t)U8data)%PACKSIZE;
BytePack partial_pack;
if(excess_bytes==0)partial_pack= simd_const_8(0);
else if(U8data_offset+excess_bytes> PACKSIZE)

partial_pack= sisd_load_unaligned(&U8pack[full_packs]);
else{

partial_pack= sisd_load_aligned(pack_base_addr(&U8pack[full_packs]));
partial_pack= sisd_sbl(partial_pack,sisd_from_int(8*U8data_offset));
}
switch(full_packs){
case 0:U8s0= partial_pack;break;
case 1:U8s0= sisd_load_unaligned(&U8pack[0]);
U8s1= partial_pack;
break;
case 2:U8s0= sisd_load_unaligned(&U8pack[0]);
U8s1= sisd_load_unaligned(&U8pack[1]);
U8s2= partial_pack;
break;
case 3:U8s0= sisd_load_unaligned(&U8pack[0]);
U8s1= sisd_load_unaligned(&U8pack[1]);
U8s2= sisd_load_unaligned(&U8pack[2]);
U8s3= partial_pack;
break;
case 4:U8s0= sisd_load_unaligned(&U8pack[0]);
U8s1= sisd_load_unaligned(&U8pack[1]);
U8s2= sisd_load_unaligned(&U8pack[2]);
U8s3= sisd_load_unaligned(&U8pack[3]);
U8s4= partial_pack;
break;
case 5:U8s0= sisd_load_unaligned(&U8pack[0]);
U8s1= sisd_load_unaligned(&U8pack[1]);
U8s2= sisd_load_unaligned(&U8pack[2]);
U8s3= sisd_load_unaligned(&U8pack[3]);
U8s4= sisd_load_unaligned(&U8pack[4]);
U8s5= partial_pack;
break;
case 6:U8s0= sisd_load_unaligned(&U8pack[0]);
U8s1= sisd_load_unaligned(&U8pack[1]);
U8s2= sisd_load_unaligned(&U8pack[2]);
U8s3= sisd_load_unaligned(&U8pack[3]);
U8s4= sisd_load_unaligned(&U8pack[4]);
U8s5= sisd_load_unaligned(&U8pack[5]);
U8s6= partial_pack;
break;
case 7:U8s0= sisd_load_unaligned(&U8pack[0]);
U8s1= sisd_load_unaligned(&U8pack[1]);
U8s2= sisd_load_unaligned(&U8pack[2]);
U8s3= sisd_load_unaligned(&U8pack[3]);
U8s4= sisd_load_unaligned(&U8pack[4]);
U8s5= sisd_load_unaligned(&U8pack[5]);
U8s6= sisd_load_unaligned(&U8pack[6]);
U8s7= partial_pack;
break;
}
input_select_mask= sisd_sbl(simd_const_8(-1),sisd_from_int(BLOCKSIZE-inbytes));
u8advance= inbytes;
}
#endif



/*:35*//*77:*/
#line 1735 "libu8u16.w"

#if ((U8U16_TARGET == ALTIVEC_TARGET) || (U8U16_TARGET == SPU_TARGET))
{
BitBlock r0,r1,r2,r3,r4,r5,r6,r7,r8;
BitBlock input_shiftl= vec_lvsl(0,U8data);
int last_byte= inbytes-1;
r0= vec_ld(0,U8data);
r1= vec_ld(min(16,last_byte),U8data);
r2= vec_ld(min(32,last_byte),U8data);
U8s0= simd_permute(r0,r1,input_shiftl);
r3= vec_ld(min(48,last_byte),U8data);
U8s1= simd_permute(r1,r2,input_shiftl);
r4= vec_ld(min(64,last_byte),U8data);
U8s2= simd_permute(r2,r3,input_shiftl);
r5= vec_ld(min(80,last_byte),U8data);
U8s3= simd_permute(r3,r4,input_shiftl);
r6= vec_ld(min(96,last_byte),U8data);
U8s4= simd_permute(r4,r5,input_shiftl);
r7= vec_ld(min(112,last_byte),U8data);
U8s5= simd_permute(r5,r6,input_shiftl);
r8= vec_ld(min(127,last_byte),U8data);
U8s6= simd_permute(r6,r7,input_shiftl);
U8s7= simd_permute(r7,r8,input_shiftl);
u8advance= inbytes;
}
#endif

/*:77*/
#line 779 "libu8u16.w"

}
else{
input_select_mask= simd_const_8(-1);
/*32:*/
#line 790 "libu8u16.w"

#ifdef INBUF_READ_NONALIGNED
{
BytePack*U8pack= (BytePack*)U8data;
U8s0= sisd_load_unaligned(&U8pack[0]);
U8s1= sisd_load_unaligned(&U8pack[1]);
U8s2= sisd_load_unaligned(&U8pack[2]);
U8s3= sisd_load_unaligned(&U8pack[3]);
U8s4= sisd_load_unaligned(&U8pack[4]);
U8s5= sisd_load_unaligned(&U8pack[5]);
U8s6= sisd_load_unaligned(&U8pack[6]);
U8s7= sisd_load_unaligned(&U8pack[7]);
u8advance= BLOCKSIZE;
/*33:*/
#line 822 "libu8u16.w"

u8advance-= is_prefix_byte(U8data[u8advance-1])
+2*is_prefix3or4_byte(U8data[u8advance-2])
+3*is_prefix4_byte(U8data[u8advance-3]);



/*:33*/
#line 803 "libu8u16.w"

}
#endif

/*:32*//*76:*/
#line 1700 "libu8u16.w"

#if ((U8U16_TARGET == ALTIVEC_TARGET) || (U8U16_TARGET == SPU_TARGET))
{
BitBlock r0,r1,r2,r3,r4,r5,r6,r7,r8;
BitBlock input_shiftl= vec_lvsl(0,U8data);
r0= vec_ld(0,U8data);
r1= vec_ld(16,U8data);
r2= vec_ld(32,U8data);
U8s0= simd_permute(r0,r1,input_shiftl);
r3= vec_ld(48,U8data);
U8s1= simd_permute(r1,r2,input_shiftl);
r4= vec_ld(64,U8data);
U8s2= simd_permute(r2,r3,input_shiftl);
r5= vec_ld(80,U8data);
U8s3= simd_permute(r3,r4,input_shiftl);
r6= vec_ld(96,U8data);
U8s4= simd_permute(r4,r5,input_shiftl);
r7= vec_ld(112,U8data);
U8s5= simd_permute(r5,r6,input_shiftl);

r8= vec_ld(127,U8data);
U8s6= simd_permute(r6,r7,input_shiftl);
U8s7= simd_permute(r7,r8,input_shiftl);
u8advance= BLOCKSIZE;
/*33:*/
#line 822 "libu8u16.w"

u8advance-= is_prefix_byte(U8data[u8advance-1])
+2*is_prefix3or4_byte(U8data[u8advance-2])
+3*is_prefix4_byte(U8data[u8advance-3]);



/*:33*/
#line 1724 "libu8u16.w"

}
#endif


/*:76*/
#line 783 "libu8u16.w"

}


/*:31*/
#line 676 "libu8u16.w"

/*19:*/
#line 510 "libu8u16.w"

#if (S2P_ALGORITHM == S2P_IDEAL)
#if (BYTE_ORDER == BIG_ENDIAN)
s2p_ideal(U8s0,U8s1,U8s2,U8s3,U8s4,U8s5,U8s6,U8s7,
u8bit0,u8bit1,u8bit2,u8bit3,u8bit4,u8bit5,u8bit6,u8bit7)
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
s2p_ideal(U8s7,U8s6,U8s5,U8s4,U8s3,U8s2,U8s1,U8s0,
u8bit0,u8bit1,u8bit2,u8bit3,u8bit4,u8bit5,u8bit6,u8bit7)
#endif
#endif

/*:19*//*70:*/
#line 1572 "libu8u16.w"

#if (S2P_ALGORITHM == S2P_BYTEPACK)
{
BitBlock mask_2= simd_himask_2;
BitBlock mask_4= simd_himask_4;
BitBlock mask_8= simd_himask_8;
#if (BYTE_ORDER == BIG_ENDIAN)
s2p_bytepack(U8s0,U8s1,U8s2,U8s3,U8s4,U8s5,U8s6,U8s7,
u8bit0,u8bit1,u8bit2,u8bit3,u8bit4,u8bit5,u8bit6,u8bit7)
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
s2p_bytepack(U8s7,U8s6,U8s5,U8s4,U8s3,U8s2,U8s1,U8s0,
u8bit0,u8bit1,u8bit2,u8bit3,u8bit4,u8bit5,u8bit6,u8bit7)
#endif
}
#endif


/*:70*/
#line 677 "libu8u16.w"

/*28:*/
#line 726 "libu8u16.w"

/*38:*/
#line 1011 "libu8u16.w"

{
BitBlock bit0_selected= simd_and(input_select_mask,u8bit0);
u8unibyte= simd_andc(input_select_mask,u8bit0);
u8prefix= simd_and(bit0_selected,u8bit1);
u8suffix= simd_andc(bit0_selected,u8bit1);
u8prefix3or4= simd_and(u8prefix,u8bit2);
u8prefix2= simd_andc(u8prefix,u8bit2);
u8prefix3= simd_andc(u8prefix3or4,u8bit3);
u8prefix4= simd_and(u8prefix3or4,u8bit3);
}

/*:38*/
#line 727 "libu8u16.w"

/*43:*/
#line 1053 "libu8u16.w"

u8scope22= bitblock_sfli(u8prefix2,1);
u8scope33= bitblock_sfli(u8prefix3,2);
u8scope44= bitblock_sfli(u8prefix4,3);
u8lastsuffix= simd_or(simd_or(u8scope22,u8scope33),u8scope44);
u8lastbyte= simd_or(u8unibyte,u8lastsuffix);

/*:43*/
#line 728 "libu8u16.w"

/*47:*/
#line 1110 "libu8u16.w"

error_mask= simd_andc(u8prefix2,simd_or(simd_or(u8bit3,u8bit4),
simd_or(u8bit5,u8bit6)));
suffix_required_scope= u8scope22;

/*:47*/
#line 729 "libu8u16.w"

/*52:*/
#line 1185 "libu8u16.w"

u16hi5= simd_and(u8lastsuffix,bitblock_sfli(u8bit3,1));
u16hi6= simd_and(u8lastsuffix,bitblock_sfli(u8bit4,1));
u16hi7= simd_and(u8lastsuffix,bitblock_sfli(u8bit5,1));
u16lo0= simd_and(u8lastsuffix,bitblock_sfli(u8bit6,1));
u16lo1= simd_or(simd_and(u8unibyte,u8bit1),simd_and(u8lastsuffix,bitblock_sfli(u8bit7,1)));
u16lo2= simd_and(u8lastbyte,u8bit2);
u16lo3= simd_and(u8lastbyte,u8bit3);
u16lo4= simd_and(u8lastbyte,u8bit4);
u16lo5= simd_and(u8lastbyte,u8bit5);
u16lo6= simd_and(u8lastbyte,u8bit6);
u16lo7= simd_and(u8lastbyte,u8bit7);

/*:52*/
#line 730 "libu8u16.w"

/*55:*/
#line 1262 "libu8u16.w"

delmask= simd_not(simd_and(input_select_mask,u8lastbyte));

/*:55*/
#line 731 "libu8u16.w"

#ifndef NO_OPTIMIZATION
if(/*39:*/
#line 1027 "libu8u16.w"

bitblock_has_bit(u8prefix3or4)

/*:39*/
#line 733 "libu8u16.w"
){
/*44:*/
#line 1063 "libu8u16.w"

u8scope32= bitblock_sfli(u8prefix3,1);

/*:44*/
#line 734 "libu8u16.w"

/*48:*/
#line 1120 "libu8u16.w"

{
BitBlock prefix_E0ED,E0ED_constraint;
prefix_E0ED= simd_andc(u8prefix3,
simd_or(simd_or(u8bit6,simd_xor(u8bit4,u8bit7)),
simd_xor(u8bit4,u8bit5)));
E0ED_constraint= simd_xor(bitblock_sfli(u8bit5,1),u8bit2);
error_mask= simd_or(error_mask,
simd_andc(bitblock_sfli(prefix_E0ED,1),E0ED_constraint));
suffix_required_scope= simd_or(u8lastsuffix,u8scope32);
}

/*:48*/
#line 735 "libu8u16.w"

/*53:*/
#line 1202 "libu8u16.w"

u16hi0= simd_and(u8scope33,bitblock_sfli(u8bit4,2));
u16hi1= simd_and(u8scope33,bitblock_sfli(u8bit5,2));
u16hi2= simd_and(u8scope33,bitblock_sfli(u8bit6,2));
u16hi3= simd_and(u8scope33,bitblock_sfli(u8bit7,2));
u16hi4= simd_and(u8scope33,bitblock_sfli(u8bit2,1));

/*:53*/
#line 736 "libu8u16.w"

if(/*40:*/
#line 1034 "libu8u16.w"

bitblock_has_bit(u8prefix4)

/*:40*/
#line 737 "libu8u16.w"
){
/*45:*/
#line 1068 "libu8u16.w"

u8scope42= bitblock_sfli(u8prefix4,1);
u8scope43= bitblock_sfli(u8prefix4,2);
u8surrogate= simd_or(u8scope42,u8scope44);



/*:45*/
#line 738 "libu8u16.w"

/*49:*/
#line 1137 "libu8u16.w"

{
BitBlock prefix_F5FF,prefix_F0F4,F0F4_constraint;
prefix_F5FF= simd_and(u8prefix4,simd_or(u8bit4,
simd_and(u8bit5,
simd_or(u8bit6,u8bit7))));
error_mask= simd_or(error_mask,prefix_F5FF);
prefix_F0F4= simd_andc(u8prefix4,simd_or(u8bit4,simd_or(u8bit6,u8bit7)));
F0F4_constraint= simd_xor(bitblock_sfli(u8bit5,1),simd_or(u8bit2,u8bit3));
error_mask= simd_or(error_mask,simd_andc(bitblock_sfli(prefix_F0F4,1),F0F4_constraint));
suffix_required_scope= simd_or(suffix_required_scope,
simd_or(u8surrogate,u8scope43));
}

/*:49*/
#line 739 "libu8u16.w"

/*54:*/
#line 1221 "libu8u16.w"

{BitBlock borrow1,borrow2;
u16hi0= simd_or(u16hi0,u8surrogate);
u16hi1= simd_or(u16hi1,u8surrogate);
u16hi3= simd_or(u16hi3,u8surrogate);
u16hi4= simd_or(u16hi4,u8surrogate);
u16hi5= simd_or(u16hi5,u8scope44);
u16lo1= simd_or(u16lo1,simd_and(u8scope42,simd_not(u8bit3)));

u16lo0= simd_or(u16lo0,simd_and(u8scope42,simd_xor(u8bit2,u16lo1)));
borrow1= simd_andc(u16lo1,u8bit2);
u16hi7= simd_or(u16hi7,simd_and(u8scope42,simd_xor(bitblock_sfli(u8bit7,1),borrow1)));
borrow2= simd_andc(borrow1,bitblock_sfli(u8bit7,1));
u16hi6= simd_or(u16hi6,simd_and(u8scope42,simd_xor(bitblock_sfli(u8bit6,1),borrow2)));
u16lo2= simd_or(u16lo2,simd_and(u8scope42,u8bit4));
u16lo3= simd_or(u16lo3,simd_and(u8scope42,u8bit5));
u16lo4= simd_or(u16lo4,simd_and(u8scope42,u8bit6));
u16lo5= simd_or(u16lo5,simd_and(u8scope42,u8bit7));
u16lo6= simd_or(u16lo6,simd_and(u8scope42,bitblock_sbli(u8bit2,1)));
u16lo7= simd_or(u16lo7,simd_and(u8scope42,bitblock_sbli(u8bit3,1)));
}


/*:54*/
#line 740 "libu8u16.w"

/*56:*/
#line 1270 "libu8u16.w"

{BitBlock scope42_selected= bitblock_sbli(simd_and(u8scope44,input_select_mask),2);
delmask= simd_not(simd_and(input_select_mask,
simd_or(u8lastbyte,scope42_selected)));
}

/*:56*/
#line 741 "libu8u16.w"

}
}
#endif
#ifdef NO_OPTIMIZATION
/*44:*/
#line 1063 "libu8u16.w"

u8scope32= bitblock_sfli(u8prefix3,1);

/*:44*/
#line 746 "libu8u16.w"

/*48:*/
#line 1120 "libu8u16.w"

{
BitBlock prefix_E0ED,E0ED_constraint;
prefix_E0ED= simd_andc(u8prefix3,
simd_or(simd_or(u8bit6,simd_xor(u8bit4,u8bit7)),
simd_xor(u8bit4,u8bit5)));
E0ED_constraint= simd_xor(bitblock_sfli(u8bit5,1),u8bit2);
error_mask= simd_or(error_mask,
simd_andc(bitblock_sfli(prefix_E0ED,1),E0ED_constraint));
suffix_required_scope= simd_or(u8lastsuffix,u8scope32);
}

/*:48*/
#line 747 "libu8u16.w"

/*53:*/
#line 1202 "libu8u16.w"

u16hi0= simd_and(u8scope33,bitblock_sfli(u8bit4,2));
u16hi1= simd_and(u8scope33,bitblock_sfli(u8bit5,2));
u16hi2= simd_and(u8scope33,bitblock_sfli(u8bit6,2));
u16hi3= simd_and(u8scope33,bitblock_sfli(u8bit7,2));
u16hi4= simd_and(u8scope33,bitblock_sfli(u8bit2,1));

/*:53*/
#line 748 "libu8u16.w"

/*45:*/
#line 1068 "libu8u16.w"

u8scope42= bitblock_sfli(u8prefix4,1);
u8scope43= bitblock_sfli(u8prefix4,2);
u8surrogate= simd_or(u8scope42,u8scope44);



/*:45*/
#line 749 "libu8u16.w"

/*49:*/
#line 1137 "libu8u16.w"

{
BitBlock prefix_F5FF,prefix_F0F4,F0F4_constraint;
prefix_F5FF= simd_and(u8prefix4,simd_or(u8bit4,
simd_and(u8bit5,
simd_or(u8bit6,u8bit7))));
error_mask= simd_or(error_mask,prefix_F5FF);
prefix_F0F4= simd_andc(u8prefix4,simd_or(u8bit4,simd_or(u8bit6,u8bit7)));
F0F4_constraint= simd_xor(bitblock_sfli(u8bit5,1),simd_or(u8bit2,u8bit3));
error_mask= simd_or(error_mask,simd_andc(bitblock_sfli(prefix_F0F4,1),F0F4_constraint));
suffix_required_scope= simd_or(suffix_required_scope,
simd_or(u8surrogate,u8scope43));
}

/*:49*/
#line 750 "libu8u16.w"

/*54:*/
#line 1221 "libu8u16.w"

{BitBlock borrow1,borrow2;
u16hi0= simd_or(u16hi0,u8surrogate);
u16hi1= simd_or(u16hi1,u8surrogate);
u16hi3= simd_or(u16hi3,u8surrogate);
u16hi4= simd_or(u16hi4,u8surrogate);
u16hi5= simd_or(u16hi5,u8scope44);
u16lo1= simd_or(u16lo1,simd_and(u8scope42,simd_not(u8bit3)));

u16lo0= simd_or(u16lo0,simd_and(u8scope42,simd_xor(u8bit2,u16lo1)));
borrow1= simd_andc(u16lo1,u8bit2);
u16hi7= simd_or(u16hi7,simd_and(u8scope42,simd_xor(bitblock_sfli(u8bit7,1),borrow1)));
borrow2= simd_andc(borrow1,bitblock_sfli(u8bit7,1));
u16hi6= simd_or(u16hi6,simd_and(u8scope42,simd_xor(bitblock_sfli(u8bit6,1),borrow2)));
u16lo2= simd_or(u16lo2,simd_and(u8scope42,u8bit4));
u16lo3= simd_or(u16lo3,simd_and(u8scope42,u8bit5));
u16lo4= simd_or(u16lo4,simd_and(u8scope42,u8bit6));
u16lo5= simd_or(u16lo5,simd_and(u8scope42,u8bit7));
u16lo6= simd_or(u16lo6,simd_and(u8scope42,bitblock_sbli(u8bit2,1)));
u16lo7= simd_or(u16lo7,simd_and(u8scope42,bitblock_sbli(u8bit3,1)));
}


/*:54*/
#line 751 "libu8u16.w"

/*56:*/
#line 1270 "libu8u16.w"

{BitBlock scope42_selected= bitblock_sbli(simd_and(u8scope44,input_select_mask),2);
delmask= simd_not(simd_and(input_select_mask,
simd_or(u8lastbyte,scope42_selected)));
}

/*:56*/
#line 752 "libu8u16.w"

#endif

/*50:*/
#line 1154 "libu8u16.w"

error_mask= simd_or(error_mask,simd_xor(suffix_required_scope,u8suffix));

/*:50*/
#line 755 "libu8u16.w"



/*:28*/
#line 678 "libu8u16.w"

/*57:*/
#line 1304 "libu8u16.w"

/*60:*/
#line 1344 "libu8u16.w"

#if ((DOUBLEBYTE_DELETION == FROM_LEFT8) || (BIT_DELETION == ROTATION_TO_LEFT8))
delcounts_2= simd_add_2_lh(delmask,delmask);
delcounts_4= simd_add_4_lh(delcounts_2,delcounts_2);
delcounts_8= simd_add_8_lh(delcounts_4,delcounts_4);
sisd_store_aligned(simd_slli_8(simd_sub_8(simd_const_8(8),delcounts_8),1),
(BytePack*)&u16_bytes_per_reg[0]);
#endif
#if (BIT_DELETION == ROTATION_TO_LEFT8)
rotl_2= simd_if(simd_himask_4,delmask,sisd_srli(delmask,1));
rotl_4= simd_if(simd_himask_8,simd_sub_2(vec_0,delcounts_2),sisd_srli(delcounts_2,2));
sll_8= sisd_srli(delcounts_4,4);
#endif

/*:60*//*79:*/
#line 1844 "libu8u16.w"

#if (BYTE_DELETION == BYTE_DEL_BY_PERMUTE_TO_LEFT8)
{
BitBlock d0,d1,q0,q1,p0,p1;
BitBlock delmask_hi4= simd_srli_8(delmask,4);

d0= simd_permute(del2_4_shift_tbl,del2_4_shift_tbl,delmask_hi4);
d1= simd_permute(del2_4_shift_tbl,del2_4_shift_tbl,delmask);
q0= simd_mergeh_8(d0,d1);
q1= simd_mergel_8(d0,d1);
p0= simd_srli_8(q0,4);
p1= simd_srli_8(q1,4);
l8perm0= simd_rotl_8(packed_identity,simd_mergeh_8(p0,q0));
l8perm1= simd_rotl_8(packed_identity,simd_mergel_8(p0,q0));
l8perm2= simd_rotl_8(packed_identity,simd_mergeh_8(p1,q1));
l8perm3= simd_rotl_8(packed_identity,simd_mergel_8(p1,q1));

d0= simd_permute(del4_8_rshift_tbl,del4_8_rshift_tbl,delmask_hi4);
d1= simd_permute(del4_8_lshift_tbl,del4_8_lshift_tbl,delmask);
p0= simd_mergeh_8(d0,d1);
p1= simd_mergel_8(d0,d1);
l8perm0= simd_rotl_16(l8perm0,simd_mergeh_8(simd_const_8(0),p0));
l8perm1= simd_rotl_16(l8perm1,simd_mergel_8(simd_const_8(0),p0));
l8perm2= simd_rotl_16(l8perm2,simd_mergeh_8(simd_const_8(0),p1));
l8perm3= simd_rotl_16(l8perm3,simd_mergel_8(simd_const_8(0),p1));

d0= simd_permute(del8_shift_tbl,del8_shift_tbl,delmask_hi4);
p0= simd_mergeh_8(simd_const_8(0),d0);
p1= simd_mergel_8(simd_const_8(0),d0);
l8perm0= simd_rotl_32(l8perm0,simd_mergeh_8(simd_const_8(0),p0));
l8perm1= simd_rotl_32(l8perm1,simd_mergel_8(simd_const_8(0),p0));
l8perm2= simd_rotl_32(l8perm2,simd_mergeh_8(simd_const_8(0),p1));
l8perm3= simd_rotl_32(l8perm3,simd_mergel_8(simd_const_8(0),p1));
}
#endif
#if (DOUBLEBYTE_DELETION == ALTIVEC_FROM_LEFT8)
{
BitBlock delmask_hi4= simd_srli_8(delmask,4);
delcounts_8= simd_add_8(simd_permute(bits_per_nybble_tbl,bits_per_nybble_tbl,delmask_hi4),
simd_permute(bits_per_nybble_tbl,bits_per_nybble_tbl,delmask));
u16_bytes_8= simd_slli_8(simd_sub_8(simd_const_8(8),delcounts_8),1);
}
#endif

/*:79*//*90:*/
#line 2037 "libu8u16.w"

#if (BIT_DELETION == SHIFT_TO_RIGHT4)
del4_rshift1= simd_xor(simd_slli_4(delmask,1),simd_slli_4(delmask,2));
del4_rshift1= simd_xor(del4_rshift1,simd_slli_4(del4_rshift1,2));

del4_trans2= simd_and(del4_rshift1,delmask);

del4_rshift2= simd_xor(simd_slli_4(del4_trans2,1),simd_slli_4(del4_trans2,2));
del4_rshift2= simd_xor(del4_rshift2,simd_slli_4(del4_rshift2,2));

del4_rshift1= simd_andc(del4_rshift1,delmask);
del4_rshift2= simd_andc(del4_rshift2,delmask);

del4_rshift2= simd_add_4(simd_and(del4_rshift1,del4_rshift2),del4_rshift2);
#endif

/*:90*//*95:*/
#line 2098 "libu8u16.w"

#if (DOUBLEBYTE_DELETION == FROM_LEFT4)
delcounts_2= simd_add_2_lh(delmask,delmask);
delcounts_4= simd_add_4_lh(delcounts_2,delcounts_2);
u16_bytes_4= sisd_slli(simd_sub_8(simd_const_4(4),delcounts_4),1);

#if BYTE_ORDER == BIG_ENDIAN
sisd_store_aligned(simd_mergeh_4(simd_const_4(0),u16_bytes_4),
&u16_bytes_per_reg[0]);
sisd_store_aligned(simd_mergel_4(simd_const_4(0),u16_bytes_4),
&u16_bytes_per_reg[8]);
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
sisd_store_aligned(simd_mergel_4(simd_const_4(0),u16_bytes_4),
&u16_bytes_per_reg[0]);
sisd_store_aligned(simd_mergeh_4(simd_const_4(0),u16_bytes_4),
&u16_bytes_per_reg[8]);
#endif
#endif

/*:95*//*99:*/
#line 2138 "libu8u16.w"

#if ((BIT_DELETION == SHIFT_TO_RIGHT8) || (BIT_DELETION == PERMUTE_INDEX_TO_RIGHT8))
del8_rshift1= simd_xor(simd_slli_8(delmask,1),simd_slli_8(delmask,2));
del8_rshift1= simd_xor(del8_rshift1,simd_slli_8(del8_rshift1,2));
del8_rshift1= simd_xor(del8_rshift1,simd_slli_8(del8_rshift1,4));

del8_trans2= simd_and(del8_rshift1,delmask);

del8_rshift2= simd_xor(simd_slli_8(del8_trans2,1),simd_slli_8(del8_trans2,2));
del8_rshift2= simd_xor(del8_rshift2,simd_slli_8(del8_rshift2,2));
del8_rshift2= simd_xor(del8_rshift2,simd_slli_8(del8_rshift2,4));

del8_trans4= simd_and(del8_rshift2,del8_trans2);
del8_rshift4= simd_xor(simd_slli_8(del8_trans4,1),simd_slli_8(del8_trans4,2));
del8_rshift4= simd_xor(del8_rshift4,simd_slli_8(del8_rshift4,2));
del8_rshift4= simd_xor(del8_rshift4,simd_slli_8(del8_rshift4,4));

del8_rshift1= simd_andc(del8_rshift1,delmask);
del8_rshift2= simd_andc(del8_rshift2,delmask);
del8_rshift4= simd_andc(del8_rshift4,delmask);

del8_rshift2= simd_sub_8(del8_rshift2,simd_srli_16(simd_and(del8_rshift1,del8_rshift2),1));

del8_rshift4= simd_sub_8(del8_rshift4,simd_srli_16(simd_and(del8_rshift1,del8_rshift4),1));
{BitBlock shift_bits= simd_and(del8_rshift2,del8_rshift4);
del8_rshift4= simd_or(simd_srli_16(shift_bits,2),simd_xor(del8_rshift4,shift_bits));}
#endif

/*:99*/
#line 1305 "libu8u16.w"

/*63:*/
#line 1372 "libu8u16.w"

#if (BIT_DELETION == ROTATION_TO_LEFT8)
u16hi5= simd_sll_8(simd_rotl_4(simd_rotl_2(u16hi5,rotl_2),rotl_4),sll_8);
u16hi6= simd_sll_8(simd_rotl_4(simd_rotl_2(u16hi6,rotl_2),rotl_4),sll_8);
u16hi7= simd_sll_8(simd_rotl_4(simd_rotl_2(u16hi7,rotl_2),rotl_4),sll_8);
u16lo0= simd_sll_8(simd_rotl_4(simd_rotl_2(u16lo0,rotl_2),rotl_4),sll_8);
u16lo1= simd_sll_8(simd_rotl_4(simd_rotl_2(u16lo1,rotl_2),rotl_4),sll_8);
u16lo2= simd_sll_8(simd_rotl_4(simd_rotl_2(u16lo2,rotl_2),rotl_4),sll_8);
u16lo3= simd_sll_8(simd_rotl_4(simd_rotl_2(u16lo3,rotl_2),rotl_4),sll_8);
u16lo4= simd_sll_8(simd_rotl_4(simd_rotl_2(u16lo4,rotl_2),rotl_4),sll_8);
u16lo5= simd_sll_8(simd_rotl_4(simd_rotl_2(u16lo5,rotl_2),rotl_4),sll_8);
u16lo6= simd_sll_8(simd_rotl_4(simd_rotl_2(u16lo6,rotl_2),rotl_4),sll_8);
u16lo7= simd_sll_8(simd_rotl_4(simd_rotl_2(u16lo7,rotl_2),rotl_4),sll_8);
#endif



/*:63*//*82:*/
#line 1919 "libu8u16.w"

#if (BYTE_DELETION == BYTE_DEL_BY_PERMUTE_TO_LEFT8)
{
}
#endif

/*:82*//*93:*/
#line 2078 "libu8u16.w"

#if (BIT_DELETION == SHIFT_TO_RIGHT4)
do_right4_shifts(u16hi5,del4_rshift1,del4_rshift2)
do_right4_shifts(u16hi6,del4_rshift1,del4_rshift2)
do_right4_shifts(u16hi7,del4_rshift1,del4_rshift2)
do_right4_shifts(u16lo0,del4_rshift1,del4_rshift2)
do_right4_shifts(u16lo1,del4_rshift1,del4_rshift2)
do_right4_shifts(u16lo2,del4_rshift1,del4_rshift2)
do_right4_shifts(u16lo3,del4_rshift1,del4_rshift2)
do_right4_shifts(u16lo4,del4_rshift1,del4_rshift2)
do_right4_shifts(u16lo5,del4_rshift1,del4_rshift2)
do_right4_shifts(u16lo6,del4_rshift1,del4_rshift2)
do_right4_shifts(u16lo7,del4_rshift1,del4_rshift2)
#endif

/*:93*//*102:*/
#line 2192 "libu8u16.w"

#if (BIT_DELETION == SHIFT_TO_RIGHT8)
do_right8_shifts(u16hi5,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16hi6,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16hi7,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16lo0,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16lo1,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16lo2,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16lo3,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16lo4,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16lo5,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16lo6,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16lo7,del8_rshift1,del8_rshift2,del8_rshift4)
#endif



/*:102*/
#line 1306 "libu8u16.w"

if(!/*39:*/
#line 1027 "libu8u16.w"

bitblock_has_bit(u8prefix3or4)

/*:39*/
#line 1307 "libu8u16.w"
){
/*22:*/
#line 612 "libu8u16.w"

#if (P2S_ALGORITHM == P2S_IDEAL)
#if (BYTE_ORDER == BIG_ENDIAN)
p2s_567_ideal(u16hi5,u16hi6,u16hi7,
U16h0,U16h1,U16h2,U16h3,U16h4,U16h5,U16h6,U16h7)
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
p2s_567_ideal(u16hi5,u16hi6,u16hi7,
U16h7,U16h6,U16h5,U16h4,U16h3,U16h2,U16h1,U16h0)
#endif
#endif

/*:22*//*73:*/
#line 1679 "libu8u16.w"

#if (P2S_ALGORITHM == P2S_BYTEMERGE)
#if (BYTE_ORDER == BIG_ENDIAN)
p2s_567_bytemerge(u16hi5,u16hi6,u16hi7,
U16h0,U16h1,U16h2,U16h3,U16h4,U16h5,U16h6,U16h7)
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
p2s_567_bytemerge(u16hi5,u16hi6,u16hi7,
U16h7,U16h6,U16h5,U16h4,U16h3,U16h2,U16h1,U16h0)
#endif
#endif


/*:73*/
#line 1308 "libu8u16.w"

}
else{
/*62:*/
#line 1363 "libu8u16.w"

#if (BIT_DELETION == ROTATION_TO_LEFT8)
u16hi0= simd_sll_8(simd_rotl_4(simd_rotl_2(u16hi0,rotl_2),rotl_4),sll_8);
u16hi1= simd_sll_8(simd_rotl_4(simd_rotl_2(u16hi1,rotl_2),rotl_4),sll_8);
u16hi2= simd_sll_8(simd_rotl_4(simd_rotl_2(u16hi2,rotl_2),rotl_4),sll_8);
u16hi3= simd_sll_8(simd_rotl_4(simd_rotl_2(u16hi3,rotl_2),rotl_4),sll_8);
u16hi4= simd_sll_8(simd_rotl_4(simd_rotl_2(u16hi4,rotl_2),rotl_4),sll_8);
#endif

/*:62*//*81:*/
#line 1913 "libu8u16.w"

#if (BYTE_DELETION == BYTE_DEL_BY_PERMUTE_TO_LEFT8)
{
}
#endif

/*:81*//*92:*/
#line 2069 "libu8u16.w"

#if (BIT_DELETION == SHIFT_TO_RIGHT4)
do_right4_shifts(u16hi0,del4_rshift1,del4_rshift2)
do_right4_shifts(u16hi1,del4_rshift1,del4_rshift2)
do_right4_shifts(u16hi2,del4_rshift1,del4_rshift2)
do_right4_shifts(u16hi3,del4_rshift1,del4_rshift2)
do_right4_shifts(u16hi4,del4_rshift1,del4_rshift2)
#endif

/*:92*//*101:*/
#line 2184 "libu8u16.w"

#if (BIT_DELETION == SHIFT_TO_RIGHT8)
do_right8_shifts(u16hi0,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16hi1,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16hi2,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16hi3,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(u16hi4,del8_rshift1,del8_rshift2,del8_rshift4)
#endif
/*:101*/
#line 1311 "libu8u16.w"

/*20:*/
#line 559 "libu8u16.w"

#if (P2S_ALGORITHM == P2S_IDEAL)
#if (BYTE_ORDER == BIG_ENDIAN)
p2s_ideal(u16hi0,u16hi1,u16hi2,u16hi3,u16hi4,u16hi5,u16hi6,u16hi7,
U16h0,U16h1,U16h2,U16h3,U16h4,U16h5,U16h6,U16h7)
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
p2s_ideal(u16hi0,u16hi1,u16hi2,u16hi3,u16hi4,u16hi5,u16hi6,u16hi7,
U16h7,U16h6,U16h5,U16h4,U16h3,U16h2,U16h1,U16h0)
#endif
#endif

/*:20*//*71:*/
#line 1621 "libu8u16.w"

#if (P2S_ALGORITHM == P2S_BYTEMERGE)
#if (BYTE_ORDER == BIG_ENDIAN)
p2s_bytemerge(u16hi0,u16hi1,u16hi2,u16hi3,u16hi4,u16hi5,u16hi6,u16hi7,
U16h0,U16h1,U16h2,U16h3,U16h4,U16h5,U16h6,U16h7)
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
p2s_bytemerge(u16hi0,u16hi1,u16hi2,u16hi3,u16hi4,u16hi5,u16hi6,u16hi7,
U16h7,U16h6,U16h5,U16h4,U16h3,U16h2,U16h1,U16h0)
#endif
#endif

/*:71*/
#line 1312 "libu8u16.w"

}
/*21:*/
#line 571 "libu8u16.w"

#if (P2S_ALGORITHM == P2S_IDEAL)
#if (BYTE_ORDER == BIG_ENDIAN)
p2s_ideal(u16lo0,u16lo1,u16lo2,u16lo3,u16lo4,u16lo5,u16lo6,u16lo7,
U16l0,U16l1,U16l2,U16l3,U16l4,U16l5,U16l6,U16l7)
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
p2s_ideal(u16lo0,u16lo1,u16lo2,u16lo3,u16lo4,u16lo5,u16lo6,u16lo7,
U16l7,U16l6,U16l5,U16l4,U16l3,U16l2,U16l1,U16l0)
#endif
#endif


/*:21*//*72:*/
#line 1633 "libu8u16.w"

#if (P2S_ALGORITHM == P2S_BYTEMERGE)
#if (BYTE_ORDER == BIG_ENDIAN)
p2s_bytemerge(u16lo0,u16lo1,u16lo2,u16lo3,u16lo4,u16lo5,u16lo6,u16lo7,
U16l0,U16l1,U16l2,U16l3,U16l4,U16l5,U16l6,U16l7)
#endif
#if (BYTE_ORDER == LITTLE_ENDIAN)
p2s_bytemerge(u16lo0,u16lo1,u16lo2,u16lo3,u16lo4,u16lo5,u16lo6,u16lo7,
U16l7,U16l6,U16l5,U16l4,U16l3,U16l2,U16l1,U16l0)
#endif
#endif


/*:72*/
#line 1314 "libu8u16.w"

/*64:*/
#line 1389 "libu8u16.w"




/*:64*//*83:*/
#line 1934 "libu8u16.w"

#if (BYTE_DELETION == BYTE_DEL_BY_PERMUTE_TO_LEFT8)
{
BitBlock high_perm,low_perm;
unpack_packed_permutation(l8perm0,high_perm,low_perm)
U16l0= simd_permute(U16l0,U16l0,high_perm);
U16h0= simd_permute(U16h0,U16h0,high_perm);
U16l1= simd_permute(U16l1,U16l1,low_perm);
U16h1= simd_permute(U16h1,U16h1,low_perm);
unpack_packed_permutation(l8perm1,high_perm,low_perm)
U16l2= simd_permute(U16l2,U16l2,high_perm);
U16h2= simd_permute(U16h2,U16h2,high_perm);
U16l3= simd_permute(U16l3,U16l3,low_perm);
U16h3= simd_permute(U16h3,U16h3,low_perm);
unpack_packed_permutation(l8perm2,high_perm,low_perm)
U16l4= simd_permute(U16l4,U16l4,high_perm);
U16h4= simd_permute(U16h4,U16h4,high_perm);
U16l5= simd_permute(U16l5,U16l5,low_perm);
U16h5= simd_permute(U16h5,U16h5,low_perm);
unpack_packed_permutation(l8perm3,high_perm,low_perm)
U16l6= simd_permute(U16l6,U16l6,high_perm);
U16h6= simd_permute(U16h6,U16h6,high_perm);
U16l7= simd_permute(U16l7,U16l7,low_perm);
U16h7= simd_permute(U16h7,U16h7,low_perm);

}
#endif



/*:83*//*103:*/
#line 2209 "libu8u16.w"

#if (BYTE_DELETION == BYTE_DEL_BY_PERMUTE_TO_RIGHT8)
{
BitBlock permute_index_bit0= simd_andc(simd_const_8(0xAA),delmask);
BitBlock permute_index_bit1= simd_andc(simd_const_8(0xCC),delmask);
BitBlock permute_index_bit2= simd_andc(simd_const_8(0xF0),delmask);
BitBlock permute_high_offset= sisd_sfli(simd_const_8(0x08),64);
BitBlock perm[8];

do_right8_shifts(permute_index_bit0,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(permute_index_bit1,del8_rshift1,del8_rshift2,del8_rshift4)
do_right8_shifts(permute_index_bit2,del8_rshift1,del8_rshift2,del8_rshift4)

p2s_567_bytemerge(permute_index_bit2,permute_index_bit1,permute_index_bit0,
perm[7],perm[6],perm[5],perm[4],perm[3],perm[2],perm[1],perm[0])

perm[0]= simd_or(perm[0],permute_high_offset);
perm[1]= simd_or(perm[1],permute_high_offset);
perm[2]= simd_or(perm[2],permute_high_offset);
perm[3]= simd_or(perm[3],permute_high_offset);
perm[4]= simd_or(perm[4],permute_high_offset);
perm[5]= simd_or(perm[5],permute_high_offset);
perm[6]= simd_or(perm[6],permute_high_offset);
perm[7]= simd_or(perm[7],permute_high_offset);

U16l0= simd_permute(U16l0,perm[0]);
U16h0= simd_permute(U16h0,perm[0]);
U16l1= simd_permute(U16l1,perm[1]);
U16h1= simd_permute(U16h1,perm[1]);
U16l2= simd_permute(U16l2,perm[2]);
U16h2= simd_permute(U16h2,perm[2]);
U16l3= simd_permute(U16l3,perm[3]);
U16h3= simd_permute(U16h3,perm[3]);
U16l4= simd_permute(U16l4,perm[4]);
U16h4= simd_permute(U16h4,perm[4]);
U16l5= simd_permute(U16l5,perm[5]);
U16h5= simd_permute(U16h5,perm[5]);
U16l6= simd_permute(U16l6,perm[6]);
U16h6= simd_permute(U16h6,perm[6]);
U16l7= simd_permute(U16l7,perm[7]);
U16h7= simd_permute(U16h7,perm[7]);

}
#endif





/*:103*/
#line 1315 "libu8u16.w"

/*23:*/
#line 630 "libu8u16.w"

U16s0= u16_merge0(U16h0,U16l0);
U16s1= u16_merge1(U16h0,U16l0);
U16s2= u16_merge0(U16h1,U16l1);
U16s3= u16_merge1(U16h1,U16l1);
U16s4= u16_merge0(U16h2,U16l2);
U16s5= u16_merge1(U16h2,U16l2);
U16s6= u16_merge0(U16h3,U16l3);
U16s7= u16_merge1(U16h3,U16l3);
U16s8= u16_merge0(U16h4,U16l4);
U16s9= u16_merge1(U16h4,U16l4);
U16s10= u16_merge0(U16h5,U16l5);
U16s11= u16_merge1(U16h5,U16l5);
U16s12= u16_merge0(U16h6,U16l6);
U16s13= u16_merge1(U16h6,U16l6);
U16s14= u16_merge0(U16h7,U16l7);
U16s15= u16_merge1(U16h7,U16l7);


/*:23*/
#line 1316 "libu8u16.w"

/*65:*/
#line 1398 "libu8u16.w"

#ifdef OUTBUF_WRITE_NONALIGNED
u16advance= 0;
unaligned_output_step(U16s0,u16_bytes_per_reg[0])
unaligned_output_step(U16s1,u16_bytes_per_reg[1])
unaligned_output_step(U16s2,u16_bytes_per_reg[2])
unaligned_output_step(U16s3,u16_bytes_per_reg[3])
unaligned_output_step(U16s4,u16_bytes_per_reg[4])
unaligned_output_step(U16s5,u16_bytes_per_reg[5])
unaligned_output_step(U16s6,u16_bytes_per_reg[6])
unaligned_output_step(U16s7,u16_bytes_per_reg[7])
unaligned_output_step(U16s8,u16_bytes_per_reg[8])
unaligned_output_step(U16s9,u16_bytes_per_reg[9])
unaligned_output_step(U16s10,u16_bytes_per_reg[10])
unaligned_output_step(U16s11,u16_bytes_per_reg[11])
unaligned_output_step(U16s12,u16_bytes_per_reg[12])
unaligned_output_step(U16s13,u16_bytes_per_reg[13])
unaligned_output_step(U16s14,u16_bytes_per_reg[14])
unaligned_output_step(U16s15,u16_bytes_per_reg[15])
#endif



/*:65*//*85:*/
#line 1977 "libu8u16.w"

#if ((U8U16_TARGET == ALTIVEC_TARGET) || (U8U16_TARGET == SPU_TARGET))
{
u16advance= 0;
BitBlock vec_0__15= vec_lvsl1(0);
unsigned char*dbyte_count= (unsigned char*)&u16_bytes_8;
output_step(U16s0,0)
output_step(U16s1,1)
output_step(U16s2,2)
output_step(U16s3,3)
output_step(U16s4,4)
output_step(U16s5,5)
output_step(U16s6,6)
output_step(U16s7,7)
output_step(U16s8,8)
output_step(U16s9,9)
output_step(U16s10,10)
output_step(U16s11,11)
output_step(U16s12,12)
output_step(U16s13,13)
output_step(U16s14,14)
output_step(U16s15,15)
vec_st(simd_permute(pending,simd_const_8(0),
vec_lvsl1(16-(0x0F&((int)&U16out[u16advance])))),
u16advance-1,U16out);
}
#endif

/*:85*/
#line 1317 "libu8u16.w"



/*:57*/
#line 679 "libu8u16.w"

if(bitblock_has_bit(error_mask))/*67:*/
#line 1428 "libu8u16.w"

{
BitBlock cutoff_mask,errbit,u8scopex2;
int errpos,u8u16errno;
/*44:*/
#line 1063 "libu8u16.w"

u8scope32= bitblock_sfli(u8prefix3,1);

/*:44*/
#line 1432 "libu8u16.w"

/*45:*/
#line 1068 "libu8u16.w"

u8scope42= bitblock_sfli(u8prefix4,1);
u8scope43= bitblock_sfli(u8prefix4,2);
u8surrogate= simd_or(u8scope42,u8scope44);



/*:45*/
#line 1433 "libu8u16.w"

u8scopex2= simd_or(u8scope22,simd_or(u8scope32,u8scope42));
if(!bitblock_has_bit(simd_and(error_mask,input_select_mask))){

u8u16errno= EINVAL;
}
else{
u8u16errno= EILSEQ;
}
errpos= count_forward_zeroes(error_mask);
u8advance= errpos-count_forward_zeroes(input_select_mask);
cutoff_mask= sisd_sfl(simd_const_8(-1),sisd_from_int(errpos));
errbit= simd_andc(error_mask,sisd_sfli(cutoff_mask,1));
input_select_mask= simd_andc(input_select_mask,cutoff_mask);
u16advance= 2*(bitblock_bit_count(simd_and(u8lastbyte,input_select_mask))+
bitblock_bit_count(simd_and(u8scope42,input_select_mask)));
if(bitblock_has_bit(simd_and(u8scope44,errbit))){
u8advance-= 3;
u16advance-= 2;
}
else if(bitblock_has_bit(simd_and(u8scope43,errbit))){
u8advance-= 2;
u16advance-= 2;
}
else if(bitblock_has_bit(simd_and(u8scope33,errbit))){
u8advance-= 2;
}
else if(bitblock_has_bit(simd_and(u8scopex2,errbit))){
u8advance-= 1;
}

/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 1464 "libu8u16.w"


*outbytesleft-= (intptr_t)U16out-(intptr_t)*outbuf;
*inbytesleft= inbytes;
*inbuf= (char*)U8data;
*outbuf= (char*)U16out;
/*97:*/
#line 2123 "libu8u16.w"

#if (U8U16_TARGET == MMX_TARGET)
_mm_empty();
#endif

/*:97*/
#line 1470 "libu8u16.w"
;
errno= u8u16errno;
return(size_t)-1;
}

/*:67*/
#line 680 "libu8u16.w"

/*26:*/
#line 704 "libu8u16.w"

inbytes-= u8advance;
U8data+= u8advance;
U16out+= u16advance;

/*:26*/
#line 681 "libu8u16.w"

}
/*29:*/
#line 763 "libu8u16.w"

*outbytesleft-= (intptr_t)U16out-(intptr_t)*outbuf;
*inbuf= (char*)U8data;
*inbytesleft= inbytes;
*outbuf= (char*)U16out;
/*97:*/
#line 2123 "libu8u16.w"

#if (U8U16_TARGET == MMX_TARGET)
_mm_empty();
#endif

/*:97*/
#line 768 "libu8u16.w"
;
if(inbytes==0)return(size_t)0;
else return(size_t)-1;


/*:29*/
#line 683 "libu8u16.w"

}

/*:24*/
#line 267 "libu8u16.w"

else return buffered_u8u16(inbuf,inbytesleft,outbuf,outbytesleft);
}
else if(inbuf==NULL||*inbuf==NULL||*inbytesleft==0)
return(size_t)0;
else{errno= E2BIG;return(size_t)-1;}
}

/*:5*//*69:*/
#line 1483 "libu8u16.w"

size_t
buffered_u8u16(char**inbuf,size_t*inbytesleft,char**outbuf,size_t*outbytesleft){
if(inbuf&&*inbuf&&outbuf&&*outbuf){
unsigned char*inbuf_start= (unsigned char*)*inbuf;
size_t max_inbytes= min(3*(*outbytesleft)/2,*inbytesleft);
size_t internal_space= 2*(*inbytesleft)+PACKSIZE;
size_t internal_space_left= internal_space;
char*internal_buf_start= (char*)malloc(internal_space);
char*internal_buf= internal_buf_start;
size_t return_code= u8u16(inbuf,&max_inbytes,&internal_buf,&internal_space_left);
intptr_t u16advance= internal_space-internal_space_left;
intptr_t u8advance= (intptr_t)(*inbuf)-(intptr_t)inbuf_start;
if(size_t(u16advance)> *outbytesleft){
errno= E2BIG;
return_code= (size_t)-1;
do{
do{
u8advance--;
}
while(is_suffix_byte(inbuf_start[u8advance]));
if(is_prefix4_byte(inbuf_start[u8advance]))u16advance-= 4;
else u16advance-= 2;
}while(size_t(u16advance)> *outbytesleft);
}
memcpy(*outbuf,internal_buf_start,u16advance);
free(internal_buf_start);
*inbuf= (char*)inbuf_start+u8advance;
*inbytesleft-= u8advance;
*outbuf+= u16advance;
*outbytesleft-= u16advance;
return return_code;
}
else if(inbuf==NULL||*inbuf==NULL||*inbytesleft==0)
return(size_t)0;
else{errno= E2BIG;return(size_t)-1;}
}



/*:69*/
