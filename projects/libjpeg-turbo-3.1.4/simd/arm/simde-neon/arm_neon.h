/*
 * Arm Neon intrinsics for x86-64, provided by SIMDe (SIMD Everywhere)
 *
 * Without AVX2, SIMDe implements the interleaving loads and stores, variable
 * shifts, and bit counts below with element-by-element loops, and it
 * implements every load and store with memcpy().  This header replaces the
 * ones that the Neon SIMD extensions use in hot paths with SSE2 or SSSE3 code.
 */

#ifndef JSIMD_SIMDE_ARM_NEON_H
#define JSIMD_SIMDE_ARM_NEON_H

#define SIMDE_ENABLE_NATIVE_ALIASES
#include <simde/arm/neon.h>

#if defined(SIMDE_X86_SSE2_NATIVE) && defined(SIMDE_X86_MMX_NATIVE) && \
    !defined(SIMDE_ARM_NEON_NEED_PORTABLE_64BIT) && \
    !defined(SIMDE_ARM_NEON_NEED_PORTABLE_128BIT)

#include <stdint.h>
#include <string.h>
#include <emmintrin.h>

/* SIMDe represents Neon vectors as 64-bit and 128-bit vector types, which
 * convert to and from __m64 and __m128i with casts.
 */
#define JSIMDE_Q(v)  ((__m128i)(v))
#define JSIMDE_D(v)  _mm_movpi64_epi64((__m64)(v))
#define JSIMDE_TO_D(T, x)  ((T)_mm_movepi64_pi64(x))

/* SIMDe loads and stores vectors with memcpy().  Under Fil-C, an 8- or
 * 16-byte memcpy() may carry a pointer, so it also copies or clears the
 * capability of each word.  Vector loads and stores carry only data.
 */
#define JSIMDE_LD1Q(T, p)  ((T)_mm_loadu_si128((const __m128i *)(p)))
#define JSIMDE_LD1(T, p) \
  JSIMDE_TO_D(T, _mm_loadl_epi64((const __m128i *)(p)))
#define JSIMDE_ST1Q(p, v)  _mm_storeu_si128((__m128i *)(p), JSIMDE_Q(v))
#define JSIMDE_ST1(p, v)  _mm_storel_epi64((__m128i *)(p), JSIMDE_D(v))

#undef vld1q_s8
#define vld1q_s8(p)  JSIMDE_LD1Q(simde_int8x16_t, p)
#undef vld1q_u8
#define vld1q_u8(p)  JSIMDE_LD1Q(simde_uint8x16_t, p)
#undef vld1q_s16
#define vld1q_s16(p)  JSIMDE_LD1Q(simde_int16x8_t, p)
#undef vld1q_u16
#define vld1q_u16(p)  JSIMDE_LD1Q(simde_uint16x8_t, p)
#undef vld1q_s32
#define vld1q_s32(p)  JSIMDE_LD1Q(simde_int32x4_t, p)
#undef vld1q_u32
#define vld1q_u32(p)  JSIMDE_LD1Q(simde_uint32x4_t, p)
#undef vld1_s8
#define vld1_s8(p)  JSIMDE_LD1(simde_int8x8_t, p)
#undef vld1_u8
#define vld1_u8(p)  JSIMDE_LD1(simde_uint8x8_t, p)
#undef vld1_s16
#define vld1_s16(p)  JSIMDE_LD1(simde_int16x4_t, p)
#undef vld1_u16
#define vld1_u16(p)  JSIMDE_LD1(simde_uint16x4_t, p)
#undef vld1_s32
#define vld1_s32(p)  JSIMDE_LD1(simde_int32x2_t, p)
#undef vld1_u32
#define vld1_u32(p)  JSIMDE_LD1(simde_uint32x2_t, p)
#undef vst1q_s8
#define vst1q_s8(p, v)  JSIMDE_ST1Q(p, v)
#undef vst1q_u8
#define vst1q_u8(p, v)  JSIMDE_ST1Q(p, v)
#undef vst1q_s16
#define vst1q_s16(p, v)  JSIMDE_ST1Q(p, v)
#undef vst1q_u16
#define vst1q_u16(p, v)  JSIMDE_ST1Q(p, v)
#undef vst1q_s32
#define vst1q_s32(p, v)  JSIMDE_ST1Q(p, v)
#undef vst1q_u32
#define vst1q_u32(p, v)  JSIMDE_ST1Q(p, v)
#undef vst1_s8
#define vst1_s8(p, v)  JSIMDE_ST1(p, v)
#undef vst1_u8
#define vst1_u8(p, v)  JSIMDE_ST1(p, v)
#undef vst1_s16
#define vst1_s16(p, v)  JSIMDE_ST1(p, v)
#undef vst1_u16
#define vst1_u16(p, v)  JSIMDE_ST1(p, v)
#undef vst1_s32
#define vst1_s32(p, v)  JSIMDE_ST1(p, v)
#undef vst1_u32
#define vst1_u32(p, v)  JSIMDE_ST1(p, v)

