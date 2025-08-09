inline SIMD_type simd_slli_2(SIMD_type r, int sh){
	 return simd_and(sisd_slli(r,sh),simd_const_2((3<<sh)&3));}
inline SIMD_type simd_slli_4(SIMD_type r, int sh){
	 return simd_and(sisd_slli(r,sh),simd_const_4((15<<sh)&15));}
inline SIMD_type simd_slli_8(SIMD_type r, int sh){
	 return simd_and(sisd_slli(r,sh),simd_const_8((255<<sh)&255));}
inline SIMD_type simd_srli_2(SIMD_type r, int sh){
	 return simd_and(sisd_srli(r,sh),simd_const_2(3>>sh));}
inline SIMD_type simd_srli_4(SIMD_type r, int sh){
	 return simd_and(sisd_srli(r,sh),simd_const_4(15>>sh));}
inline SIMD_type simd_srli_8(SIMD_type r, int sh){
	 return simd_and(sisd_srli(r,sh),simd_const_8(255>>sh));}

static inline
SIMD_type simd_sub_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_xor(a,b);
	 SIMD_type borrow = simd_andc(b,a);
	 SIMD_type c2 = simd_xor(c1,(sisd_slli(borrow,1)));
	 return simd_if(simd_himask_2,c2,c1);
}
static inline
SIMD_type simd_add_2(SIMD_type a, SIMD_type b)
{
	 SIMD_type c1 = simd_xor(a,b);
	 SIMD_type borrow = simd_and(a,b);
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

inline SIMD_type simd_srai_8(SIMD_type a,int shft){
	return simd_if(simd_himask_16, simd_srai_16(a,shft),simd_srai_16(simd_srai_16(sisd_slli(a,8),8),shft));}

#ifndef simd_sub_4
inline SIMD_type simd_sub_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_sub_8(simd_and(a,simd_himask_8),simd_and(b,simd_himask_8))
	,simd_sub_8(simd_andc(a,simd_himask_8),simd_andc(b,simd_himask_8)));}
#endif

#ifndef simd_add_4
inline SIMD_type simd_add_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_add_8(simd_and(a,simd_himask_8),simd_and(b,simd_himask_8))
	,simd_add_8(simd_andc(a,simd_himask_8),simd_andc(b,simd_himask_8)));}
#endif

#ifndef simd_sll_32
inline SIMD_type simd_sll_32(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_64, simd_sll_64(simd_and(a,simd_himask_64),simd_and(simd_const_32(31),simd_srli_64(b,32)))
	,simd_sll_64(a,simd_and(_mm_cvtsi32_si64(31),b)));}
#endif

#ifndef simd_sll_16
inline SIMD_type simd_sll_16(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_32, simd_sll_32(simd_and(a,simd_himask_32),simd_and(simd_const_16(15),simd_srli_32(b,16)))
	,simd_sll_32(a,simd_and(b,simd_const_32(15))));}
#endif

#ifndef simd_sll_8
inline SIMD_type simd_sll_8(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_16, simd_sll_16(simd_and(a,simd_himask_16),simd_and(simd_const_8(7),simd_srli_16(b,8)))
	,simd_sll_16(a,simd_and(b,simd_const_16(7))));}
#endif

#ifndef simd_sll_4
inline SIMD_type simd_sll_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_sll_8(simd_and(a,simd_himask_8),simd_and(simd_const_4(3),simd_srli_8(b,4)))
	,simd_sll_8(a,simd_and(b,simd_const_8(3))));}
#endif

#ifndef simd_srl_32
inline SIMD_type simd_srl_32(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_64, simd_srl_64(a,simd_and(simd_const_32(31),simd_srli_64(b,32)))
	,simd_srl_64(simd_andc(a,simd_himask_64),simd_and(_mm_cvtsi32_si64(31),b)));}
#endif

#ifndef simd_srl_16
inline SIMD_type simd_srl_16(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_32, simd_srl_32(a,simd_and(simd_const_16(15),simd_srli_32(b,16)))
	,simd_srl_32(simd_andc(a,simd_himask_32),simd_and(b,simd_const_32(15))));}
#endif

#ifndef simd_srl_8
inline SIMD_type simd_srl_8(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_16, simd_srl_16(a,simd_and(simd_const_8(7),simd_srli_16(b,8)))
	,simd_srl_16(simd_andc(a,simd_himask_16),simd_and(b,simd_const_16(7))));}
#endif

#ifndef simd_srl_4
inline SIMD_type simd_srl_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_srl_8(a,simd_and(simd_const_4(3),simd_srli_8(b,4)))
	,simd_srl_8(simd_andc(a,simd_himask_8),simd_and(b,simd_const_8(3))));}
#endif

#ifndef simd_sra_32
inline SIMD_type simd_sra_32(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_64,_mm_sra_pi32(a, simd_and(simd_const_32(31),simd_srli_64(b,32))),_mm_sra_pi32(a, simd_and(_mm_cvtsi32_si64(31), b)));}
#endif

