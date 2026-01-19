/* camellia-simd128.h - Camellia cipher SIMD128 intrinsics implementation
 * Copyright (C) 2023 Jussi Kivilinna <jussi.kivilinna@iki.fi>
 *
 * This file is part of Libgcrypt.
 *
 * Libgcrypt is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Libgcrypt is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * SSE/AVX/NEON implementation of Camellia cipher, using AES-NI/ARMv8-CE/
 * PPC-crypto for sbox calculations. This implementation takes 16 input blocks
 * and process them in parallel. Vectorized key setup is also available at
 * the end of file. This implementation is from
 *  - https://github.com/jkivilin/camellia-simd-aesni
 *
 * This work was originally presented in Master's Thesis,
 *   "Block Ciphers: Fast Implementations on x86-64 Architecture" (pages 42-50)
 *   http://urn.fi/URN:NBN:fi:oulu-201305311409
 */

#include <config.h>
#include "types.h"


#define ALWAYS_INLINE inline __attribute__((always_inline))
#define NO_INLINE __attribute__((noinline))
#define NO_INSTRUMENT_FUNCTION __attribute__((no_instrument_function))

#define ASM_FUNC_ATTR          NO_INSTRUMENT_FUNCTION
#define ASM_FUNC_ATTR_INLINE   ASM_FUNC_ATTR ALWAYS_INLINE
#define ASM_FUNC_ATTR_NOINLINE ASM_FUNC_ATTR NO_INLINE SIMD128_OPT_ATTR


#if defined(HAVE_GCC_INLINE_ASM_PPC_ALTIVEC) && !defined(WORDS_BIGENDIAN)

/**********************************************************************
  AT&T x86 asm to intrinsics conversion macros (PowerPC VSX+crypto)
 **********************************************************************/
#include <altivec.h>

typedef vector signed char int8x16_t;
typedef vector unsigned char uint8x16_t;
typedef vector unsigned short uint16x8_t;
typedef vector unsigned int uint32x4_t;
typedef vector unsigned long long uint64x2_t;
typedef uint64x2_t __m128i;

#ifdef __clang__
/* clang has mismatching prototype for vec_sbox_be. */
static ASM_FUNC_ATTR_INLINE uint8x16_t
asm_sbox_be(uint8x16_t b)
{
  uint8x16_t o;
  __asm__ ("vsbox %0, %1\n\t" : "=v" (o) : "v" (b));
  return o;
}
#undef vec_sbox_be
#define vec_sbox_be asm_sbox_be
#endif

#define vec_bswap(a)            ((__m128i)vec_reve((uint8x16_t)a))

#define vpand128(a, b, o)       (o = vec_and(b, a))
#define vpandn128(a, b, o)      (o = vec_andc(a, b))
#define vpxor128(a, b, o)       (o = vec_xor(b, a))
#define vpor128(a, b, o)        (o = vec_or(b, a))

#define vpsrlb128(s, a, o)      ({ o = (__m128i)((uint8x16_t)a >> s); })
#define vpsllb128(s, a, o)      ({ o = (__m128i)((uint8x16_t)a << s); })
#define vpsrlw128(s, a, o)      ({ o = (__m128i)((uint16x8_t)a >> s); })
#define vpsllw128(s, a, o)      ({ o = (__m128i)((uint16x8_t)a << s); })
#define vpsrld128(s, a, o)      ({ o = (__m128i)((uint32x4_t)a >> s); })
#define vpslld128(s, a, o)      ({ o = (__m128i)((uint32x4_t)a << s); })
#define vpsrlq128(s, a, o)      ({ o = (__m128i)((uint64x2_t)a >> s); })
#define vpsllq128(s, a, o)      ({ o = (__m128i)((uint64x2_t)a << s); })
#define vpsrldq128(s, a, o)     ({ uint64x2_t __tmp = { 0, 0 }; \
				  o = (__m128i)vec_sld((uint8x16_t)__tmp, \
						       (uint8x16_t)a, (16 - (s)) & 15);})
#define vpslldq128(s, a, o)     ({ uint64x2_t __tmp = { 0, 0 }; \
				  o = (__m128i)vec_sld((uint8x16_t)a, \
						       (uint8x16_t)__tmp, (s) & 15);})

#define if_vpsrlb128(...)       __VA_ARGS__
#define if_not_vpsrlb128(...)   /*_*/
#define vpsrl_byte_128(s, a, o) vpsrlb128(s, a, o)
#define vpsll_byte_128(s, a, o) vpsllb128(s, a, o)

#define vpaddb128(a, b, o)      (o = (__m128i)vec_add((uint8x16_t)b, (uint8x16_t)a))

#define vpcmpgtb128(a, b, o)    (o = (__m128i)vec_cmpgt((int8x16_t)b, (int8x16_t)a))
#define vpabsb128(a, o)         (o = (__m128i)vec_abs((int8x16_t)a))

#define vpshufd128_0x4e(a, o)   (o = (__m128i)vec_reve((uint64x2_t)a))
#define vpshufd128_0x1b(a, o)   (o = (__m128i)vec_reve((uint32x4_t)a))

#define vpshufb128(m, a, o) \
	({ uint64x2_t __tmpz = { 0, 0 }; \
	   o = (__m128i)vec_perm((uint8x16_t)a, (uint8x16_t)__tmpz, (uint8x16_t)m); })

#define vpunpckhdq128(a, b, o)  (o = (__m128i)vec_mergel((uint32x4_t)b, (uint32x4_t)a))
#define vpunpckldq128(a, b, o)  (o = (__m128i)vec_mergeh((uint32x4_t)b, (uint32x4_t)a))
#define vpunpckhqdq128(a, b, o) (o = (__m128i)vec_mergel((uint64x2_t)b, (uint64x2_t)a))
#define vpunpcklqdq128(a, b, o) (o = (__m128i)vec_mergeh((uint64x2_t)b, (uint64x2_t)a))

#define vmovdqa128(a, o)        (o = a)
#define vmovd128(a, o)          ({ uint32x4_t __tmp = { (a), 0, 0, 0 }; \
				   o = (__m128i)(__tmp); })
#define vmovq128(a, o)          ({ uint64x2_t __tmp = { (a), 0 }; \
				   o = (__m128i)(__tmp); })

#define vmovdqa128_memld(a, o)  (o = *(const __m128i *)(a))
#define vmovdqa128_memst(a, o)  (*(__m128i *)(o) = (a))
#define vpshufb128_amemld(m, a, o) vpshufb128(*(const __m128i *)(m), a, o)

/* Following operations may have unaligned memory input */
#define vmovdqu128_memld(a, o)  (o = (__m128i)vec_xl(0, (const uint8_t *)(a)))
#define vpxor128_memld(a, b, o) vpxor128(b, (__m128i)vec_xl(0, (const uint8_t *)(a)), o)

/* Following operations may have unaligned memory output */
#define vmovdqu128_memst(a, o)  vec_xst((uint8x16_t)(a), 0, (uint8_t *)(o))
#define vmovq128_memst(a, o)    (((uint64_unaligned_t *)(o))[0] = ((__m128i)(a))[0])

/* PowerPC AES encrypt last round => ShiftRows + SubBytes + XOR round key  */
static const uint8x16_t shift_row =
  { 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12, 1, 6, 11 };
#define vaesenclast128(a, b, o) \
	({ uint64x2_t __tmp = (__m128i)vec_sbox_be((uint8x16_t)(b)); \
	   vpshufb128(shift_row, __tmp, __tmp); \
	   vpxor128(a, __tmp, o); })

/* Macros for exposing SubBytes from PowerPC crypto instructions. */
#define aes_subbytes(a, o) \
	(o = (__m128i)vec_sbox_be((uint8x16_t)(a)))
#define aes_subbytes_and_shuf_and_xor(zero, a, o) \
        vaesenclast128((zero), (a), (o))
/*#define aes_load_inv_shufmask(shufmask_reg) \
	load_frequent_const(inv_shift_row, (shufmask_reg))*/
#define aes_inv_shuf(shufmask_reg, a, o) \
	vpshufb128(shufmask_reg, (a), (o))
#define if_aes_subbytes(...) __VA_ARGS__
#define if_not_aes_subbytes(...) /*_*/

#define memory_barrier_with_vec(a) __asm__("" : "+wa"(a) :: "memory")

#endif /* __powerpc__ */

#ifdef __ARM_NEON

/**********************************************************************
  AT&T x86 asm to intrinsics conversion macros (ARMv8-CE)
 **********************************************************************/
#include <arm_neon.h>

#define __m128i uint64x2_t

#define vpand128(a, b, o)       (o = vandq_u64(b, a))
#define vpandn128(a, b, o)      (o = vbicq_u64(a, b))
#define vpxor128(a, b, o)       (o = veorq_u64(b, a))
#define vpor128(a, b, o)        (o = vorrq_u64(b, a))

#define vpsrlb128(s, a, o)      (o = (__m128i)vshrq_n_u8((uint8x16_t)a, s))
#define vpsllb128(s, a, o)      (o = (__m128i)vshlq_n_u8((uint8x16_t)a, s))
#define vpsrlw128(s, a, o)      (o = (__m128i)vshrq_n_u16((uint16x8_t)a, s))
#define vpsllw128(s, a, o)      (o = (__m128i)vshlq_n_u16((uint16x8_t)a, s))
#define vpsrld128(s, a, o)      (o = (__m128i)vshrq_n_u32((uint32x4_t)a, s))
#define vpslld128(s, a, o)      (o = (__m128i)vshlq_n_u32((uint32x4_t)a, s))
#define vpsrlq128(s, a, o)      (o = (__m128i)vshrq_n_u64(a, s))
#define vpsllq128(s, a, o)      (o = (__m128i)vshlq_n_u64(a, s))
#define vpsrldq128(s, a, o)     ({ uint64x2_t __tmp = { 0, 0 }; \
				o = (__m128i)vextq_u8((uint8x16_t)a, \
						      (uint8x16_t)__tmp, (s) & 15);})
#define vpslldq128(s, a, o)     ({ uint64x2_t __tmp = { 0, 0 }; \
				o = (__m128i)vextq_u8((uint8x16_t)__tmp, \
						      (uint8x16_t)a, (16 - (s)) & 15);})

#define if_vpsrlb128(...)       __VA_ARGS__
#define if_not_vpsrlb128(...)   /*_*/
#define vpsrl_byte_128(s, a, o) vpsrlb128(s, a, o)
#define vpsll_byte_128(s, a, o) vpsllb128(s, a, o)

#define vpaddb128(a, b, o)      (o = (__m128i)vaddq_u8((uint8x16_t)b, (uint8x16_t)a))

#define vpcmpgtb128(a, b, o)    (o = (__m128i)vcgtq_s8((int8x16_t)b, (int8x16_t)a))
#define vpabsb128(a, o)         (o = (__m128i)vabsq_s8((int8x16_t)a))

#define vpshufd128_0x4e(a, o)   (o = (__m128i)vextq_u8((uint8x16_t)a, (uint8x16_t)a, 8))
#define vpshufd128_0x1b(a, o)   (o = (__m128i)vrev64q_u32((uint32x4_t)vextq_u8((uint8x16_t)a, (uint8x16_t)a, 8)))
#define vpshufb128(m, a, o)     (o = (__m128i)vqtbl1q_u8((uint8x16_t)a, (uint8x16_t)m))

#define vpunpckhdq128(a, b, o)  (o = (__m128i)vzip2q_u32((uint32x4_t)b, (uint32x4_t)a))
#define vpunpckldq128(a, b, o)  (o = (__m128i)vzip1q_u32((uint32x4_t)b, (uint32x4_t)a))
#define vpunpckhqdq128(a, b, o) (o = (__m128i)vzip2q_u64(b, a))
#define vpunpcklqdq128(a, b, o) (o = (__m128i)vzip1q_u64(b, a))

/* CE AES encrypt last round => ShiftRows + SubBytes + XOR round key  */
#define vaesenclast128(a, b, o) (o = (__m128i)vaeseq_u8((uint8x16_t)b, (uint8x16_t)a))

#define vmovdqa128(a, o)        (o = a)
#define vmovd128(a, o)          ({ uint32x4_t __tmp = { a, 0, 0, 0 }; o = (__m128i)__tmp; })
#define vmovq128(a, o)          ({ uint64x2_t __tmp = { a, 0 }; o = (__m128i)__tmp; })