static inline void jsimde_store3(uint8_t *ptr, __m128i c0, __m128i c1,
                                 __m128i c2, __m128i c3, int n)
{
  /* c0..c3 hold 12 bytes each in their low bytes.  n is 2 (24 bytes) or
   * 4 (48 bytes).
   */
  _mm_storeu_si128((__m128i *)ptr, _mm_or_si128(c0, _mm_slli_si128(c1, 12)));
  if (n == 2) {
    _mm_storel_epi64((__m128i *)(ptr + 16), _mm_srli_si128(c1, 4));
    return;
  }
  _mm_storeu_si128((__m128i *)(ptr + 16),
                   _mm_or_si128(_mm_srli_si128(c1, 4), _mm_slli_si128(c2, 8)));
  _mm_storeu_si128((__m128i *)(ptr + 32),
                   _mm_or_si128(_mm_srli_si128(c2, 8), _mm_slli_si128(c3, 4)));
}

/* Pack four 4-byte pixels (fourth byte zero) into 12 contiguous bytes. */
static inline __m128i jsimde_pack_3of4(__m128i v)
{
  const __m128i lo32 = _mm_set_epi32(0, -1, 0, -1);
  __m128i t = _mm_or_si128(_mm_and_si128(v, lo32),
                           _mm_srli_epi64(_mm_andnot_si128(lo32, v), 8));
  const __m128i lo64 = _mm_set_epi32(0, 0, -1, -1);
  return _mm_or_si128(_mm_and_si128(t, lo64),
                      _mm_srli_si128(_mm_andnot_si128(lo64, t), 2));
}

static inline void jsimde_vst3q_u8(uint8_t *ptr, __m128i a, __m128i b,
                                   __m128i c)
{
  const __m128i z = _mm_setzero_si128();
  __m128i ab_lo = _mm_unpacklo_epi8(a, b), ab_hi = _mm_unpackhi_epi8(a, b);
  __m128i c0_lo = _mm_unpacklo_epi8(c, z), c0_hi = _mm_unpackhi_epi8(c, z);
  jsimde_store3(ptr,
                jsimde_pack_3of4(_mm_unpacklo_epi16(ab_lo, c0_lo)),
                jsimde_pack_3of4(_mm_unpackhi_epi16(ab_lo, c0_lo)),
                jsimde_pack_3of4(_mm_unpacklo_epi16(ab_hi, c0_hi)),
                jsimde_pack_3of4(_mm_unpackhi_epi16(ab_hi, c0_hi)), 4);
}

static inline void jsimde_vst3_u8(uint8_t *ptr, __m128i a, __m128i b,
                                  __m128i c)
{
  __m128i ab = _mm_unpacklo_epi8(a, b);
  __m128i c0 = _mm_unpacklo_epi8(c, _mm_setzero_si128());
  jsimde_store3(ptr, jsimde_pack_3of4(_mm_unpacklo_epi16(ab, c0)),
                jsimde_pack_3of4(_mm_unpackhi_epi16(ab, c0)),
                _mm_setzero_si128(), _mm_setzero_si128(), 2);
}

