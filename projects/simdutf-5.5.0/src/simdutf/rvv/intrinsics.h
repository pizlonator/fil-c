#ifndef SIMDUTF_RVV_INTRINSICS_H
#define SIMDUTF_RVV_INTRINSICS_H

#include "simdutf.h"

#include <riscv_vector.h>

#if __riscv_v_intrinsic >= 1000000 ||  __GCC__ >= 14
#define simdutf_vrgather_u8m1x2(tbl, idx) __riscv_vcreate_v_u8m1_u8m2( \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m2_u8m1(idx, 0), __riscv_vsetvlmax_e8m1()), \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m2_u8m1(idx, 1), __riscv_vsetvlmax_e8m1()));

#define simdutf_vrgather_u8m1x4(tbl, idx) __riscv_vcreate_v_u8m1_u8m4( \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m4_u8m1(idx, 0), __riscv_vsetvlmax_e8m1()), \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m4_u8m1(idx, 1), __riscv_vsetvlmax_e8m1()), \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m4_u8m1(idx, 2), __riscv_vsetvlmax_e8m1()), \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m4_u8m1(idx, 3), __riscv_vsetvlmax_e8m1()));
#else
// This has worse codegen on gcc
#define simdutf_vrgather_u8m1x2(tbl, idx) \
        __riscv_vset_v_u8m1_u8m2(__riscv_vlmul_ext_v_u8m1_u8m2( \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m2_u8m1(idx, 0), __riscv_vsetvlmax_e8m1())), 1, \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m2_u8m1(idx, 1), __riscv_vsetvlmax_e8m1()))

#define simdutf_vrgather_u8m1x4(tbl, idx) \
        __riscv_vset_v_u8m1_u8m4(__riscv_vset_v_u8m1_u8m4(\
        __riscv_vset_v_u8m1_u8m4(__riscv_vlmul_ext_v_u8m1_u8m4( \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m4_u8m1(idx, 0), __riscv_vsetvlmax_e8m1())), 1, \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m4_u8m1(idx, 1), __riscv_vsetvlmax_e8m1())), 2, \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m4_u8m1(idx, 2), __riscv_vsetvlmax_e8m1())), 3, \
        __riscv_vrgather_vv_u8m1(tbl, __riscv_vget_v_u8m4_u8m1(idx, 3), __riscv_vsetvlmax_e8m1()))
#endif

/* Zvbb adds dedicated support for endianness swaps with vrev8, but if we can't
 * use that, we have to emulate it with the standard V extension.
 * Using LMUL=1 vrgathers could be faster than the srl+macc variant, but that
 * would increase register pressure, and vrgather implementations performance
 * varies a lot. */
enum class simdutf_ByteFlip { NONE, V, ZVBB };

template<simdutf_ByteFlip method>
simdutf_really_inline static uint16_t simdutf_byteflip(uint16_t v) {
  if (method != simdutf_ByteFlip::NONE)
    return (uint16_t)((v*1u) << 8 | (v*1u) >> 8);
  return v;
}

#ifdef SIMDUTF_TARGET_ZVBB
SIMDUTF_UNTARGET_REGION
SIMDUTF_TARGET_ZVBB
#endif

template<simdutf_ByteFlip method>
simdutf_really_inline static vuint16m1_t simdutf_byteflip(vuint16m1_t v, size_t vl) {
#if SIMDUTF_HAS_ZVBB_INTRINSICS
  if (method == simdutf_ByteFlip::ZVBB)
    return __riscv_vrev8_v_u16m1(v, vl);
#endif
  if (method == simdutf_ByteFlip::V)
    return __riscv_vmacc_vx_u16m1(__riscv_vsrl_vx_u16m1(v, 8, vl), 0x100, v, vl);
  return v;
}

template<simdutf_ByteFlip method>
simdutf_really_inline static vuint16m2_t simdutf_byteflip(vuint16m2_t v, size_t vl) {
#if SIMDUTF_HAS_ZVBB_INTRINSICS
  if (method == simdutf_ByteFlip::ZVBB)
    return __riscv_vrev8_v_u16m2(v, vl);
#endif
  if (method == simdutf_ByteFlip::V)
    return __riscv_vmacc_vx_u16m2(__riscv_vsrl_vx_u16m2(v, 8, vl), 0x100, v, vl);
  return v;
}

template<simdutf_ByteFlip method>
simdutf_really_inline static vuint16m4_t simdutf_byteflip(vuint16m4_t v, size_t vl) {
#if SIMDUTF_HAS_ZVBB_INTRINSICS
  if (method == simdutf_ByteFlip::ZVBB)
    return __riscv_vrev8_v_u16m4(v, vl);
#endif
  if (method == simdutf_ByteFlip::V)
    return __riscv_vmacc_vx_u16m4(__riscv_vsrl_vx_u16m4(v, 8, vl), 0x100, v, vl);
  return v;
}

template<simdutf_ByteFlip method>
simdutf_really_inline static vuint16m8_t simdutf_byteflip(vuint16m8_t v, size_t vl) {
#if SIMDUTF_HAS_ZVBB_INTRINSICS
  if (method == simdutf_ByteFlip::ZVBB)
    return __riscv_vrev8_v_u16m8(v, vl);
#endif
  if (method == simdutf_ByteFlip::V)
    return __riscv_vmacc_vx_u16m8(__riscv_vsrl_vx_u16m8(v, 8, vl), 0x100, v, vl);
  return v;
}

#ifdef SIMDUTF_TARGET_ZVBB
SIMDUTF_UNTARGET_REGION
SIMDUTF_TARGET_RVV
#endif

#endif //  SIMDUTF_RVV_INTRINSICS_H