#ifndef simd_sra_16
inline SIMD_type simd_sra_16(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_32, simd_sra_32(simd_and(a,simd_himask_32),simd_and(sisd_srli(b,16),simd_const_32(15)))
	,simd_sra_32(simd_srai_32(sisd_slli(a,16),16),simd_and(b,simd_const_32(15))));}
#endif

#ifndef simd_sra_8
inline SIMD_type simd_sra_8(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_16, simd_sra_16(simd_and(a,simd_himask_16),simd_and(sisd_srli(b,8),simd_const_16(7)))
	,simd_sra_16(simd_srai_16(sisd_slli(a,8),8),simd_and(b,simd_const_16(7))));}
#endif

#ifndef simd_sra_4
inline SIMD_type simd_sra_4(SIMD_type a,SIMD_type b){
	return simd_if(simd_himask_8, simd_sra_8(simd_and(a,simd_himask_8),simd_and(sisd_srli(b,4),simd_const_8(3)))
	,simd_sra_8(simd_srai_8(sisd_slli(a,4),4),simd_and(b,simd_const_8(3))));}
#endif

#ifndef simd_rotl_32
inline SIMD_type simd_rotl_32(SIMD_type a,SIMD_type b){
	return simd_or(simd_sll_32(a,b),simd_srl_32(a,simd_sub_32(simd_const_32(32),b)));}
#endif

#ifndef simd_rotl_16
inline SIMD_type simd_rotl_16(SIMD_type a,SIMD_type b){
	return simd_or(simd_sll_16(a,b),simd_srl_16(a,simd_sub_16(simd_const_16(16),b)));}
#endif

#ifndef simd_rotl_8
inline SIMD_type simd_rotl_8(SIMD_type a,SIMD_type b){
	return simd_or(simd_sll_8(a,b),simd_srl_8(a,simd_sub_8(simd_const_8(8),b)));}
#endif

#ifndef simd_rotl_4
inline SIMD_type simd_rotl_4(SIMD_type a,SIMD_type b){
	return simd_or(simd_sll_4(a,b),simd_srl_4(a,simd_sub_4(simd_const_4(4),b)));}
#endif

#ifndef simd_pack_8
inline SIMD_type simd_pack_8(SIMD_type a,SIMD_type b){
	return simd_pack_16(simd_if(simd_himask_8,sisd_srli(a,4),a),
	simd_if(simd_himask_8,sisd_srli(b,4),b));}
#endif

#ifndef simd_pack_4
inline SIMD_type simd_pack_4(SIMD_type a,SIMD_type b){
	return simd_pack_8(simd_if(simd_himask_4,sisd_srli(a,2),a),
	simd_if(simd_himask_4,sisd_srli(b,2),b));}
#endif

#ifndef simd_pack_2
inline SIMD_type simd_pack_2(SIMD_type a,SIMD_type b){
	return simd_pack_4(simd_if(simd_himask_2,sisd_srli(a,1),a),
	simd_if(simd_himask_2,sisd_srli(b,1),b));}
#endif

#ifndef simd_mergeh_4
inline SIMD_type simd_mergeh_4(SIMD_type a,SIMD_type b){
	return simd_mergeh_8(simd_if(simd_himask_8,a,sisd_srli(b,4)),
	simd_if(simd_himask_8,sisd_slli(a,4),b));}
#endif

#ifndef simd_mergeh_2
inline SIMD_type simd_mergeh_2(SIMD_type a,SIMD_type b){
	return simd_mergeh_4(simd_if(simd_himask_4,a,sisd_srli(b,2)),
	simd_if(simd_himask_4,sisd_slli(a,2),b));}
#endif

#ifndef simd_mergeh_1
inline SIMD_type simd_mergeh_1(SIMD_type a,SIMD_type b){
	return simd_mergeh_2(simd_if(simd_himask_2,a,sisd_srli(b,1)),
	simd_if(simd_himask_2,sisd_slli(a,1),b));}
#endif

#ifndef simd_mergel_4
inline SIMD_type simd_mergel_4(SIMD_type a,SIMD_type b){
	return simd_mergel_8(simd_if(simd_himask_8,a,sisd_srli(b,4)),
	simd_if(simd_himask_8,sisd_slli(a,4),b));}
#endif

#ifndef simd_mergel_2
inline SIMD_type simd_mergel_2(SIMD_type a,SIMD_type b){
	return simd_mergel_4(simd_if(simd_himask_4,a,sisd_srli(b,2)),
	simd_if(simd_himask_4,sisd_slli(a,2),b));}
#endif

#ifndef simd_mergel_1
inline SIMD_type simd_mergel_1(SIMD_type a,SIMD_type b){
	return simd_mergel_2(simd_if(simd_himask_2,a,sisd_srli(b,1)),
	simd_if(simd_himask_2,sisd_slli(a,1),b));}
#endif