static inline void jsimde_vst4q_u8(uint8_t *ptr, __m128i a, __m128i b,
                                   __m128i c, __m128i d)
{
  __m128i ab_lo = _mm_unpacklo_epi8(a, b), ab_hi = _mm_unpackhi_epi8(a, b);
  __m128i cd_lo = _mm_unpacklo_epi8(c, d), cd_hi = _mm_unpackhi_epi8(c, d);
  _mm_storeu_si128((__m128i *)ptr, _mm_unpacklo_epi16(ab_lo, cd_lo));
  _mm_storeu_si128((__m128i *)(ptr + 16), _mm_unpackhi_epi16(ab_lo, cd_lo));
  _mm_storeu_si128((__m128i *)(ptr + 32), _mm_unpacklo_epi16(ab_hi, cd_hi));
  _mm_storeu_si128((__m128i *)(ptr + 48), _mm_unpackhi_epi16(ab_hi, cd_hi));
}

static inline void jsimde_vst4_u8(uint8_t *ptr, __m128i a, __m128i b,
                                  __m128i c, __m128i d)
{
  __m128i ab = _mm_unpacklo_epi8(a, b), cd = _mm_unpacklo_epi8(c, d);
  _mm_storeu_si128((__m128i *)ptr, _mm_unpacklo_epi16(ab, cd));
  _mm_storeu_si128((__m128i *)(ptr + 16), _mm_unpackhi_epi16(ab, cd));
}

static inline void jsimde_vst4_16(void *ptr, __m128i a, __m128i b, __m128i c,
                                  __m128i d)
{
  __m128i ab = _mm_unpacklo_epi16(a, b), cd = _mm_unpacklo_epi16(c, d);
  _mm_storeu_si128((__m128i *)ptr, _mm_unpacklo_epi32(ab, cd));
  _mm_storeu_si128((__m128i *)((char *)ptr + 16), _mm_unpackhi_epi32(ab, cd));
}

static inline simde_uint8x8x2_t jsimde_vld2_u8(const uint8_t *ptr)
{
  __m128i v = _mm_loadu_si128((const __m128i *)ptr);
  __m128i even = _mm_and_si128(v, _mm_set1_epi16(0xFF));
  __m128i odd = _mm_srli_epi16(v, 8);
  simde_uint8x8x2_t r;
  r.val[0] = JSIMDE_TO_D(simde_uint8x8_t, _mm_packus_epi16(even, even));
  r.val[1] = JSIMDE_TO_D(simde_uint8x8_t, _mm_packus_epi16(odd, odd));
  return r;
}

static inline simde_int16x4x3_t jsimde_vld1_s16_x3(const int16_t *ptr)
{
  simde_int16x4x3_t r;
  r.val[0] = vld1_s16(ptr);
  r.val[1] = vld1_s16(ptr + 4);
  r.val[2] = vld1_s16(ptr + 8);
  return r;
}

static inline __m128i jsimde_vshll_n_s16(__m128i a, int n)
{
  return _mm_sll_epi32(_mm_srai_epi32(_mm_unpacklo_epi16(a, a), 16),
                       _mm_cvtsi32_si128(n));
}

#undef vshll_n_s16
#define vshll_n_s16(a, n) \
  ((simde_int32x4_t)jsimde_vshll_n_s16(JSIMDE_D(a), (n)))
static inline simde_uint8x16x2_t jsimde_vtrnq_u8(__m128i a, __m128i b)
{
  const __m128i lo = _mm_set1_epi16(0x00FF);
  simde_uint8x16x2_t r;
  r.val[0] = (simde_uint8x16_t)_mm_or_si128(_mm_and_si128(a, lo),
                                            _mm_slli_epi16(b, 8));
  r.val[1] = (simde_uint8x16_t)_mm_or_si128(_mm_srli_epi16(a, 8),
                                            _mm_andnot_si128(lo, b));
  return r;
}

/* Returns { a0 b0 a2 b2 a4 b4 a6 b6 }, { a1 b1 a3 b3 a5 b5 a7 b7 } */
static inline void jsimde_vtrnq_16(__m128i a, __m128i b, __m128i *r0,
                                   __m128i *r1)
{
  __m128i t0 = _mm_shuffle_epi32(_mm_unpacklo_epi16(a, b),
                                 _MM_SHUFFLE(3, 1, 2, 0));
  __m128i t1 = _mm_shuffle_epi32(_mm_unpackhi_epi16(a, b),
                                 _MM_SHUFFLE(3, 1, 2, 0));
  *r0 = _mm_unpacklo_epi64(t0, t1);
  *r1 = _mm_unpackhi_epi64(t0, t1);
}