#define vmovdqa128_memld(a, o)  (o = (*(const __m128i *)(a)))
#define vmovdqa128_memst(a, o)  (*(__m128i *)(o) = (a))
#define vpshufb128_amemld(m, a, o) vpshufb128(*(const __m128i *)(m), a, o)

/* Following operations may have unaligned memory input */
#define vmovdqu128_memld(a, o)  (o = (__m128i)vld1q_u8((const uint8_t *)(a)))
#define vpxor128_memld(a, b, o) vpxor128(b, (__m128i)vld1q_u8((const uint8_t *)(a)), o)

/* Following operations may have unaligned memory output */
#define vmovdqu128_memst(a, o)  vst1q_u8((uint8_t *)(o), (uint8x16_t)a)
#define vmovq128_memst(a, o)    (((uint64_unaligned_t *)(o))[0] = (a)[0])

/* Macros for exposing SubBytes from Crypto-Extension instruction set. */
#define aes_subbytes_and_shuf_and_xor(zero, a, o) \
        vaesenclast128(zero, a, o)
#define aes_load_inv_shufmask(shufmask_reg) \
	load_frequent_const(inv_shift_row, shufmask_reg)
#define aes_inv_shuf(shufmask_reg, a, o) \
	vpshufb128(shufmask_reg, a, o)
#define if_aes_subbytes(...) /*_*/
#define if_not_aes_subbytes(...) __VA_ARGS__

#define memory_barrier_with_vec(a) __asm__("" : "+w"(a) :: "memory")

#endif /* __ARM_NEON */

#if defined(__x86_64__) || defined(__i386__)

/**********************************************************************
  AT&T x86 asm to intrinsics conversion macros
 **********************************************************************/
#include <x86intrin.h>

#define vpand128(a, b, o)       (o = _mm_and_si128(b, a))
#define vpandn128(a, b, o)      (o = _mm_andnot_si128(b, a))
#define vpxor128(a, b, o)       (o = _mm_xor_si128(b, a))
#define vpor128(a, b, o)        (o = _mm_or_si128(b, a))

#define vpsrlw128(s, a, o)      (o = _mm_srli_epi16(a, s))
#define vpsllw128(s, a, o)      (o = _mm_slli_epi16(a, s))
#define vpsrld128(s, a, o)      (o = _mm_srli_epi32(a, s))
#define vpslld128(s, a, o)      (o = _mm_slli_epi32(a, s))
#define vpsrlq128(s, a, o)      (o = _mm_srli_epi64(a, s))
#define vpsllq128(s, a, o)      (o = _mm_slli_epi64(a, s))
#define vpsrldq128(s, a, o)     (o = _mm_srli_si128(a, s))
#define vpslldq128(s, a, o)     (o = _mm_slli_si128(a, s))

#define if_vpsrlb128(...)       /*_*/
#define if_not_vpsrlb128(...)   __VA_ARGS__
#define vpsrl_byte_128(s, a, o) vpsrld128(s, a, o)
#define vpsll_byte_128(s, a, o) vpslld128(s, a, o)

#define vpaddb128(a, b, o)      (o = _mm_add_epi8(b, a))

#define vpcmpgtb128(a, b, o)    (o = _mm_cmpgt_epi8(b, a))
#define vpabsb128(a, o)         (o = _mm_abs_epi8(a))

#define vpshufd128_0x1b(a, o)   (o = _mm_shuffle_epi32(a, 0x1b))
#define vpshufd128_0x4e(a, o)   (o = _mm_shuffle_epi32(a, 0x4e))
#define vpshufb128(m, a, o)     (o = _mm_shuffle_epi8(a, m))

#define vpunpckhdq128(a, b, o)  (o = _mm_unpackhi_epi32(b, a))
#define vpunpckldq128(a, b, o)  (o = _mm_unpacklo_epi32(b, a))
#define vpunpckhqdq128(a, b, o) (o = _mm_unpackhi_epi64(b, a))
#define vpunpcklqdq128(a, b, o) (o = _mm_unpacklo_epi64(b, a))

/* AES-NI encrypt last round => ShiftRows + SubBytes + XOR round key  */
#define vaesenclast128(a, b, o) (o = _mm_aesenclast_si128(b, a))

#define vmovdqa128(a, o)        (o = a)
#define vmovd128(a, o)          (o = _mm_set_epi32(0, 0, 0, a))
#define vmovq128(a, o)          (o = _mm_set_epi64x(0, a))

#define vmovdqa128_memld(a, o)  (o = (*(const __m128i *)(a)))
#define vmovdqa128_memst(a, o)  (*(__m128i *)(o) = (a))
#define vpshufb128_amemld(m, a, o) vpshufb128(*(const __m128i *)(m), a, o)

/* Following operations may have unaligned memory input */
#define vmovdqu128_memld(a, o)  (o = _mm_loadu_si128((const __m128i *)(a)))
#define vpxor128_memld(a, b, o) \
	vpxor128(b, _mm_loadu_si128((const __m128i *)(a)), o)

/* Following operations may have unaligned memory output */
#define vmovdqu128_memst(a, o)  _mm_storeu_si128((__m128i *)(o), a)
#define vmovq128_memst(a, o)    _mm_storel_epi64((__m128i *)(o), a)

/* Macros for exposing SubBytes from AES-NI instruction set. */
#define aes_subbytes_and_shuf_and_xor(zero, a, o) \
	vaesenclast128(zero, a, o)
#define aes_load_inv_shufmask(shufmask_reg) \
	load_frequent_const(inv_shift_row, shufmask_reg)
#define aes_inv_shuf(shufmask_reg, a, o) \
	vpshufb128(shufmask_reg, a, o)
#define if_aes_subbytes(...) /*_*/
#define if_not_aes_subbytes(...) __VA_ARGS__

#define memory_barrier_with_vec(a) __asm__("" : "+x"(a) :: "memory")

#endif /* defined(__x86_64__) || defined(__i386__) */

/**********************************************************************
  helper macros
 **********************************************************************/
#define filter_8bit(x, lo_t, hi_t, mask4bit, tmp0) \
	vpand128(x, mask4bit, tmp0); \
	if_vpsrlb128(vpsrlb128(4, x, x)); \
	if_not_vpsrlb128(vpandn128(x, mask4bit, x)); \
	if_not_vpsrlb128(vpsrld128(4, x, x)); \
	\
	vpshufb128(tmp0, lo_t, tmp0); \
	vpshufb128(x, hi_t, x); \
	vpxor128(tmp0, x, x);

#define transpose_4x4(x0, x1, x2, x3, t1, t2) \
	vpunpckhdq128(x1, x0, t2); \
	vpunpckldq128(x1, x0, x0); \
	\
	vpunpckldq128(x3, x2, t1); \
	vpunpckhdq128(x3, x2, x2); \
	\
	vpunpckhqdq128(t1, x0, x1); \
	vpunpcklqdq128(t1, x0, x0); \
	\
	vpunpckhqdq128(x2, t2, x3); \
	vpunpcklqdq128(x2, t2, x2);

#define load_zero(o) vmovq128(0, o)

#define load_frequent_const(constant, o) vmovdqa128(constant ## _stack, o)

#define prepare_frequent_const(constant) \
	vmovdqa128_memld(&(constant), constant ## _stack); \
	memory_barrier_with_vec(constant ## _stack)

#define prepare_frequent_constants() \
	prepare_frequent_const(inv_shift_row); \
	prepare_frequent_const(pack_bswap); \
	prepare_frequent_const(shufb_16x16b); \
	prepare_frequent_const(mask_0f); \
	prepare_frequent_const(pre_tf_lo_s1); \
	prepare_frequent_const(pre_tf_hi_s1); \
	prepare_frequent_const(pre_tf_lo_s4); \
	prepare_frequent_const(pre_tf_hi_s4); \
	prepare_frequent_const(post_tf_lo_s1); \
	prepare_frequent_const(post_tf_hi_s1); \
	prepare_frequent_const(post_tf_lo_s3); \
	prepare_frequent_const(post_tf_hi_s3); \
	prepare_frequent_const(post_tf_lo_s2); \
	prepare_frequent_const(post_tf_hi_s2)

#define frequent_constants_declare \
	__m128i inv_shift_row_stack; \
	__m128i pack_bswap_stack; \
	__m128i shufb_16x16b_stack; \
	__m128i mask_0f_stack; \
	__m128i pre_tf_lo_s1_stack; \
	__m128i pre_tf_hi_s1_stack; \
	__m128i pre_tf_lo_s4_stack; \
	__m128i pre_tf_hi_s4_stack; \
	__m128i post_tf_lo_s1_stack; \
	__m128i post_tf_hi_s1_stack; \
	__m128i post_tf_lo_s3_stack; \
	__m128i post_tf_hi_s3_stack; \
	__m128i post_tf_lo_s2_stack; \
	__m128i post_tf_hi_s2_stack

/**********************************************************************
  16-way camellia macros
 **********************************************************************/

/*
 * IN:
 *   x0..x7: byte-sliced AB state
 *   mem_cd: register pointer storing CD state
 *   key: index for key material
 * OUT:
 *   x0..x7: new byte-sliced CD state
 */
#define roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, t0, t1, t2, t3, t4, t5, t6, \
		  t7, mem_cd, key) \
	/* \
	 * S-function with AES subbytes \
	 */ \
	if_not_aes_subbytes(aes_load_inv_shufmask(t4);) \
	load_frequent_const(mask_0f, t7); \
	load_frequent_const(pre_tf_lo_s1, t0); \
	load_frequent_const(pre_tf_hi_s1, t1); \
	\
	/* AES inverse shift rows */ \
	if_not_aes_subbytes( \
	  aes_inv_shuf(t4, x0, x0); \
	  aes_inv_shuf(t4, x7, x7); \
	  aes_inv_shuf(t4, x1, x1); \
	  aes_inv_shuf(t4, x4, x4); \
	  aes_inv_shuf(t4, x2, x2); \
	  aes_inv_shuf(t4, x5, x5); \
	  aes_inv_shuf(t4, x3, x3); \
	  aes_inv_shuf(t4, x6, x6); \
	) \
	\
	/* prefilter sboxes 1, 2 and 3 */ \
	load_frequent_const(pre_tf_lo_s4, t2); \
	load_frequent_const(pre_tf_hi_s4, t3); \
	filter_8bit(x0, t0, t1, t7, t6); \
	filter_8bit(x7, t0, t1, t7, t6); \
	filter_8bit(x1, t0, t1, t7, t6); \
	filter_8bit(x4, t0, t1, t7, t6); \
	filter_8bit(x2, t0, t1, t7, t6); \
	filter_8bit(x5, t0, t1, t7, t6); \
	\
	/* prefilter sbox 4 */ \
	if_not_aes_subbytes(load_zero(t4);) \
	filter_8bit(x3, t2, t3, t7, t6); \
	filter_8bit(x6, t2, t3, t7, t6); \
	\
	/* AES subbytes + AES shift rows */ \
	load_frequent_const(post_tf_lo_s1, t0); \
	load_frequent_const(post_tf_hi_s1, t1); \
	if_not_aes_subbytes( \
	  aes_subbytes_and_shuf_and_xor(t4, x0, x0); \
	  aes_subbytes_and_shuf_and_xor(t4, x7, x7); \
	  aes_subbytes_and_shuf_and_xor(t4, x1, x1); \
	  aes_subbytes_and_shuf_and_xor(t4, x4, x4); \
	  aes_subbytes_and_shuf_and_xor(t4, x2, x2); \
	  aes_subbytes_and_shuf_and_xor(t4, x5, x5); \
	  aes_subbytes_and_shuf_and_xor(t4, x3, x3); \
	  aes_subbytes_and_shuf_and_xor(t4, x6, x6); \
	) \
	if_aes_subbytes( \
	  aes_subbytes(x0, x0); \
	  aes_subbytes(x7, x7); \
	  aes_subbytes(x1, x1); \
	  aes_subbytes(x4, x4); \
	  aes_subbytes(x2, x2); \
	  aes_subbytes(x5, x5); \
	  aes_subbytes(x3, x3); \
	  aes_subbytes(x6, x6); \
	) \
	\
	/* postfilter sboxes 1 and 4 */ \
	load_frequent_const(post_tf_lo_s3, t2); \
	load_frequent_const(post_tf_hi_s3, t3); \
	filter_8bit(x0, t0, t1, t7, t6); \
	filter_8bit(x7, t0, t1, t7, t6); \
	filter_8bit(x3, t0, t1, t7, t6); \
	filter_8bit(x6, t0, t1, t7, t6); \
	\
	/* postfilter sbox 3 */ \
	load_frequent_const(post_tf_lo_s2, t4); \
	load_frequent_const(post_tf_hi_s2, t5); \
	filter_8bit(x2, t2, t3, t7, t6); \
	filter_8bit(x5, t2, t3, t7, t6); \
	\
	vmovq128((key), t0); \
	\
	/* postfilter sbox 2 */ \
	filter_8bit(x1, t4, t5, t7, t2); \
	filter_8bit(x4, t4, t5, t7, t2); \
	\
	/* P-function */ \
	vpxor128(x5, x0, x0); \
	vpxor128(x6, x1, x1); \
	vpxor128(x7, x2, x2); \
	vpxor128(x4, x3, x3); \
	\
	vpxor128(x2, x4, x4); \
	vpxor128(x3, x5, x5); \
	vpxor128(x0, x6, x6); \
	vpxor128(x1, x7, x7); \
	\
	vpxor128(x7, x0, x0); \
	vpxor128(x4, x1, x1); \
	vpxor128(x5, x2, x2); \
	vpxor128(x6, x3, x3); \
	\
	vpxor128(x3, x4, x4); \
	vpxor128(x0, x5, x5); \
	vpxor128(x1, x6, x6); \
	vpxor128(x2, x7, x7); /* note: high and low parts swapped */ \
	\
	/* Add key material and result to CD (x becomes new CD) */ \
	\
	vpshufb128(bcast[7], t0, t7); \
	vpshufb128(bcast[6], t0, t6); \
	vpshufb128(bcast[5], t0, t5); \
	vpshufb128(bcast[4], t0, t4); \
	vpshufb128(bcast[3], t0, t3); \
	vpshufb128(bcast[2], t0, t2); \
	vpshufb128(bcast[1], t0, t1); \
	\
	vpxor128(t3, x4, x4); \
	vpxor128(mem_cd[0], x4, x4); \
	\
	load_zero(t3); \
	vpshufb128(t3, t0, t0); \
	\
	vpxor128(t2, x5, x5); \
	vpxor128(mem_cd[1], x5, x5); \
	\
	vpxor128(t1, x6, x6); \
	vpxor128(mem_cd[2], x6, x6); \
	\
	vpxor128(t0, x7, x7); \
	vpxor128(mem_cd[3], x7, x7); \
	\
	vpxor128(t7, x0, x0); \
	vpxor128(mem_cd[4], x0, x0); \
	\
	vpxor128(t6, x1, x1); \
	vpxor128(mem_cd[5], x1, x1); \
	\
	vpxor128(t5, x2, x2); \
	vpxor128(mem_cd[6], x2, x2); \
	\
	vpxor128(t4, x3, x3); \
	vpxor128(mem_cd[7], x3, x3);

/*
 * IN/OUT:
 *  x0..x7: byte-sliced AB state preloaded
 *  mem_ab: byte-sliced AB state in memory
 *  mem_cb: byte-sliced CD state in memory
 */
#define two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i, dir, store_ab) \
	roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		  y6, y7, mem_cd, ctx->key_table[(i)]); \
	\
	vmovdqa128(x4, mem_cd[0]); \
	vmovdqa128(x5, mem_cd[1]); \
	vmovdqa128(x6, mem_cd[2]); \
	vmovdqa128(x7, mem_cd[3]); \
	vmovdqa128(x0, mem_cd[4]); \
	vmovdqa128(x1, mem_cd[5]); \
	vmovdqa128(x2, mem_cd[6]); \
	vmovdqa128(x3, mem_cd[7]); \
	\
	roundsm16(x4, x5, x6, x7, x0, x1, x2, x3, y0, y1, y2, y3, y4, y5, \
		  y6, y7, mem_ab, ctx->key_table[(i) + (dir)]); \
	\
	store_ab(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab);