static inline simde_int16x8x2_t jsimde_vtrnq_s16(__m128i a, __m128i b)
{
  simde_int16x8x2_t r;
  __m128i r0, r1;
  jsimde_vtrnq_16(a, b, &r0, &r1);
  r.val[0] = (simde_int16x8_t)r0;
  r.val[1] = (simde_int16x8_t)r1;
  return r;
}

static inline simde_uint16x8x2_t jsimde_vtrnq_u16(__m128i a, __m128i b)
{
  simde_uint16x8x2_t r;
  __m128i r0, r1;
  jsimde_vtrnq_16(a, b, &r0, &r1);
  r.val[0] = (simde_uint16x8_t)r0;
  r.val[1] = (simde_uint16x8_t)r1;
  return r;
}

static inline simde_int32x4x2_t jsimde_vtrnq_s32(__m128i a, __m128i b)
{
  __m128i t0 = _mm_unpacklo_epi32(a, b), t1 = _mm_unpackhi_epi32(a, b);
  simde_int32x4x2_t r;
  r.val[0] = (simde_int32x4_t)_mm_unpacklo_epi64(t0, t1);
  r.val[1] = (simde_int32x4_t)_mm_unpackhi_epi64(t0, t1);
  return r;
}

static inline simde_int16x8x4_t jsimde_vld4q_s16(const int16_t *ptr)
{
  /* Each 16-byte vector holds two 4-element groups; transpose 16-bit lanes */
  __m128i v0 = _mm_loadu_si128((const __m128i *)ptr);
  __m128i v1 = _mm_loadu_si128((const __m128i *)(ptr + 8));
  __m128i v2 = _mm_loadu_si128((const __m128i *)(ptr + 16));
  __m128i v3 = _mm_loadu_si128((const __m128i *)(ptr + 24));
  /* a: x0 y0 z0 w0 x1 y1 z1 w1 ... -> group by element */
  __m128i t0 = _mm_unpacklo_epi16(v0, v1);  /* x0 x2 y0 y2 z0 z2 w0 w2 */
  __m128i t1 = _mm_unpackhi_epi16(v0, v1);  /* x1 x3 y1 y3 z1 z3 w1 w3 */
  __m128i t2 = _mm_unpacklo_epi16(v2, v3);  /* x4 x6 ... */
  __m128i t3 = _mm_unpackhi_epi16(v2, v3);  /* x5 x7 ... */
  __m128i u0 = _mm_unpacklo_epi16(t0, t1);  /* x0 x1 x2 x3 y0 y1 y2 y3 */
  __m128i u1 = _mm_unpackhi_epi16(t0, t1);  /* z0..z3 w0..w3 */
  __m128i u2 = _mm_unpacklo_epi16(t2, t3);  /* x4..x7 y4..y7 */
  __m128i u3 = _mm_unpackhi_epi16(t2, t3);  /* z4..z7 w4..w7 */
  simde_int16x8x4_t r;
  r.val[0] = (simde_int16x8_t)_mm_unpacklo_epi64(u0, u2);
  r.val[1] = (simde_int16x8_t)_mm_unpackhi_epi64(u0, u2);
  r.val[2] = (simde_int16x8_t)_mm_unpacklo_epi64(u1, u3);
  r.val[3] = (simde_int16x8_t)_mm_unpackhi_epi64(u1, u3);
  return r;
}

#define JSIMDE_LD1Q_X4(T, ptr) { { \
  (T)_mm_loadu_si128((const __m128i *)(ptr)), \
  (T)_mm_loadu_si128((const __m128i *)(ptr) + 1), \
  (T)_mm_loadu_si128((const __m128i *)(ptr) + 2), \
  (T)_mm_loadu_si128((const __m128i *)(ptr) + 3) } }

static inline simde_uint8x16x4_t jsimde_vld1q_u8_x4(const uint8_t *ptr)
{
  simde_uint8x16x4_t r = JSIMDE_LD1Q_X4(simde_uint8x16_t, ptr);
  return r;
}