#define dummy_store(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab) /* do nothing */

#define store_ab_state(x0, x1, x2, x3, x4, x5, x6, x7, mem_ab) \
	/* Store new AB state */ \
	vmovdqa128(x0, mem_ab[0]); \
	vmovdqa128(x1, mem_ab[1]); \
	vmovdqa128(x2, mem_ab[2]); \
	vmovdqa128(x3, mem_ab[3]); \
	vmovdqa128(x4, mem_ab[4]); \
	vmovdqa128(x5, mem_ab[5]); \
	vmovdqa128(x6, mem_ab[6]); \
	vmovdqa128(x7, mem_ab[7]);

#define enc_rounds16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i) \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 2, 1, store_ab_state); \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 4, 1, store_ab_state); \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 6, 1, dummy_store);

#define dec_rounds16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, i) \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 7, -1, store_ab_state); \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 5, -1, store_ab_state); \
	two_roundsm16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd, (i) + 3, -1, dummy_store);

#define LE64_LO32(x) ((x) & 0xffffffffU)
#define LE64_HI32(x) ((x >> 32) & 0xffffffffU)

/*
 * IN:
 *  v0..3: byte-sliced 32-bit integers
 * OUT:
 *  v0..3: (IN <<< 1)
 */
#define rol32_1_16(v0, v1, v2, v3, t0, t1, t2, zero) \
	if_vpsrlb128(vpsrlb128(7, v0, t0)); \
	if_not_vpsrlb128(vpcmpgtb128(v0, zero, t0)); \
	vpaddb128(v0, v0, v0); \
	if_not_vpsrlb128(vpabsb128(t0, t0)); \
	\
	if_vpsrlb128(vpsrlb128(7, v1, t1)); \
	if_not_vpsrlb128(vpcmpgtb128(v1, zero, t1)); \
	vpaddb128(v1, v1, v1); \
	if_not_vpsrlb128(vpabsb128(t1, t1)); \
	\
	if_vpsrlb128(vpsrlb128(7, v2, t2)); \
	if_not_vpsrlb128(vpcmpgtb128(v2, zero, t2)); \
	vpaddb128(v2, v2, v2); \
	if_not_vpsrlb128(vpabsb128(t2, t2)); \
	\
	vpor128(t0, v1, v1); \
	\
	if_vpsrlb128(vpsrlb128(7, v3, t0)); \
	if_not_vpsrlb128(vpcmpgtb128(v3, zero, t0)); \
	vpaddb128(v3, v3, v3); \
	if_not_vpsrlb128(vpabsb128(t0, t0)); \
	\
	vpor128(t1, v2, v2); \
	vpor128(t2, v3, v3); \
	vpor128(t0, v0, v0);

/*
 * IN:
 *   r: byte-sliced AB state in memory
 *   l: byte-sliced CD state in memory
 * OUT:
 *   x0..x7: new byte-sliced CD state
 */
#define fls16(l, l0, l1, l2, l3, l4, l5, l6, l7, r, t0, t1, t2, t3, tt0, \
	      tt1, tt2, tt3, kl, kr) \
	/* \
	 * t0 = kll; \
	 * t0 &= ll; \
	 * lr ^= rol32(t0, 1); \
	 */ \
	load_zero(tt0); \
	vmovd128(LE64_LO32(*(kl)), t0); \
	vpshufb128(tt0, t0, t3); \
	vpshufb128(bcast[1], t0, t2); \
	vpshufb128(bcast[2], t0, t1); \
	vpshufb128(bcast[3], t0, t0); \
	\
	vpand128(l0, t0, t0); \
	vpand128(l1, t1, t1); \
	vpand128(l2, t2, t2); \
	vpand128(l3, t3, t3); \
	\
	rol32_1_16(t3, t2, t1, t0, tt1, tt2, tt3, tt0); \
	\
	vpxor128(l4, t0, l4); \
	vmovdqa128(l4, l[4]); \
	vpxor128(l5, t1, l5); \
	vmovdqa128(l5, l[5]); \
	vpxor128(l6, t2, l6); \
	vmovdqa128(l6, l[6]); \
	vpxor128(l7, t3, l7); \
	vmovdqa128(l7, l[7]); \
	\
	/* \
	 * t2 = krr; \
	 * t2 |= rr; \
	 * rl ^= t2; \
	 */ \
	\
	vmovd128(LE64_HI32(*(kr)), t0); \
	vpshufb128(tt0, t0, t3); \
	vpshufb128(bcast[1], t0, t2); \
	vpshufb128(bcast[2], t0, t1); \
	vpshufb128(bcast[3], t0, t0); \
	\
	vpor128(r[4], t0, t0); \
	vpor128(r[5], t1, t1); \
	vpor128(r[6], t2, t2); \
	vpor128(r[7], t3, t3); \
	\
	vpxor128(r[0], t0, t0); \
	vpxor128(r[1], t1, t1); \
	vpxor128(r[2], t2, t2); \
	vpxor128(r[3], t3, t3); \
	vmovdqa128(t0, r[0]); \
	vmovdqa128(t1, r[1]); \
	vmovdqa128(t2, r[2]); \
	vmovdqa128(t3, r[3]); \
	\
	/* \
	 * t2 = krl; \
	 * t2 &= rl; \
	 * rr ^= rol32(t2, 1); \
	 */ \
	vmovd128(LE64_LO32(*(kr)), t0); \
	vpshufb128(tt0, t0, t3); \
	vpshufb128(bcast[1], t0, t2); \
	vpshufb128(bcast[2], t0, t1); \
	vpshufb128(bcast[3], t0, t0); \
	\
	vpand128(r[0], t0, t0); \
	vpand128(r[1], t1, t1); \
	vpand128(r[2], t2, t2); \
	vpand128(r[3], t3, t3); \
	\
	rol32_1_16(t3, t2, t1, t0, tt1, tt2, tt3, tt0); \
	\
	vpxor128(r[4], t0, t0); \
	vpxor128(r[5], t1, t1); \
	vpxor128(r[6], t2, t2); \
	vpxor128(r[7], t3, t3); \
	vmovdqa128(t0, r[4]); \
	vmovdqa128(t1, r[5]); \
	vmovdqa128(t2, r[6]); \
	vmovdqa128(t3, r[7]); \
	\
	/* \
	 * t0 = klr; \
	 * t0 |= lr; \
	 * ll ^= t0; \
	 */ \
	\
	vmovd128(LE64_HI32(*(kl)), t0); \
	vpshufb128(tt0, t0, t3); \
	vpshufb128(bcast[1], t0, t2); \
	vpshufb128(bcast[2], t0, t1); \
	vpshufb128(bcast[3], t0, t0); \
	\
	vpor128(l4, t0, t0); \
	vpor128(l5, t1, t1); \
	vpor128(l6, t2, t2); \
	vpor128(l7, t3, t3); \
	\
	vpxor128(l0, t0, l0); \
	vmovdqa128(l0, l[0]); \
	vpxor128(l1, t1, l1); \
	vmovdqa128(l1, l[1]); \
	vpxor128(l2, t2, l2); \
	vmovdqa128(l2, l[2]); \
	vpxor128(l3, t3, l3); \
	vmovdqa128(l3, l[3]);