static inline simde_int8x16x4_t jsimde_vld1q_s8_x4(const int8_t *ptr)
{
  simde_int8x16x4_t r = JSIMDE_LD1Q_X4(simde_int8x16_t, ptr);
  return r;
}

#if defined(__SSSE3__)
#include <tmmintrin.h>

/* Gather every fourth byte of four 16-byte vectors into four 16-byte vectors */
static inline void jsimde_deinterleave4(__m128i v0, __m128i v1, __m128i v2,
                                        __m128i v3, __m128i *r)
{
  const __m128i m = _mm_setr_epi8(0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14,
                                  3, 7, 11, 15);
  v0 = _mm_shuffle_epi8(v0, m);
  v1 = _mm_shuffle_epi8(v1, m);
  v2 = _mm_shuffle_epi8(v2, m);
  v3 = _mm_shuffle_epi8(v3, m);
  __m128i t0 = _mm_unpacklo_epi32(v0, v1), t1 = _mm_unpackhi_epi32(v0, v1);
  __m128i t2 = _mm_unpacklo_epi32(v2, v3), t3 = _mm_unpackhi_epi32(v2, v3);
  r[0] = _mm_unpacklo_epi64(t0, t2);
  r[1] = _mm_unpackhi_epi64(t0, t2);
  r[2] = _mm_unpacklo_epi64(t1, t3);
  r[3] = _mm_unpackhi_epi64(t1, t3);
}

static inline simde_uint8x16x4_t jsimde_vld4q_u8(const uint8_t *ptr)
{
  __m128i v[4];
  simde_uint8x16x4_t r;
  jsimde_deinterleave4(_mm_loadu_si128((const __m128i *)ptr),
                       _mm_loadu_si128((const __m128i *)ptr + 1),
                       _mm_loadu_si128((const __m128i *)ptr + 2),
                       _mm_loadu_si128((const __m128i *)ptr + 3), v);
  r.val[0] = (simde_uint8x16_t)v[0];
  r.val[1] = (simde_uint8x16_t)v[1];
  r.val[2] = (simde_uint8x16_t)v[2];
  r.val[3] = (simde_uint8x16_t)v[3];
  return r;
}

static inline simde_uint8x8x4_t jsimde_vld4_u8(const uint8_t *ptr)
{
  __m128i v[4];
  simde_uint8x8x4_t r;
  jsimde_deinterleave4(_mm_loadu_si128((const __m128i *)ptr),
                       _mm_loadu_si128((const __m128i *)ptr + 1),
                       _mm_setzero_si128(), _mm_setzero_si128(), v);
  r.val[0] = JSIMDE_TO_D(simde_uint8x8_t, v[0]);
  r.val[1] = JSIMDE_TO_D(simde_uint8x8_t, v[1]);
  r.val[2] = JSIMDE_TO_D(simde_uint8x8_t, v[2]);
  r.val[3] = JSIMDE_TO_D(simde_uint8x8_t, v[3]);
  return r;
}

/* jsimde_ld3_masks[c][k]: byte i of channel c is byte 3 * i + c of the
 * 48-byte input; mask k selects the bytes that lie in input vector k.
 */
static const int8_t jsimde_ld3_masks[3][3][16] = {
  {
    { 0, 3, 6, 9, 12, 15, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128 },
    { -128, -128, -128, -128, -128, -128, 2, 5, 8, 11, 14, -128, -128, -128, -128, -128 },
    { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 1, 4, 7, 10, 13 }
  },
  {
    { 1, 4, 7, 10, 13, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128 },
    { -128, -128, -128, -128, -128, 0, 3, 6, 9, 12, 15, -128, -128, -128, -128, -128 },
    { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 2, 5, 8, 11, 14 }
  },
  {
    { 2, 5, 8, 11, 14, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128 },
    { -128, -128, -128, -128, -128, 1, 4, 7, 10, 13, -128, -128, -128, -128, -128, -128 },
    { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, 0, 3, 6, 9, 12, 15 }
  }
};

#define JSIMDE_LD3_MASK(c, k) \
  _mm_loadu_si128((const __m128i *)jsimde_ld3_masks[c][k])