#define byteslice_16x16b_fast(a0, b0, c0, d0, a1, b1, c1, d1, a2, b2, c2, d2, \
			      a3, b3, c3, d3, st0, st1) \
	vmovdqa128(d2, st0); \
	vmovdqa128(d3, st1); \
	transpose_4x4(a0, a1, a2, a3, d2, d3); \
	transpose_4x4(b0, b1, b2, b3, d2, d3); \
	vmovdqa128(st0, d2); \
	vmovdqa128(st1, d3); \
	\
	vmovdqa128(a0, st0); \
	vmovdqa128(a1, st1); \
	transpose_4x4(c0, c1, c2, c3, a0, a1); \
	transpose_4x4(d0, d1, d2, d3, a0, a1); \
	\
	vmovdqa128(shufb_16x16b_stack, a0); \
	vmovdqa128(st1, a1); \
	vpshufb128(a0, a2, a2); \
	vpshufb128(a0, a3, a3); \
	vpshufb128(a0, b0, b0); \
	vpshufb128(a0, b1, b1); \
	vpshufb128(a0, b2, b2); \
	vpshufb128(a0, b3, b3); \
	vpshufb128(a0, a1, a1); \
	vpshufb128(a0, c0, c0); \
	vpshufb128(a0, c1, c1); \
	vpshufb128(a0, c2, c2); \
	vpshufb128(a0, c3, c3); \
	vpshufb128(a0, d0, d0); \
	vpshufb128(a0, d1, d1); \
	vpshufb128(a0, d2, d2); \
	vpshufb128(a0, d3, d3); \
	vmovdqa128(d3, st1); \
	vmovdqa128(st0, d3); \
	vpshufb128(a0, d3, a0); \
	vmovdqa128(d2, st0); \
	\
	transpose_4x4(a0, b0, c0, d0, d2, d3); \
	transpose_4x4(a1, b1, c1, d1, d2, d3); \
	vmovdqa128(st0, d2); \
	vmovdqa128(st1, d3); \
	\
	vmovdqa128(b0, st0); \
	vmovdqa128(b1, st1); \
	transpose_4x4(a2, b2, c2, d2, b0, b1); \
	transpose_4x4(a3, b3, c3, d3, b0, b1); \
	vmovdqa128(st0, b0); \
	vmovdqa128(st1, b1); \
	/* does not adjust output bytes inside vectors */

/* load blocks to registers and apply pre-whitening */
#define inpack16_pre(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		     y6, y7, rio, key) \
	vmovq128((key), x0); \
	vpshufb128(pack_bswap_stack, x0, x0); \
	\
	vpxor128_memld((rio) + 0 * 16, x0, y7); \
	vpxor128_memld((rio) + 1 * 16, x0, y6); \
	vpxor128_memld((rio) + 2 * 16, x0, y5); \
	vpxor128_memld((rio) + 3 * 16, x0, y4); \
	vpxor128_memld((rio) + 4 * 16, x0, y3); \
	vpxor128_memld((rio) + 5 * 16, x0, y2); \
	vpxor128_memld((rio) + 6 * 16, x0, y1); \
	vpxor128_memld((rio) + 7 * 16, x0, y0); \
	vpxor128_memld((rio) + 8 * 16, x0, x7); \
	vpxor128_memld((rio) + 9 * 16, x0, x6); \
	vpxor128_memld((rio) + 10 * 16, x0, x5); \
	vpxor128_memld((rio) + 11 * 16, x0, x4); \
	vpxor128_memld((rio) + 12 * 16, x0, x3); \
	vpxor128_memld((rio) + 13 * 16, x0, x2); \
	vpxor128_memld((rio) + 14 * 16, x0, x1); \
	vpxor128_memld((rio) + 15 * 16, x0, x0);

/* byteslice pre-whitened blocks and store to temporary memory */
#define inpack16_post(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		      y6, y7, mem_ab, mem_cd) \
	byteslice_16x16b_fast(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, \
			      y4, y5, y6, y7, mem_ab[0], mem_cd[0]); \
	\
	vmovdqa128(x0, mem_ab[0]); \
	vmovdqa128(x1, mem_ab[1]); \
	vmovdqa128(x2, mem_ab[2]); \
	vmovdqa128(x3, mem_ab[3]); \
	vmovdqa128(x4, mem_ab[4]); \
	vmovdqa128(x5, mem_ab[5]); \
	vmovdqa128(x6, mem_ab[6]); \
	vmovdqa128(x7, mem_ab[7]); \
	vmovdqa128(y0, mem_cd[0]); \
	vmovdqa128(y1, mem_cd[1]); \
	vmovdqa128(y2, mem_cd[2]); \
	vmovdqa128(y3, mem_cd[3]); \
	vmovdqa128(y4, mem_cd[4]); \
	vmovdqa128(y5, mem_cd[5]); \
	vmovdqa128(y6, mem_cd[6]); \
	vmovdqa128(y7, mem_cd[7]);

/* de-byteslice, apply post-whitening and store blocks */
#define outunpack16(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, \
		    y5, y6, y7, key, stack_tmp0, stack_tmp1) \
	byteslice_16x16b_fast(y0, y4, x0, x4, y1, y5, x1, x5, y2, y6, x2, x6, \
			      y3, y7, x3, x7, stack_tmp0, stack_tmp1); \
	\
	vmovdqa128(x0, stack_tmp0); \
	\
	vmovq128((key), x0); \
	vpshufb128(pack_bswap_stack, x0, x0); \
	\
	vpxor128(x0, y7, y7); \
	vpxor128(x0, y6, y6); \
	vpxor128(x0, y5, y5); \
	vpxor128(x0, y4, y4); \
	vpxor128(x0, y3, y3); \
	vpxor128(x0, y2, y2); \
	vpxor128(x0, y1, y1); \
	vpxor128(x0, y0, y0); \
	vpxor128(x0, x7, x7); \
	vpxor128(x0, x6, x6); \
	vpxor128(x0, x5, x5); \
	vpxor128(x0, x4, x4); \
	vpxor128(x0, x3, x3); \
	vpxor128(x0, x2, x2); \
	vpxor128(x0, x1, x1); \
	vpxor128(stack_tmp0, x0, x0);

#define write_output(x0, x1, x2, x3, x4, x5, x6, x7, y0, y1, y2, y3, y4, y5, \
		     y6, y7, rio) \
	vmovdqu128_memst(x0, (rio) + 0 * 16); \
	vmovdqu128_memst(x1, (rio) + 1 * 16); \
	vmovdqu128_memst(x2, (rio) + 2 * 16); \
	vmovdqu128_memst(x3, (rio) + 3 * 16); \
	vmovdqu128_memst(x4, (rio) + 4 * 16); \
	vmovdqu128_memst(x5, (rio) + 5 * 16); \
	vmovdqu128_memst(x6, (rio) + 6 * 16); \
	vmovdqu128_memst(x7, (rio) + 7 * 16); \
	vmovdqu128_memst(y0, (rio) + 8 * 16); \
	vmovdqu128_memst(y1, (rio) + 9 * 16); \
	vmovdqu128_memst(y2, (rio) + 10 * 16); \
	vmovdqu128_memst(y3, (rio) + 11 * 16); \
	vmovdqu128_memst(y4, (rio) + 12 * 16); \
	vmovdqu128_memst(y5, (rio) + 13 * 16); \
	vmovdqu128_memst(y6, (rio) + 14 * 16); \
	vmovdqu128_memst(y7, (rio) + 15 * 16);

/**********************************************************************
  macros for defining constant vectors
 **********************************************************************/
#define SWAP_LE64(x) (x)

#define M128I_BYTE(a0, a1, a2, a3, a4, a5, a6, a7, b0, b1, b2, b3, b4, b5, b6, b7) \
	{ \
	  SWAP_LE64((((a0) & 0xffULL) << 0) | \
		    (((a1) & 0xffULL) << 8) | \
		    (((a2) & 0xffULL) << 16) | \
		    (((a3) & 0xffULL) << 24) | \
		    (((a4) & 0xffULL) << 32) | \
		    (((a5) & 0xffULL) << 40) | \
		    (((a6) & 0xffULL) << 48) | \
		    (((a7) & 0xffULL) << 56)), \
	  SWAP_LE64((((b0) & 0xffULL) << 0) | \
		    (((b1) & 0xffULL) << 8) | \
		    (((b2) & 0xffULL) << 16) | \
		    (((b3) & 0xffULL) << 24) | \
		    (((b4) & 0xffULL) << 32) | \
		    (((b5) & 0xffULL) << 40) | \
		    (((b6) & 0xffULL) << 48) | \
		    (((b7) & 0xffULL) << 56)) \
	}

#define M128I_U32(a0, a1, b0, b1) \
	{ \
	  SWAP_LE64((((a0) & 0xffffffffULL) << 0) | \
		    (((a1) & 0xffffffffULL) << 32)), \
	  SWAP_LE64((((b0) & 0xffffffffULL) << 0) | \
		    (((b1) & 0xffffffffULL) << 32)) \
	}

#define M128I_REP16(x) { (0x0101010101010101ULL * (x)), (0x0101010101010101ULL * (x)) }

#define SHUFB_BYTES(idx) \
	(((0 + (idx)) << 0)  | ((4 + (idx)) << 8) | \
	 ((8 + (idx)) << 16) | ((12 + (idx)) << 24))

typedef u64 uint64_unaligned_t __attribute__((aligned(1), may_alias));

static const __m128i shufb_16x16b =
  M128I_U32(SHUFB_BYTES(0), SHUFB_BYTES(1), SHUFB_BYTES(2), SHUFB_BYTES(3));

static const __m128i pack_bswap =
  M128I_U32(0x00010203, 0x04050607, 0x0f0f0f0f, 0x0f0f0f0f);

static const __m128i bcast[8] =
{
  M128I_REP16(0), M128I_REP16(1), M128I_REP16(2), M128I_REP16(3),
  M128I_REP16(4), M128I_REP16(5), M128I_REP16(6), M128I_REP16(7)
};

/*
 * pre-SubByte transform
 *
 * pre-lookup for sbox1, sbox2, sbox3:
 *   swap_bitendianness(
 *       isom_map_camellia_to_aes(
 *           camellia_f(
 *               swap_bitendianess(in)
 *           )
 *       )
 *   )
 *
 * (note: '⊕ 0xc5' inside camellia_f())
 */
static const __m128i pre_tf_lo_s1 =
  M128I_BYTE(0x45, 0xe8, 0x40, 0xed, 0x2e, 0x83, 0x2b, 0x86,
	     0x4b, 0xe6, 0x4e, 0xe3, 0x20, 0x8d, 0x25, 0x88);

static const __m128i pre_tf_hi_s1 =
  M128I_BYTE(0x00, 0x51, 0xf1, 0xa0, 0x8a, 0xdb, 0x7b, 0x2a,
	     0x09, 0x58, 0xf8, 0xa9, 0x83, 0xd2, 0x72, 0x23);

/*
 * pre-SubByte transform
 *
 * pre-lookup for sbox4:
 *   swap_bitendianness(
 *       isom_map_camellia_to_aes(
 *           camellia_f(
 *               swap_bitendianess(in <<< 1)
 *           )
 *       )
 *   )
 *
 * (note: '⊕ 0xc5' inside camellia_f())
 */
static const __m128i pre_tf_lo_s4 =
  M128I_BYTE(0x45, 0x40, 0x2e, 0x2b, 0x4b, 0x4e, 0x20, 0x25,
	     0x14, 0x11, 0x7f, 0x7a, 0x1a, 0x1f, 0x71, 0x74);

static const __m128i pre_tf_hi_s4 =
  M128I_BYTE(0x00, 0xf1, 0x8a, 0x7b, 0x09, 0xf8, 0x83, 0x72,
	     0xad, 0x5c, 0x27, 0xd6, 0xa4, 0x55, 0x2e, 0xdf);

/*
 * post-SubByte transform
 *
 * post-lookup for sbox1, sbox4:
 *  swap_bitendianness(
 *      camellia_h(
 *          isom_map_aes_to_camellia(
 *              swap_bitendianness(
 *                  aes_inverse_affine_transform(in)
 *              )
 *          )
 *      )
 *  )
 *
 * (note: '⊕ 0x6e' inside camellia_h())
 */
static const __m128i post_tf_lo_s1 =
  M128I_BYTE(0x3c, 0xcc, 0xcf, 0x3f, 0x32, 0xc2, 0xc1, 0x31,
	     0xdc, 0x2c, 0x2f, 0xdf, 0xd2, 0x22, 0x21, 0xd1);

static const __m128i post_tf_hi_s1 =
  M128I_BYTE(0x00, 0xf9, 0x86, 0x7f, 0xd7, 0x2e, 0x51, 0xa8,
	     0xa4, 0x5d, 0x22, 0xdb, 0x73, 0x8a, 0xf5, 0x0c);

/*
 * post-SubByte transform
 *
 * post-lookup for sbox2:
 *  swap_bitendianness(
 *      camellia_h(
 *          isom_map_aes_to_camellia(
 *              swap_bitendianness(
 *                  aes_inverse_affine_transform(in)
 *              )
 *          )
 *      )
 *  ) <<< 1
 *
 * (note: '⊕ 0x6e' inside camellia_h())
 */