static inline __m128i jsimde_ld3_channel(__m128i v0, __m128i v1, __m128i v2,
                                         int c)
{
  return _mm_or_si128(_mm_or_si128(
    _mm_shuffle_epi8(v0, JSIMDE_LD3_MASK(c, 0)),
    _mm_shuffle_epi8(v1, JSIMDE_LD3_MASK(c, 1))),
    _mm_shuffle_epi8(v2, JSIMDE_LD3_MASK(c, 2)));
}

static inline simde_uint8x16x3_t jsimde_vld3q_u8(const uint8_t *ptr)
{
  __m128i v0 = _mm_loadu_si128((const __m128i *)ptr);
  __m128i v1 = _mm_loadu_si128((const __m128i *)ptr + 1);
  __m128i v2 = _mm_loadu_si128((const __m128i *)ptr + 2);
  simde_uint8x16x3_t r;
  r.val[0] = (simde_uint8x16_t)jsimde_ld3_channel(v0, v1, v2, 0);
  r.val[1] = (simde_uint8x16_t)jsimde_ld3_channel(v0, v1, v2, 1);
  r.val[2] = (simde_uint8x16_t)jsimde_ld3_channel(v0, v1, v2, 2);
  return r;
}

static inline simde_uint8x8x3_t jsimde_vld3_u8(const uint8_t *ptr)
{
  __m128i v0 = _mm_loadu_si128((const __m128i *)ptr);
  __m128i v1 = _mm_loadl_epi64((const __m128i *)(ptr + 16));
  __m128i z = _mm_setzero_si128();
  simde_uint8x8x3_t r;
  r.val[0] = JSIMDE_TO_D(simde_uint8x8_t, jsimde_ld3_channel(v0, v1, z, 0));
  r.val[1] = JSIMDE_TO_D(simde_uint8x8_t, jsimde_ld3_channel(v0, v1, z, 1));
  r.val[2] = JSIMDE_TO_D(simde_uint8x8_t, jsimde_ld3_channel(v0, v1, z, 2));
  return r;
}

/* Shift each lane left by the signed low byte of the corresponding lane of b,
 * or right if it is negative.  Shifts by 16 or more yield 0.  SSE has no
 * per-lane shift, so multiply by 2^n for left shifts and take the high half
 * of a * 2^(16-n) for right shifts by n.
 */
static inline __m128i jsimde_vshlq_u16(__m128i a, __m128i b)
{
  const __m128i pow2 = _mm_setr_epi8(1, 2, 4, 8, 16, 32, 64, -128,
                                     0, 0, 0, 0, 0, 0, 0, 0);
  __m128i n = _mm_srai_epi16(_mm_slli_epi16(b, 8), 8);
  __m128i neg = _mm_srai_epi16(n, 15);
  __m128i m = _mm_and_si128(_mm_add_epi16(n, _mm_and_si128(neg,
                                                           _mm_set1_epi16(16))),
                            _mm_set1_epi16(15));
  /* Byte 0 of each lane looks up 2^m, byte 1 looks up 2^(m - 8). */
  __m128i p = _mm_shuffle_epi8(pow2, _mm_or_si128(m,
    _mm_slli_epi16(_mm_xor_si128(m, _mm_set1_epi16(8)), 8)));
  __m128i r = _mm_or_si128(_mm_andnot_si128(neg, _mm_mullo_epi16(a, p)),
                           _mm_and_si128(neg, _mm_mulhi_epu16(a, p)));
  __m128i valid = _mm_and_si128(_mm_cmpgt_epi16(n, _mm_set1_epi16(-16)),
                                _mm_cmplt_epi16(n, _mm_set1_epi16(16)));
  return _mm_and_si128(r, valid);
}

/* Count the set bits of each byte with a nibble lookup. */
static inline __m128i jsimde_cnt_8(__m128i a)
{
  const __m128i lut = _mm_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3,
                                    1, 2, 2, 3, 2, 3, 3, 4);
  const __m128i lo = _mm_set1_epi8(0x0F);
  return _mm_add_epi8(_mm_shuffle_epi8(lut, _mm_and_si128(a, lo)),
                      _mm_shuffle_epi8(lut, _mm_and_si128(_mm_srli_epi16(a, 4),
                                                          lo)));
}

#undef vshlq_u16
#define vshlq_u16(a, b) \
  ((simde_uint16x8_t)jsimde_vshlq_u16(JSIMDE_Q(a), JSIMDE_Q(b)))
#undef vcnt_u8
#define vcnt_u8(a)  JSIMDE_TO_D(simde_uint8x8_t, jsimde_cnt_8(JSIMDE_D(a)))

#undef vld4q_u8
#define vld4q_u8(p)  jsimde_vld4q_u8(p)
#undef vld4_u8
#define vld4_u8(p)  jsimde_vld4_u8(p)
#undef vld3q_u8
#define vld3q_u8(p)  jsimde_vld3q_u8(p)
#undef vld3_u8
#define vld3_u8(p)  jsimde_vld3_u8(p)
#endif /* __SSSE3__ */

#undef vtrnq_u8
#define vtrnq_u8(a, b)  jsimde_vtrnq_u8(JSIMDE_Q(a), JSIMDE_Q(b))
#undef vtrnq_s16
#define vtrnq_s16(a, b)  jsimde_vtrnq_s16(JSIMDE_Q(a), JSIMDE_Q(b))
#undef vtrnq_u16
#define vtrnq_u16(a, b)  jsimde_vtrnq_u16(JSIMDE_Q(a), JSIMDE_Q(b))
#undef vtrnq_s32
#define vtrnq_s32(a, b)  jsimde_vtrnq_s32(JSIMDE_Q(a), JSIMDE_Q(b))
#undef vqdmulhq_lane_s16
#define vqdmulhq_lane_s16(a, v, lane) \
  ((simde_int16x8_t)jsimde_vqdmulh_16(JSIMDE_Q(a), JSIMDE_DUP16(v, lane)))
#undef vqdmulh_lane_s16
#define vqdmulh_lane_s16(a, v, lane) \
  JSIMDE_TO_D(simde_int16x4_t, \
              jsimde_vqdmulh_16(JSIMDE_D(a), JSIMDE_DUP16(v, lane)))
#undef vld4q_s16
#define vld4q_s16(p)  jsimde_vld4q_s16(p)
#undef vld1q_u8_x4
#define vld1q_u8_x4(p)  jsimde_vld1q_u8_x4(p)
#undef vld1q_s8_x4
#define vld1q_s8_x4(p)  jsimde_vld1q_s8_x4(p)
/* Saturating doubling multiply returning the high half:
 * min((2 * a * b) >> 16, 32767)
 */
static inline __m128i jsimde_vqdmulh_16(__m128i a, __m128i b)
{
  const __m128i min = _mm_set1_epi16(-32768);
  __m128i r = _mm_or_si128(_mm_slli_epi16(_mm_mulhi_epi16(a, b), 1),
                           _mm_srli_epi16(_mm_mullo_epi16(a, b), 15));
  /* Only -32768 * -32768 overflows; it yields 0x8000 instead of 0x7FFF. */
  __m128i ovf = _mm_and_si128(_mm_cmpeq_epi16(a, min), _mm_cmpeq_epi16(b, min));
  return _mm_xor_si128(r, ovf);
}

#define JSIMDE_DUP16(v, lane) \
  _mm_unpacklo_epi64(_mm_shufflelo_epi16(JSIMDE_D(v), (lane) * 0x55), \
                     _mm_shufflelo_epi16(JSIMDE_D(v), (lane) * 0x55))

#undef vst3q_u8
#define vst3q_u8(p, v) \
  jsimde_vst3q_u8((p), JSIMDE_Q((v).val[0]), JSIMDE_Q((v).val[1]), \
                  JSIMDE_Q((v).val[2]))
#undef vst3_u8
#define vst3_u8(p, v) \
  jsimde_vst3_u8((p), JSIMDE_D((v).val[0]), JSIMDE_D((v).val[1]), \
                 JSIMDE_D((v).val[2]))
#undef vst4q_u8
#define vst4q_u8(p, v) \
  jsimde_vst4q_u8((p), JSIMDE_Q((v).val[0]), JSIMDE_Q((v).val[1]), \
                  JSIMDE_Q((v).val[2]), JSIMDE_Q((v).val[3]))