static const __m128i post_tf_lo_s2 =
  M128I_BYTE(0x78, 0x99, 0x9f, 0x7e, 0x64, 0x85, 0x83, 0x62,
	     0xb9, 0x58, 0x5e, 0xbf, 0xa5, 0x44, 0x42, 0xa3);

static const __m128i post_tf_hi_s2 =
  M128I_BYTE(0x00, 0xf3, 0x0d, 0xfe, 0xaf, 0x5c, 0xa2, 0x51,
	     0x49, 0xba, 0x44, 0xb7, 0xe6, 0x15, 0xeb, 0x18);

/*
 * post-SubByte transform
 *
 * post-lookup for sbox3:
 *  swap_bitendianness(
 *      camellia_h(
 *          isom_map_aes_to_camellia(
 *              swap_bitendianness(
 *                  aes_inverse_affine_transform(in)
 *              )
 *          )
 *      )
 *  ) >>> 1
 *
 * (note: '⊕ 0x6e' inside camellia_h())
 */
static const __m128i post_tf_lo_s3 =
  M128I_BYTE(0x1e, 0x66, 0xe7, 0x9f, 0x19, 0x61, 0xe0, 0x98,
	     0x6e, 0x16, 0x97, 0xef, 0x69, 0x11, 0x90, 0xe8);

static const __m128i post_tf_hi_s3 =
  M128I_BYTE(0x00, 0xfc, 0x43, 0xbf, 0xeb, 0x17, 0xa8, 0x54,
	     0x52, 0xae, 0x11, 0xed, 0xb9, 0x45, 0xfa, 0x06);

/* For isolating SubBytes from AESENCLAST, inverse shift row */
static const __m128i inv_shift_row =
  M128I_BYTE(0x00, 0x0d, 0x0a, 0x07, 0x04, 0x01, 0x0e, 0x0b,
	     0x08, 0x05, 0x02, 0x0f, 0x0c, 0x09, 0x06, 0x03);

/* 4-bit mask */
static const __m128i mask_0f =
  M128I_U32(0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f, 0x0f0f0f0f);

/* Encrypts 16 input block from IN and writes result to OUT. IN and OUT may
 * unaligned pointers. */
void ASM_FUNC_ATTR_NOINLINE
FUNC_ENC_BLK16(const void *key_table, void *vout, const void *vin,
	       int key_length)
{
  const struct enc_ctx_s
  {
    const u64 *key_table;
    int key_length;
  } sctx =
    {
      .key_table = (const u64 *)key_table,
      .key_length = key_length
    };
  const struct enc_ctx_s *ctx = &sctx;
  char *out = vout;
  const char *in = vin;
  __m128i x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
  __m128i ab[8];
  __m128i cd[8];
  __m128i tmp0, tmp1;
  unsigned int lastk, k;
  frequent_constants_declare;

  prepare_frequent_constants();

  if (ctx->key_length > 16)
    lastk = 32;
  else
    lastk = 24;

  inpack16_pre(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
	       x15, in, ctx->key_table[0]);

  inpack16_post(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
		x15, ab, cd);

  k = 0;
  while (1)
    {
      enc_rounds16(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
		  x15, ab, cd, k);

      if (k == lastk - 8)
	break;

      fls16(ab, x0, x1, x2, x3, x4, x5, x6, x7, cd, x8, x9, x10, x11, x12, x13, x14,
	    x15, &ctx->key_table[k + 8], &ctx->key_table[k + 9]);

      k += 8;
    }

  /* load CD for output */
  vmovdqa128(cd[0], x8);
  vmovdqa128(cd[1], x9);
  vmovdqa128(cd[2], x10);
  vmovdqa128(cd[3], x11);
  vmovdqa128(cd[4], x12);
  vmovdqa128(cd[5], x13);
  vmovdqa128(cd[6], x14);
  vmovdqa128(cd[7], x15);

  outunpack16(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
	      x15, ctx->key_table[lastk], tmp0, tmp1);

  write_output(x7, x6, x5, x4, x3, x2, x1, x0, x15, x14, x13, x12, x11, x10, x9,
	       x8, out);
}

/* Decrypts 16 input block from IN and writes result to OUT. IN and OUT may
 * unaligned pointers. */
void ASM_FUNC_ATTR_NOINLINE
FUNC_DEC_BLK16(const void *key_table, void *vout, const void *vin,
	       int key_length)
{
  const struct dec_ctx_s
  {
    const u64 *key_table;
    int key_length;
  } sctx =
    {
      .key_table = (const u64 *)key_table,
      .key_length = key_length
    };
  const struct dec_ctx_s *ctx = &sctx;
  char *out = vout;
  const char *in = vin;
  __m128i x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
  __m128i ab[8];
  __m128i cd[8];
  __m128i tmp0, tmp1;
  unsigned int firstk, k;
  frequent_constants_declare;

  prepare_frequent_constants();

  if (ctx->key_length > 16)
    firstk = 32;
  else
    firstk = 24;

  inpack16_pre(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
	       x15, in, ctx->key_table[firstk]);

  inpack16_post(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
		x15, ab, cd);

  k = firstk - 8;
  while (1)
    {
      dec_rounds16(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13,
		  x14, x15, ab, cd, k);

      if (k == 0)
	break;

      fls16(ab, x0, x1, x2, x3, x4, x5, x6, x7, cd, x8, x9, x10, x11, x12, x13,
	    x14, x15, &ctx->key_table[k + 1], &ctx->key_table[k]);

      k -= 8;
    }

  /* load CD for output */
  vmovdqa128(cd[0], x8);
  vmovdqa128(cd[1], x9);
  vmovdqa128(cd[2], x10);
  vmovdqa128(cd[3], x11);
  vmovdqa128(cd[4], x12);
  vmovdqa128(cd[5], x13);
  vmovdqa128(cd[6], x14);
  vmovdqa128(cd[7], x15);

  outunpack16(x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14,
	      x15, ctx->key_table[0], tmp0, tmp1);

  write_output(x7, x6, x5, x4, x3, x2, x1, x0, x15, x14, x13, x12, x11, x10, x9,
	       x8, out);
}

/********* Key setup **********************************************************/

/*
 * Camellia F-function, 1-way SIMD/AESNI.
 *
 * IN:
 *  ab: 64-bit AB state
 *  cd: 64-bit CD state
 */
#define camellia_f(ab, x, t0, t1, t2, t3, t4, inv_shift_row, sbox4mask, \
		   _0f0f0f0fmask, pre_s1lo_mask, pre_s1hi_mask, key) \
	vmovq128((key), t0); \
	load_zero(t3); \
	\
	vpxor128(ab, t0, x); \
	\
	/* \
	 * S-function with AES subbytes \
	 */ \
	\
	/* input rotation for sbox4 (<<< 1) */ \
	vpand128(x, sbox4mask, t0); \
	vpandn128(x, sbox4mask, x); \
	vpaddb128(t0, t0, t1); \
	vpsrl_byte_128(7, t0, t0); \
	vpor128(t0, t1, t0); \
	vpand128(sbox4mask, t0, t0); \
	vpor128(t0, x, x); \
	\
	vmovdqa128_memld(&post_tf_lo_s1, t0); \
	vmovdqa128_memld(&post_tf_hi_s1, t1); \
	\
	/* prefilter sboxes */ \
	filter_8bit(x, pre_s1lo_mask, pre_s1hi_mask, _0f0f0f0fmask, t2); \
	\
	/* AES subbytes + AES shift rows + AES inv shift rows */ \
	aes_subbytes_and_shuf_and_xor(t3, x, x); \
	\
	/* postfilter sboxes */ \
	filter_8bit(x, t0, t1, _0f0f0f0fmask, t2); \
	\
	/* output rotation for sbox2 (<<< 1) */ \
	/* output rotation for sbox3 (>>> 1) */ \
	aes_inv_shuf(inv_shift_row, x, t1); \
	vpshufb128_amemld(&sp0044440444044404mask, x, t4); \
	vpshufb128_amemld(&sp1110111010011110mask, x, x); \
	vpaddb128(t1, t1, t2); \
	vpsrl_byte_128(7, t1, t0); \
	vpsll_byte_128(7, t1, t3); \
	vpor128(t0, t2, t0); \
	vpsrl_byte_128(1, t1, t1); \
	vpshufb128_amemld(&sp0222022222000222mask, t0, t0); \
	vpor128(t1, t3, t1); \
	\
	vpxor128(x, t4, t4); \
	vpshufb128_amemld(&sp3033303303303033mask, t1, t1); \
	vpxor128(t4, t0, t0); \
	vpxor128(t1, t0, t0); \
	vpsrldq128(8, t0, x); \
	vpxor128(t0, x, x); \

#define vec_rol128(in, out, nrol, t0) \
	vpshufd128_0x4e(in, out); \
	vpsllq128((nrol), in, t0); \
	vpsrlq128((64-(nrol)), out, out); \
	vpaddb128(t0, out, out);

#define vec_ror128(in, out, nror, t0) \
	vpshufd128_0x4e(in, out); \
	vpsrlq128((nror), in, t0); \
	vpsllq128((64-(nror)), out, out); \
	vpaddb128(t0, out, out);

#define U64_BYTE(a0, a1, a2, a3, b0, b1, b2, b3) \
	( \
	  SWAP_LE64((((a0) & 0xffULL) << 0) | \
		    (((a1) & 0xffULL) << 8) | \
		    (((a2) & 0xffULL) << 16) | \
		    (((a3) & 0xffULL) << 24) | \
		    (((b0) & 0xffULL) << 32) | \
		    (((b1) & 0xffULL) << 40) | \
		    (((b2) & 0xffULL) << 48) | \
		    (((b3) & 0xffULL) << 56)) \
	)

#define U64_U32(a0, b0) \
	( \
	  SWAP_LE64((((a0) & 0xffffffffULL) << 0) | \
		    (((b0) & 0xffffffffULL) << 32)) \
	)

static const __m128i bswap128_mask =
  M128I_BYTE(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);

static const __m128i inv_shift_row_and_unpcklbw =
  M128I_BYTE(0x00, 0xff, 0x0d, 0xff, 0x0a, 0xff, 0x07, 0xff,
	     0x04, 0xff, 0x01, 0xff, 0x0e, 0xff, 0x0b, 0xff);

static const __m128i sp0044440444044404mask =
  M128I_U32(0xffff0404, 0x0404ff04, 0x0d0dff0d, 0x0d0dff0d);

static const __m128i sp1110111010011110mask =
  M128I_U32(0x000000ff, 0x000000ff, 0x0bffff0b, 0x0b0b0bff);

static const __m128i sp0222022222000222mask =
  M128I_U32(0xff060606, 0xff060606, 0x0c0cffff, 0xff0c0c0c);

static const __m128i sp3033303303303033mask =
  M128I_U32(0x04ff0404, 0x04ff0404, 0xff0a0aff, 0x0aff0a0a);

static const u64 sbox4_input_mask =
  U64_BYTE(0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00);

static const u64 sigma1 =
  U64_U32(0x3BCC908B, 0xA09E667F);

static const u64 sigma2 =
  U64_U32(0x4CAA73B2, 0xB67AE858);

static const u64 sigma3 =
  U64_U32(0xE94F82BE, 0xC6EF372F);

static const u64 sigma4 =
  U64_U32(0xF1D36F1C, 0x54FF53A5);

static const u64 sigma5 =
  U64_U32(0xDE682D1D, 0x10E527FA);

static const u64 sigma6 =
  U64_U32(0xB3E6C1FD, 0xB05688C2);

#define cmll_sub(n, ctx) &ctx->key_table[n]