#undef vst2q_u8
#define vst2q_u8(p, v)  do { \
  uint8_t *jsimde_p = (p); \
  __m128i jsimde_a = JSIMDE_Q((v).val[0]), jsimde_b = JSIMDE_Q((v).val[1]); \
  _mm_storeu_si128((__m128i *)jsimde_p, \
                   _mm_unpacklo_epi8(jsimde_a, jsimde_b)); \
  _mm_storeu_si128((__m128i *)(jsimde_p + 16), \
                   _mm_unpackhi_epi8(jsimde_a, jsimde_b)); \
} while (0)
#undef vst4_u8
#define vst4_u8(p, v) \
  jsimde_vst4_u8((p), JSIMDE_D((v).val[0]), JSIMDE_D((v).val[1]), \
                 JSIMDE_D((v).val[2]), JSIMDE_D((v).val[3]))
#undef vst4_s16
#define vst4_s16(p, v) \
  jsimde_vst4_16((int16_t *)(p), JSIMDE_D((v).val[0]), JSIMDE_D((v).val[1]), \
                 JSIMDE_D((v).val[2]), JSIMDE_D((v).val[3]))
#undef vst4_u16
#define vst4_u16(p, v) \
  jsimde_vst4_16((uint16_t *)(p), JSIMDE_D((v).val[0]), \
                 JSIMDE_D((v).val[1]), JSIMDE_D((v).val[2]), \
                 JSIMDE_D((v).val[3]))
#undef vst4_lane_u16
#define vst4_lane_u16(p, v, lane)  do { \
  uint16_t *jsimde_p = (p); \
  jsimde_p[0] = simde_vget_lane_u16((v).val[0], lane); \
  jsimde_p[1] = simde_vget_lane_u16((v).val[1], lane); \
  jsimde_p[2] = simde_vget_lane_u16((v).val[2], lane); \
  jsimde_p[3] = simde_vget_lane_u16((v).val[3], lane); \
} while (0)
#undef vst2_lane_u16
#define vst2_lane_u16(p, v, lane)  do { \
  uint16_t *jsimde_p = (p); \
  jsimde_p[0] = simde_vget_lane_u16((v).val[0], lane); \
  jsimde_p[1] = simde_vget_lane_u16((v).val[1], lane); \
} while (0)
/* Leading zeros of each 16-bit lane: a nonzero 16-bit value converts exactly
 * to float, whose biased exponent is 127 + floor(log2(x)).  Zero converts to
 * exponent 0, which the minimum clamps to 16.
 */
static inline __m128i jsimde_vclzq_16(__m128i a)
{
  const __m128i z = _mm_setzero_si128();
  __m128i elo = _mm_srli_epi32(_mm_castps_si128(
    _mm_cvtepi32_ps(_mm_unpacklo_epi16(a, z))), 23);
  __m128i ehi = _mm_srli_epi32(_mm_castps_si128(
    _mm_cvtepi32_ps(_mm_unpackhi_epi16(a, z))), 23);
  return _mm_min_epi16(_mm_sub_epi16(_mm_set1_epi16(127 + 15),
                                     _mm_packs_epi32(elo, ehi)),
                       _mm_set1_epi16(16));
}

/* Sum of the eight bytes, modulo 256 */
static inline uint8_t jsimde_vaddv_u8(__m128i a)
{
  return (uint8_t)_mm_cvtsi128_si32(
    _mm_sad_epu8(_mm_move_epi64(a), _mm_setzero_si128()));
}

#undef vclzq_s16
#define vclzq_s16(a)  ((simde_int16x8_t)jsimde_vclzq_16(JSIMDE_Q(a)))
#undef vclzq_u16
#define vclzq_u16(a)  ((simde_uint16x8_t)jsimde_vclzq_16(JSIMDE_Q(a)))
#undef vaddv_u8
#define vaddv_u8(a)  jsimde_vaddv_u8(JSIMDE_D(a))

#undef vld2_u8
#define vld2_u8(p)  jsimde_vld2_u8(p)
#undef vld1_s16_x3
#define vld1_s16_x3(p)  jsimde_vld1_s16_x3(p)

#endif

#endif /* JSIMD_SIMDE_ARM_NEON_H */