static ASM_FUNC_ATTR_INLINE void
camellia_setup128(void *key_table, __m128i x0)
{
  struct setup128_ctx_s
  {
    u64 *key_table;
  } sctx = { .key_table = (u64 *)key_table };
  struct setup128_ctx_s *ctx = &sctx;

  /* input:
   *   ctx: subkey storage at key_table(CTX)
   *   x0: key
   */

  __m128i x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
  __m128i tmp0;

#define KL128 x0
#define KA128 x2

  vpshufb128_amemld(&bswap128_mask, KL128, KL128);

  vmovdqa128_memld(&inv_shift_row_and_unpcklbw, x11);
  vmovq128(sbox4_input_mask, x12);
  vmovdqa128_memld(&mask_0f, x13);
  vmovdqa128_memld(&pre_tf_lo_s1, x14);
  vmovdqa128_memld(&pre_tf_hi_s1, x15);

  /*
   * Generate KA
   */
  vpsrldq128(8, KL128, x2);
  vmovdqa128(KL128, x3);
  vpslldq128(8, x3, x3);
  vpsrldq128(8, x3, x3);

  camellia_f(x2, x4, x1,
	     x5, x6, x7, x8,
	     x11, x12, x13, x14, x15, sigma1);
  vpxor128(x4, x3, x3);
  camellia_f(x3, x2, x1,
	     x5, x6, x7, x8,
	     x11, x12, x13, x14, x15, sigma2);
  camellia_f(x2, x3, x1,
	     x5, x6, x7, x8,
	     x11, x12, x13, x14, x15, sigma3);
  vpxor128(x4, x3, x3);
  camellia_f(x3, x4, x1,
	     x5, x6, x7, x8,
	     x11, x12, x13, x14, x15, sigma4);

  vpslldq128(8, x3, x3);
  vpxor128(x4, x2, x2);
  vpsrldq128(8, x3, x3);
  vpslldq128(8, x2, KA128);
  vpor128(x3, KA128, KA128);

  /*
   * Generate subkeys
   */
  vmovdqu128_memst(KA128, cmll_sub(24, ctx));
  vec_rol128(KL128, x3, 15, x15);
  vec_rol128(KA128, x4, 15, x15);
  vec_rol128(KA128, x5, 30, x15);
  vec_rol128(KL128, x6, 45, x15);
  vec_rol128(KA128, x7, 45, x15);
  vec_rol128(KL128, x8, 60, x15);
  vec_rol128(KA128, x9, 60, x15);
  vec_ror128(KL128, x10, 128-77, x15);

  /* absorb kw2 to other subkeys */
  vpslldq128(8, KL128, x15);
  vpsrldq128(8, x15, x15);
  vpxor128(x15, KA128, KA128);
  vpxor128(x15, x3, x3);
  vpxor128(x15, x4, x4);

  /* subl(1) ^= subr(1) & ~subr(9); */
  vpandn128(x15, x5, x13);
  vpslldq128(12, x13, x13);
  vpsrldq128(8, x13, x13);
  vpxor128(x13, x15, x15);
  /* dw = subl(1) & subl(9), subr(1) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x5, x14);
  vpslld128(1, x14, x11);
  vpsrld128(31, x14, x14);
  vpaddb128(x11, x14, x14);
  vpslldq128(8, x14, x14);
  vpsrldq128(12, x14, x14);
  vpxor128(x14, x15, x15);

  vpxor128(x15, x6, x6);
  vpxor128(x15, x8, x8);
  vpxor128(x15, x9, x9);

  /* subl(1) ^= subr(1) & ~subr(17); */
  vpandn128(x15, x10, x13);
  vpslldq128(12, x13, x13);
  vpsrldq128(8, x13, x13);
  vpxor128(x13, x15, x15);
  /* dw = subl(1) & subl(17), subr(1) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x10, x14);
  vpslld128(1, x14, x11);
  vpsrld128(31, x14, x14);
  vpaddb128(x11, x14, x14);
  vpslldq128(8, x14, x14);
  vpsrldq128(12, x14, x14);
  vpxor128(x14, x15, x15);

  vpshufd128_0x1b(KL128, KL128);
  vpshufd128_0x1b(KA128, KA128);
  vpshufd128_0x1b(x3, x3);
  vpshufd128_0x1b(x4, x4);
  vpshufd128_0x1b(x5, x5);
  vpshufd128_0x1b(x6, x6);
  vpshufd128_0x1b(x7, x7);
  vpshufd128_0x1b(x8, x8);
  vpshufd128_0x1b(x9, x9);
  vpshufd128_0x1b(x10, x10);

  vmovdqu128_memst(KL128, cmll_sub(0, ctx));
  vpshufd128_0x1b(KL128, KL128);
  vmovdqu128_memst(KA128, cmll_sub(2, ctx));
  vmovdqu128_memst(x3, cmll_sub(4, ctx));
  vmovdqu128_memst(x4, cmll_sub(6, ctx));
  vmovdqu128_memst(x5, cmll_sub(8, ctx));
  vmovdqu128_memst(x6, cmll_sub(10, ctx));
  vpsrldq128(8, x8, x8);
  vmovq128_memst(x7, cmll_sub(12, ctx));
  vmovq128_memst(x8, cmll_sub(13, ctx));
  vmovdqu128_memst(x9, cmll_sub(14, ctx));
  vmovdqu128_memst(x10, cmll_sub(16, ctx));

  vmovdqu128_memld(cmll_sub(24, ctx), KA128);

  vec_ror128(KL128, x3, 128 - 94, x7);
  vec_ror128(KA128, x4, 128 - 94, x7);
  vec_ror128(KL128, x5, 128 - 111, x7);
  vec_ror128(KA128, x6, 128 - 111, x7);

  vpxor128(x15, x3, x3);
  vpxor128(x15, x4, x4);
  vpxor128(x15, x5, x5);
  vpslldq128(8, x15, x15);
  vpxor128(x15, x6, x6);

  /* absorb kw4 to other subkeys */
  vpslldq128(8, x6, x15);
  vpxor128(x15, x5, x5);
  vpxor128(x15, x4, x4);
  vpxor128(x15, x3, x3);

  /* subl(25) ^= subr(25) & ~subr(16); */
  vmovdqu128_memld(cmll_sub(16, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x10);
  vpandn128(x15, x10, x13);
  vpslldq128(4, x13, x13);
  vpxor128(x13, x15, x15);
  /* dw = subl(25) & subl(16), subr(25) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x10, x14);
  vpslld128(1, x14, x11);
  vpsrld128(31, x14, x14);
  vpaddb128(x11, x14, x14);
  vpsrldq128(12, x14, x14);
  vpslldq128(8, x14, x14);
  vpxor128(x14, x15, x15);

  vpshufd128_0x1b(x3, x3);
  vpshufd128_0x1b(x4, x4);
  vpshufd128_0x1b(x5, x5);
  vpshufd128_0x1b(x6, x6);

  vmovdqu128_memst(x3, cmll_sub(18, ctx));
  vmovdqu128_memst(x4, cmll_sub(20, ctx));
  vmovdqu128_memst(x5, cmll_sub(22, ctx));
  vmovdqu128_memst(x6, cmll_sub(24, ctx));

  vmovdqu128_memld(cmll_sub(14, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x3);
  vmovdqu128_memld(cmll_sub(12, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x4);
  vmovdqu128_memld(cmll_sub(10, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x5);
  vmovdqu128_memld(cmll_sub(8, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x6);

  vpxor128(x15, x3, x3);
  vpxor128(x15, x4, x4);
  vpxor128(x15, x5, x5);

  /* subl(25) ^= subr(25) & ~subr(8); */
  vpandn128(x15, x6, x13);
  vpslldq128(4, x13, x13);
  vpxor128(x13, x15, x15);
  /* dw = subl(25) & subl(8), subr(25) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x6, x14);
  vpslld128(1, x14, x11);
  vpsrld128(31, x14, x14);
  vpaddb128(x11, x14, x14);
  vpsrldq128(12, x14, x14);
  vpslldq128(8, x14, x14);
  vpxor128(x14, x15, x15);

  vpshufd128_0x1b(x3, x3);
  vpshufd128_0x1b(x4, x4);
  vpshufd128_0x1b(x5, x5);

  vmovdqu128_memst(x3, cmll_sub(14, ctx));
  vmovdqu128_memst(x4, cmll_sub(12, ctx));
  vmovdqu128_memst(x5, cmll_sub(10, ctx));

  vmovdqu128_memld(cmll_sub(6, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x6);
  vmovdqu128_memld(cmll_sub(4, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x4);
  vmovdqu128_memld(cmll_sub(2, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x2);
  vmovdqu128_memld(cmll_sub(0, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x0);

  vpxor128(x15, x6, x6);
  vpxor128(x15, x4, x4);
  vpxor128(x15, x2, x2);
  vpxor128(x15, x0, x0);

  vpshufd128_0x1b(x6, x6);
  vpshufd128_0x1b(x4, x4);
  vpshufd128_0x1b(x2, x2);
  vpshufd128_0x1b(x0, x0);

  vpsrldq128(8, x2, x3);
  vpsrldq128(8, x4, x5);
  vpsrldq128(8, x6, x7);

  /*
   * key XOR is end of F-function.
   */
  vpxor128(x2, x0, x0);
  vpxor128(x4, x2, x2);

  vmovq128_memst(x0, cmll_sub(0, ctx));
  vmovq128_memst(x3, cmll_sub(2, ctx));
  vpxor128(x5, x3, x3);
  vpxor128(x6, x4, x4);
  vpxor128(x7, x5, x5);
  vmovq128_memst(x2, cmll_sub(3, ctx));
  vmovq128_memst(x3, cmll_sub(4, ctx));
  vmovq128_memst(x4, cmll_sub(5, ctx));
  vmovq128_memst(x5, cmll_sub(6, ctx));

  vmovq128(*cmll_sub(7, ctx), x7);
  vmovq128(*cmll_sub(8, ctx), x8);
  vmovq128(*cmll_sub(9, ctx), x9);
  vmovq128(*cmll_sub(10, ctx), x10);
  /* tl = subl(10) ^ (subr(10) & ~subr(8)); */
  vpandn128(x10, x8, x15);
  vpsrldq128(4, x15, x15);
  vpxor128(x15, x10, x0);
  /* dw = tl & subl(8), tr = subr(10) ^ CAMELLIA_RL1(dw); */
  vpand128(x8, x0, x15);
  vpslld128(1, x15, x14);
  vpsrld128(31, x15, x15);
  vpaddb128(x14, x15, x15);
  vpslldq128(12, x15, x15);
  vpsrldq128(8, x15, x15);
  vpxor128(x15, x0, x0);

  vpxor128(x0, x6, x6);
  vmovq128_memst(x6, cmll_sub(7, ctx));

  vmovq128(*cmll_sub(11, ctx), x11);
  vmovq128(*cmll_sub(12, ctx), x12);
  vmovq128(*cmll_sub(13, ctx), x13);
  vmovq128(*cmll_sub(14, ctx), x14);
  vmovq128(*cmll_sub(15, ctx), x15);
  /* tl = subl(7) ^ (subr(7) & ~subr(9)); */
  vpandn128(x7, x9, x1);
  vpsrldq128(4, x1, x1);
  vpxor128(x1, x7, x0);
  /* dw = tl & subl(9), tr = subr(7) ^ CAMELLIA_RL1(dw); */
  vpand128(x9, x0, x1);
  vpslld128(1, x1, x2);
  vpsrld128(31, x1, x1);
  vpaddb128(x2, x1, x1);
  vpslldq128(12, x1, x1);
  vpsrldq128(8, x1, x1);
  vpxor128(x1, x0, x0);

  vpxor128(x11, x0, x0);
  vpxor128(x12, x10, x10);
  vpxor128(x13, x11, x11);
  vpxor128(x14, x12, x12);
  vpxor128(x15, x13, x13);
  vmovq128_memst(x0, cmll_sub(10, ctx));
  vmovq128_memst(x10, cmll_sub(11, ctx));
  vmovq128_memst(x11, cmll_sub(12, ctx));
  vmovq128_memst(x12, cmll_sub(13, ctx));
  vmovq128_memst(x13, cmll_sub(14, ctx));

  vmovq128(*cmll_sub(16, ctx), x6);
  vmovq128(*cmll_sub(17, ctx), x7);
  vmovq128(*cmll_sub(18, ctx), x8);
  vmovq128(*cmll_sub(19, ctx), x9);
  vmovq128(*cmll_sub(20, ctx), x10);
  /* tl = subl(18) ^ (subr(18) & ~subr(16)); */
  vpandn128(x8, x6, x1);
  vpsrldq128(4, x1, x1);
  vpxor128(x1, x8, x0);
  /* dw = tl & subl(16), tr = subr(18) ^ CAMELLIA_RL1(dw); */
  vpand128(x6, x0, x1);
  vpslld128(1, x1, x2);
  vpsrld128(31, x1, x1);
  vpaddb128(x2, x1, x1);
  vpslldq128(12, x1, x1);
  vpsrldq128(8, x1, x1);
  vpxor128(x1, x0, x0);

  vpxor128(x14, x0, x0);
  vmovq128_memst(x0, cmll_sub(15, ctx));

  /* tl = subl(15) ^ (subr(15) & ~subr(17)); */
  vpandn128(x15, x7, x1);
  vpsrldq128(4, x1, x1);
  vpxor128(x1, x15, x0);
  /* dw = tl & subl(17), tr = subr(15) ^ CAMELLIA_RL1(dw); */
  vpand128(x7, x0, x1);
  vpslld128(1, x1, x2);
  vpsrld128(31, x1, x1);
  vpaddb128(x2, x1, x1);
  vpslldq128(12, x1, x1);
  vpsrldq128(8, x1, x1);
  vpxor128(x1, x0, x0);

  vmovq128(*cmll_sub(21, ctx), x1);
  vmovq128(*cmll_sub(22, ctx), x2);
  vmovq128(*cmll_sub(23, ctx), x3);
  vmovq128(*cmll_sub(24, ctx), x4);

  vpxor128(x9, x0, x0);
  vpxor128(x10, x8, x8);
  vpxor128(x1, x9, x9);
  vpxor128(x2, x10, x10);
  vpxor128(x3, x1, x1);
  vpxor128(x4, x3, x3);

  vmovq128_memst(x0, cmll_sub(18, ctx));
  vmovq128_memst(x8, cmll_sub(19, ctx));
  vmovq128_memst(x9, cmll_sub(20, ctx));
  vmovq128_memst(x10, cmll_sub(21, ctx));
  vmovq128_memst(x1, cmll_sub(22, ctx));
  vmovq128_memst(x2, cmll_sub(23, ctx));
  vmovq128_memst(x3, cmll_sub(24, ctx));

#undef KL128
#undef KA128

  /* kw2 and kw4 are unused now. */
  load_zero(tmp0);
  vmovq128_memst(tmp0, cmll_sub(1, ctx));
  vmovq128_memst(tmp0, cmll_sub(25, ctx));
}

static ASM_FUNC_ATTR_INLINE void
camellia_setup256(void *key_table, __m128i x0, __m128i x1)
{
  struct setup256_ctx_s
  {
    u64 *key_table;
  } sctx = { .key_table = (u64 *)key_table };
  struct setup256_ctx_s *ctx = &sctx;

  /* input:
   *   ctx: subkey storage at key_table(CTX)
   *   x0, x1: key
   */

  __m128i x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15;
  __m128i tmp0;

#define KL128 x0
#define KR128 x1
#define KA128 x2
#define KB128 x3

  vpshufb128_amemld(&bswap128_mask, KL128, KL128);
  vpshufb128_amemld(&bswap128_mask, KR128, KR128);

  vmovdqa128_memld(&inv_shift_row_and_unpcklbw, x11);
  vmovq128(*&sbox4_input_mask, x12);
  vmovdqa128_memld(&mask_0f, x13);
  vmovdqa128_memld(&pre_tf_lo_s1, x14);
  vmovdqa128_memld(&pre_tf_hi_s1, x15);

  /*
   * Generate KA
   */
  vpxor128(KL128, KR128, x3);
  vpsrldq128(8, KR128, x6);
  vpsrldq128(8, x3, x2);
  vpslldq128(8, x3, x3);
  vpsrldq128(8, x3, x3);

  camellia_f(x2, x4, x5,
	     x7, x8, x9, x10,
	     x11, x12, x13, x14, x15, sigma1);
  vpxor128(x4, x3, x3);
  camellia_f(x3, x2, x5,
	     x7, x8, x9, x10,
	     x11, x12, x13, x14, x15, sigma2);
  vpxor128(x6, x2, x2);
  camellia_f(x2, x3, x5,
	     x7, x8, x9, x10,
	     x11, x12, x13, x14, x15, sigma3);
  vpxor128(x4, x3, x3);
  vpxor128(KR128, x3, x3);
  camellia_f(x3, x4, x5,
	     x7, x8, x9, x10,
	     x11, x12, x13, x14, x15, sigma4);

  vpslldq128(8, x3, x3);
  vpxor128(x4, x2, x2);
  vpsrldq128(8, x3, x3);
  vpslldq128(8, x2, KA128);
  vpor128(x3, KA128, KA128);

  /*
   * Generate KB
   */
  vpxor128(KA128, KR128, x3);
  vpsrldq128(8, x3, x4);
  vpslldq128(8, x3, x3);
  vpsrldq128(8, x3, x3);

  camellia_f(x4, x5, x6,
	     x7, x8, x9, x10,
	     x11, x12, x13, x14, x15, sigma5);
  vpxor128(x5, x3, x3);

  camellia_f(x3, x5, x6,
	     x7, x8, x9, x10,
	     x11, x12, x13, x14, x15, sigma6);
  vpslldq128(8, x3, x3);
  vpxor128(x5, x4, x4);
  vpsrldq128(8, x3, x3);
  vpslldq128(8, x4, x4);
  vpor128(x3, x4, KB128);

  /*
   * Generate subkeys
   */
  vmovdqu128_memst(KB128, cmll_sub(32, ctx));
  vec_rol128(KR128, x4, 15, x15);
  vec_rol128(KA128, x5, 15, x15);
  vec_rol128(KR128, x6, 30, x15);
  vec_rol128(KB128, x7, 30, x15);
  vec_rol128(KL128, x8, 45, x15);
  vec_rol128(KA128, x9, 45, x15);
  vec_rol128(KL128, x10, 60, x15);
  vec_rol128(KR128, x11, 60, x15);
  vec_rol128(KB128, x12, 60, x15);

  /* absorb kw2 to other subkeys */
  vpslldq128(8, KL128, x15);
  vpsrldq128(8, x15, x15);
  vpxor128(x15, KB128, KB128);
  vpxor128(x15, x4, x4);
  vpxor128(x15, x5, x5);

  /* subl(1) ^= subr(1) & ~subr(9); */
  vpandn128(x15, x6, x13);
  vpslldq128(12, x13, x13);
  vpsrldq128(8, x13, x13);
  vpxor128(x13, x15, x15);
  /* dw = subl(1) & subl(9), subr(1) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x6, x14);
  vpslld128(1, x14, x13);
  vpsrld128(31, x14, x14);
  vpaddb128(x13, x14, x14);
  vpslldq128(8, x14, x14);
  vpsrldq128(12, x14, x14);
  vpxor128(x14, x15, x15);

  vpxor128(x15, x7, x7);
  vpxor128(x15, x8, x8);
  vpxor128(x15, x9, x9);

  vpshufd128_0x1b(KL128, KL128);
  vpshufd128_0x1b(KB128, KB128);
  vpshufd128_0x1b(x4, x4);
  vpshufd128_0x1b(x5, x5);
  vpshufd128_0x1b(x6, x6);
  vpshufd128_0x1b(x7, x7);
  vpshufd128_0x1b(x8, x8);
  vpshufd128_0x1b(x9, x9);

  vmovdqu128_memst(KL128, cmll_sub(0, ctx));
  vpshufd128_0x1b(KL128, KL128);
  vmovdqu128_memst(KB128, cmll_sub(2, ctx));
  vmovdqu128_memst(x4, cmll_sub(4, ctx));
  vmovdqu128_memst(x5, cmll_sub(6, ctx));
  vmovdqu128_memst(x6, cmll_sub(8, ctx));
  vmovdqu128_memst(x7, cmll_sub(10, ctx));
  vmovdqu128_memst(x8, cmll_sub(12, ctx));
  vmovdqu128_memst(x9, cmll_sub(14, ctx));

  vmovdqu128_memld(cmll_sub(32, ctx), KB128);

  /* subl(1) ^= subr(1) & ~subr(17); */
  vpandn128(x15, x10, x13);
  vpslldq128(12, x13, x13);
  vpsrldq128(8, x13, x13);
  vpxor128(x13, x15, x15);
  /* dw = subl(1) & subl(17), subr(1) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x10, x14);
  vpslld128(1, x14, x13);
  vpsrld128(31, x14, x14);
  vpaddb128(x13, x14, x14);
  vpslldq128(8, x14, x14);
  vpsrldq128(12, x14, x14);
  vpxor128(x14, x15, x15);

  vpxor128(x15, x11, x11);
  vpxor128(x15, x12, x12);

  vec_ror128(KL128, x4, 128-77, x14);
  vec_ror128(KA128, x5, 128-77, x14);
  vec_ror128(KR128, x6, 128-94, x14);
  vec_ror128(KA128, x7, 128-94, x14);
  vec_ror128(KL128, x8, 128-111, x14);
  vec_ror128(KB128, x9, 128-111, x14);

  vpxor128(x15, x4, x4);

  vpshufd128_0x1b(x10, x10);
  vpshufd128_0x1b(x11, x11);
  vpshufd128_0x1b(x12, x12);
  vpshufd128_0x1b(x4, x4);

  vmovdqu128_memst(x10, cmll_sub(16, ctx));
  vmovdqu128_memst(x11, cmll_sub(18, ctx));
  vmovdqu128_memst(x12, cmll_sub(20, ctx));
  vmovdqu128_memst(x4, cmll_sub(22, ctx));

  /* subl(1) ^= subr(1) & ~subr(25); */
  vpandn128(x15, x5, x13);
  vpslldq128(12, x13, x13);
  vpsrldq128(8, x13, x13);
  vpxor128(x13, x15, x15);
  /* dw = subl(1) & subl(25), subr(1) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x5, x14);
  vpslld128(1, x14, x13);
  vpsrld128(31, x14, x14);
  vpaddb128(x13, x14, x14);
  vpslldq128(8, x14, x14);
  vpsrldq128(12, x14, x14);
  vpxor128(x14, x15, x15);

  vpxor128(x15, x6, x6);
  vpxor128(x15, x7, x7);
  vpxor128(x15, x8, x8);
  vpslldq128(8, x15, x15);
  vpxor128(x15, x9, x9);

  /* absorb kw4 to other subkeys */
  vpslldq128(8, x9, x15);
  vpxor128(x15, x8, x8);
  vpxor128(x15, x7, x7);
  vpxor128(x15, x6, x6);

  /* subl(33) ^= subr(33) & ~subr(24); */
  vpandn128(x15, x5, x14);
  vpslldq128(4, x14, x14);
  vpxor128(x14, x15, x15);
  /* dw = subl(33) & subl(24), subr(33) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x5, x14);
  vpslld128(1, x14, x13);
  vpsrld128(31, x14, x14);
  vpaddb128(x13, x14, x14);
  vpsrldq128(12, x14, x14);
  vpslldq128(8, x14, x14);
  vpxor128(x14, x15, x15);

  vpshufd128_0x1b(x5, x5);
  vpshufd128_0x1b(x6, x6);
  vpshufd128_0x1b(x7, x7);
  vpshufd128_0x1b(x8, x8);
  vpshufd128_0x1b(x9, x9);

  vmovdqu128_memst(x5, cmll_sub(24, ctx));
  vmovdqu128_memst(x6, cmll_sub(26, ctx));
  vmovdqu128_memst(x7, cmll_sub(28, ctx));
  vmovdqu128_memst(x8, cmll_sub(30, ctx));
  vmovdqu128_memst(x9, cmll_sub(32, ctx));

  vmovdqu128_memld(cmll_sub(22, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x0);
  vmovdqu128_memld(cmll_sub(20, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x1);
  vmovdqu128_memld(cmll_sub(18, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x2);
  vmovdqu128_memld(cmll_sub(16, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x3);
  vmovdqu128_memld(cmll_sub(14, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x4);
  vmovdqu128_memld(cmll_sub(12, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x5);
  vmovdqu128_memld(cmll_sub(10, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x6);
  vmovdqu128_memld(cmll_sub(8, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x7);

  vpxor128(x15, x0, x0);
  vpxor128(x15, x1, x1);
  vpxor128(x15, x2, x2);

  /* subl(33) ^= subr(33) & ~subr(24); */
  vpandn128(x15, x3, x14);
  vpslldq128(4, x14, x14);
  vpxor128(x14, x15, x15);
  /* dw = subl(33) & subl(24), subr(33) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x3, x14);
  vpslld128(1, x14, x13);
  vpsrld128(31, x14, x14);
  vpaddb128(x13, x14, x14);
  vpsrldq128(12, x14, x14);
  vpslldq128(8, x14, x14);
  vpxor128(x14, x15, x15);

  vpxor128(x15, x4, x4);
  vpxor128(x15, x5, x5);
  vpxor128(x15, x6, x6);

  vpshufd128_0x1b(x0, x0);
  vpshufd128_0x1b(x1, x1);
  vpshufd128_0x1b(x2, x2);
  vpshufd128_0x1b(x4, x4);
  vpshufd128_0x1b(x5, x5);
  vpshufd128_0x1b(x6, x6);

  vmovdqu128_memst(x0, cmll_sub(22, ctx));
  vmovdqu128_memst(x1, cmll_sub(20, ctx));
  vmovdqu128_memst(x2, cmll_sub(18, ctx));
  vmovdqu128_memst(x4, cmll_sub(14, ctx));
  vmovdqu128_memst(x5, cmll_sub(12, ctx));
  vmovdqu128_memst(x6, cmll_sub(10, ctx));

  vmovdqu128_memld(cmll_sub(6, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x6);
  vmovdqu128_memld(cmll_sub(4, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x4);
  vmovdqu128_memld(cmll_sub(2, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x2);
  vmovdqu128_memld(cmll_sub(0, ctx), tmp0);
  vpshufd128_0x1b(tmp0, x0);

  /* subl(33) ^= subr(33) & ~subr(24); */
  vpandn128(x15, x7, x14);
  vpslldq128(4, x14, x14);
  vpxor128(x14, x15, x15);
  /* dw = subl(33) & subl(24), subr(33) ^= CAMELLIA_RL1(dw); */
  vpand128(x15, x7, x14);
  vpslld128(1, x14, x13);
  vpsrld128(31, x14, x14);
  vpaddb128(x13, x14, x14);
  vpsrldq128(12, x14, x14);
  vpslldq128(8, x14, x14);
  vpxor128(x14, x15, x15);

  vpxor128(x15, x6, x6);
  vpxor128(x15, x4, x4);
  vpxor128(x15, x2, x2);
  vpxor128(x15, x0, x0);

  vpshufd128_0x1b(x6, x6);
  vpshufd128_0x1b(x4, x4);
  vpshufd128_0x1b(x2, x2);
  vpshufd128_0x1b(x0, x0);

  vpsrldq128(8, x2, x3);
  vpsrldq128(8, x4, x5);
  vpsrldq128(8, x6, x7);

  /*
    * key XOR is end of F-function.
    */
  vpxor128(x2, x0, x0);
  vpxor128(x4, x2, x2);

  vmovq128_memst(x0, cmll_sub(0, ctx));
  vmovq128_memst(x3, cmll_sub(2, ctx));
  vpxor128(x5, x3, x3);
  vpxor128(x6, x4, x4);
  vpxor128(x7, x5, x5);
  vmovq128_memst(x2, cmll_sub(3, ctx));
  vmovq128_memst(x3, cmll_sub(4, ctx));
  vmovq128_memst(x4, cmll_sub(5, ctx));
  vmovq128_memst(x5, cmll_sub(6, ctx));

  vmovq128(*cmll_sub(7, ctx), x7);
  vmovq128(*cmll_sub(8, ctx), x8);
  vmovq128(*cmll_sub(9, ctx), x9);
  vmovq128(*cmll_sub(10, ctx), x10);
  /* tl = subl(10) ^ (subr(10) & ~subr(8)); */
  vpandn128(x10, x8, x15);
  vpsrldq128(4, x15, x15);
  vpxor128(x15, x10, x0);
  /* dw = tl & subl(8), tr = subr(10) ^ CAMELLIA_RL1(dw); */
  vpand128(x8, x0, x15);
  vpslld128(1, x15, x14);
  vpsrld128(31, x15, x15);
  vpaddb128(x14, x15, x15);
  vpslldq128(12, x15, x15);
  vpsrldq128(8, x15, x15);
  vpxor128(x15, x0, x0);

  vpxor128(x0, x6, x6);
  vmovq128_memst(x6, cmll_sub(7, ctx));

  vmovq128(*cmll_sub(11, ctx), x11);
  vmovq128(*cmll_sub(12, ctx), x12);
  vmovq128(*cmll_sub(13, ctx), x13);
  vmovq128(*cmll_sub(14, ctx), x14);
  vmovq128(*cmll_sub(15, ctx), x15);
  /* tl = subl(7) ^ (subr(7) & ~subr(9)); */
  vpandn128(x7, x9, x1);
  vpsrldq128(4, x1, x1);
  vpxor128(x1, x7, x0);
  /* dw = tl & subl(9), tr = subr(7) ^ CAMELLIA_RL1(dw); */
  vpand128(x9, x0, x1);
  vpslld128(1, x1, x2);
  vpsrld128(31, x1, x1);
  vpaddb128(x2, x1, x1);
  vpslldq128(12, x1, x1);
  vpsrldq128(8, x1, x1);
  vpxor128(x1, x0, x0);

  vpxor128(x11, x0, x0);
  vpxor128(x12, x10, x10);
  vpxor128(x13, x11, x11);
  vpxor128(x14, x12, x12);
  vpxor128(x15, x13, x13);
  vmovq128_memst(x0, cmll_sub(10, ctx));
  vmovq128_memst(x10, cmll_sub(11, ctx));
  vmovq128_memst(x11, cmll_sub(12, ctx));
  vmovq128_memst(x12, cmll_sub(13, ctx));
  vmovq128_memst(x13, cmll_sub(14, ctx));

  vmovq128(*cmll_sub(16, ctx), x6);
  vmovq128(*cmll_sub(17, ctx), x7);
  vmovq128(*cmll_sub(18, ctx), x8);
  vmovq128(*cmll_sub(19, ctx), x9);
  vmovq128(*cmll_sub(20, ctx), x10);
  /* tl = subl(18) ^ (subr(18) & ~subr(16)); */
  vpandn128(x8, x6, x1);
  vpsrldq128(4, x1, x1);
  vpxor128(x1, x8, x0);
  /* dw = tl & subl(16), tr = subr(18) ^ CAMELLIA_RL1(dw); */
  vpand128(x6, x0, x1);
  vpslld128(1, x1, x2);
  vpsrld128(31, x1, x1);
  vpaddb128(x2, x1, x1);
  vpslldq128(12, x1, x1);
  vpsrldq128(8, x1, x1);
  vpxor128(x1, x0, x0);

  vpxor128(x14, x0, x0);
  vmovq128_memst(x0, cmll_sub(15, ctx));

  /* tl = subl(15) ^ (subr(15) & ~subr(17)); */
  vpandn128(x15, x7, x1);
  vpsrldq128(4, x1, x1);
  vpxor128(x1, x15, x0);
  /* dw = tl & subl(17), tr = subr(15) ^ CAMELLIA_RL1(dw); */
  vpand128(x7, x0, x1);
  vpslld128(1, x1, x2);
  vpsrld128(31, x1, x1);
  vpaddb128(x2, x1, x1);
  vpslldq128(12, x1, x1);
  vpsrldq128(8, x1, x1);
  vpxor128(x1, x0, x0);

  vmovq128(*cmll_sub(21, ctx), x1);
  vmovq128(*cmll_sub(22, ctx), x2);
  vmovq128(*cmll_sub(23, ctx), x3);
  vmovq128(*cmll_sub(24, ctx), x4);

  vpxor128(x9, x0, x0);
  vpxor128(x10, x8, x8);
  vpxor128(x1, x9, x9);
  vpxor128(x2, x10, x10);
  vpxor128(x3, x1, x1);

  vmovq128_memst(x0, cmll_sub(18, ctx));
  vmovq128_memst(x8, cmll_sub(19, ctx));
  vmovq128_memst(x9, cmll_sub(20, ctx));
  vmovq128_memst(x10, cmll_sub(21, ctx));
  vmovq128_memst(x1, cmll_sub(22, ctx));

  vmovq128(*cmll_sub(25, ctx), x5);
  vmovq128(*cmll_sub(26, ctx), x6);
  vmovq128(*cmll_sub(27, ctx), x7);
  vmovq128(*cmll_sub(28, ctx), x8);
  vmovq128(*cmll_sub(29, ctx), x9);
  vmovq128(*cmll_sub(30, ctx), x10);
  vmovq128(*cmll_sub(31, ctx), x11);
  vmovq128(*cmll_sub(32, ctx), x12);

  /* tl = subl(26) ^ (subr(26) & ~subr(24)); */
  vpandn128(x6, x4, x15);
  vpsrldq128(4, x15, x15);
  vpxor128(x15, x6, x0);
  /* dw = tl & subl(26), tr = subr(24) ^ CAMELLIA_RL1(dw); */
  vpand128(x4, x0, x15);
  vpslld128(1, x15, x14);
  vpsrld128(31, x15, x15);
  vpaddb128(x14, x15, x15);
  vpslldq128(12, x15, x15);
  vpsrldq128(8, x15, x15);
  vpxor128(x15, x0, x0);

  vpxor128(x0, x2, x2);
  vmovq128_memst(x2, cmll_sub(23, ctx));

  /* tl = subl(23) ^ (subr(23) &  ~subr(25)); */
  vpandn128(x3, x5, x15);
  vpsrldq128(4, x15, x15);
  vpxor128(x15, x3, x0);
  /* dw = tl & subl(26), tr = subr(24) ^ CAMELLIA_RL1(dw); */
  vpand128(x5, x0, x15);
  vpslld128(1, x15, x14);
  vpsrld128(31, x15, x15);
  vpaddb128(x14, x15, x15);
  vpslldq128(12, x15, x15);
  vpsrldq128(8, x15, x15);
  vpxor128(x15, x0, x0);

  vpxor128(x7, x0, x0);
  vpxor128(x8, x6, x6);
  vpxor128(x9, x7, x7);
  vpxor128(x10, x8, x8);
  vpxor128(x11, x9, x9);
  vpxor128(x12, x11, x11);

  vmovq128_memst(x0, cmll_sub(26, ctx));
  vmovq128_memst(x6, cmll_sub(27, ctx));
  vmovq128_memst(x7, cmll_sub(28, ctx));
  vmovq128_memst(x8, cmll_sub(29, ctx));
  vmovq128_memst(x9, cmll_sub(30, ctx));
  vmovq128_memst(x10, cmll_sub(31, ctx));
  vmovq128_memst(x11, cmll_sub(32, ctx));

#undef KL128
#undef KR128
#undef KA128
#undef KB128

  /* kw2 and kw4 are unused now. */
  load_zero(tmp0);
  vmovq128_memst(tmp0, cmll_sub(1, ctx));
  vmovq128_memst(tmp0, cmll_sub(33, ctx));
}

void ASM_FUNC_ATTR_NOINLINE
FUNC_KEY_SETUP(void *key_table, const void *vkey, unsigned int keylen)
{
  const char *key = vkey;

  /* input:
   *   key_table: subkey storage at key_table(CTX)
   *   key_length_bits: output key length as number of bits
   *   key: input key buffer
   *   keylen: key length in bytes
   */

  __m128i x0, x1, x2;

  switch (keylen)
    {
      default:
	return; /* Unsupported key length! */

      case 16:
	vmovdqu128_memld(key, x0);
	camellia_setup128(key_table, x0);
	return;

      case 24:
	vmovdqu128_memld(key, x0);
	vmovq128(*(uint64_unaligned_t *)(key + 16), x1);

	x2[0] = -1;
	x2[1] = -1;
	vpxor128(x1, x2, x2);
	vpslldq128(8, x2, x2);
	vpor128(x2, x1, x1);
	break;

      case 32:
	vmovdqu128_memld(key, x0);
	vmovdqu128_memld(key + 16, x1);
	break;
    }

  camellia_setup256(key_table, x0, x1);
}
