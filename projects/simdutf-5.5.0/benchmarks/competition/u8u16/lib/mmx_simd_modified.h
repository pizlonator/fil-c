#ifndef simd_sub_2_xx
inline SIMD_type simd_sub_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(v1, v2);
}
#endif

#ifndef simd_sub_2_xl
inline SIMD_type simd_sub_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(v1, simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_sub_2_xh
inline SIMD_type simd_sub_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(v1, simd_srli_2(v2, 1));
}
#endif

#ifndef simd_sub_2_lx
inline SIMD_type simd_sub_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_sub_2_ll
inline SIMD_type simd_sub_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(simd_andc(v1, simd_himask_2), simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_sub_2_lh
inline SIMD_type simd_sub_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(simd_andc(v1, simd_himask_2), simd_srli_2(v2, 1));
}
#endif

#ifndef simd_sub_2_hx
inline SIMD_type simd_sub_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_sub_2_hl
inline SIMD_type simd_sub_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(simd_srli_2(v1, 1), simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_sub_2_hh
inline SIMD_type simd_sub_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_2(simd_srli_2(v1, 1), simd_srli_2(v2, 1));
}
#endif

#ifndef simd_sub_4_xx
inline SIMD_type simd_sub_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(v1, v2);
}
#endif

#ifndef simd_sub_4_xl
inline SIMD_type simd_sub_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(v1, simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_sub_4_xh
inline SIMD_type simd_sub_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(v1, simd_srli_4(v2, 2));
}
#endif

#ifndef simd_sub_4_lx
inline SIMD_type simd_sub_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_sub_4_ll
inline SIMD_type simd_sub_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(simd_andc(v1, simd_himask_4), simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_sub_4_lh
inline SIMD_type simd_sub_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(simd_andc(v1, simd_himask_4), simd_srli_4(v2, 2));
}
#endif

#ifndef simd_sub_4_hx
inline SIMD_type simd_sub_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_sub_4_hl
inline SIMD_type simd_sub_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(simd_srli_4(v1, 2), simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_sub_4_hh
inline SIMD_type simd_sub_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_4(simd_srli_4(v1, 2), simd_srli_4(v2, 2));
}
#endif

#ifndef simd_sub_8_xx
inline SIMD_type simd_sub_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(v1, v2);
}
#endif

#ifndef simd_sub_8_xl
inline SIMD_type simd_sub_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(v1, simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_sub_8_xh
inline SIMD_type simd_sub_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(v1, simd_srli_8(v2, 4));
}
#endif

#ifndef simd_sub_8_lx
inline SIMD_type simd_sub_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_sub_8_ll
inline SIMD_type simd_sub_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(simd_andc(v1, simd_himask_8), simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_sub_8_lh
inline SIMD_type simd_sub_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(simd_andc(v1, simd_himask_8), simd_srli_8(v2, 4));
}
#endif

#ifndef simd_sub_8_hx
inline SIMD_type simd_sub_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_sub_8_hl
inline SIMD_type simd_sub_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(simd_srli_8(v1, 4), simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_sub_8_hh
inline SIMD_type simd_sub_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_8(simd_srli_8(v1, 4), simd_srli_8(v2, 4));
}
#endif

#ifndef simd_sub_16_xx
inline SIMD_type simd_sub_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(v1, v2);
}
#endif

#ifndef simd_sub_16_xl
inline SIMD_type simd_sub_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(v1, simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_sub_16_xh
inline SIMD_type simd_sub_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sub_16_lx
inline SIMD_type simd_sub_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_sub_16_ll
inline SIMD_type simd_sub_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(simd_andc(v1, simd_himask_16), simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_sub_16_lh
inline SIMD_type simd_sub_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sub_16_hx
inline SIMD_type simd_sub_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_sub_16_hl
inline SIMD_type simd_sub_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(simd_srli_16(v1, 8), simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_sub_16_hh
inline SIMD_type simd_sub_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sub_32_xx
inline SIMD_type simd_sub_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(v1, v2);
}
#endif

#ifndef simd_sub_32_xl
inline SIMD_type simd_sub_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(v1, simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_sub_32_xh
inline SIMD_type simd_sub_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_sub_32_lx
inline SIMD_type simd_sub_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_sub_32_ll
inline SIMD_type simd_sub_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(simd_andc(v1, simd_himask_32), simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_sub_32_lh
inline SIMD_type simd_sub_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_sub_32_hx
inline SIMD_type simd_sub_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_sub_32_hl
inline SIMD_type simd_sub_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(simd_srli_32(v1, 16), simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_sub_32_hh
inline SIMD_type simd_sub_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sub_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_add_2_xx
inline SIMD_type simd_add_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_add_2(v1, v2);
}
#endif

#ifndef simd_add_2_xl
inline SIMD_type simd_add_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_add_2(v1, simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_add_2_xh
inline SIMD_type simd_add_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_add_2(v1, simd_srli_2(v2, 1));
}
#endif

#ifndef simd_add_2_lx
inline SIMD_type simd_add_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_add_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_add_2_ll
inline SIMD_type simd_add_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_andc(v1, simd_himask_2), simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_add_2_lh
inline SIMD_type simd_add_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_andc(v1, simd_himask_2), simd_srli_2(v2, 1));
}
#endif

#ifndef simd_add_2_hx
inline SIMD_type simd_add_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_add_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_add_2_hl
inline SIMD_type simd_add_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_srli_2(v1, 1), simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_add_2_hh
inline SIMD_type simd_add_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_srli_2(v1, 1), simd_srli_2(v2, 1));
}
#endif

#ifndef simd_add_4_xx
inline SIMD_type simd_add_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_add_4(v1, v2);
}
#endif

#ifndef simd_add_4_xl
inline SIMD_type simd_add_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_add_4(v1, simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_add_4_xh
inline SIMD_type simd_add_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_add_4(v1, simd_srli_4(v2, 2));
}
#endif

#ifndef simd_add_4_lx
inline SIMD_type simd_add_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_add_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_add_4_ll
inline SIMD_type simd_add_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_andc(v1, simd_himask_4), simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_add_4_lh
inline SIMD_type simd_add_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_andc(v1, simd_himask_4), simd_srli_4(v2, 2));
}
#endif

#ifndef simd_add_4_hx
inline SIMD_type simd_add_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_add_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_add_4_hl
inline SIMD_type simd_add_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_srli_4(v1, 2), simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_add_4_hh
inline SIMD_type simd_add_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_srli_4(v1, 2), simd_srli_4(v2, 2));
}
#endif

#ifndef simd_add_8_xx
inline SIMD_type simd_add_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(v1, v2);
}
#endif

#ifndef simd_add_8_xl
inline SIMD_type simd_add_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(v1, simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_add_8_xh
inline SIMD_type simd_add_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(v1, simd_srli_8(v2, 4));
}
#endif

#ifndef simd_add_8_lx
inline SIMD_type simd_add_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_add_8_ll
inline SIMD_type simd_add_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_andc(v1, simd_himask_8), simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_add_8_lh
inline SIMD_type simd_add_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_andc(v1, simd_himask_8), simd_srli_8(v2, 4));
}
#endif

#ifndef simd_add_8_hx
inline SIMD_type simd_add_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_add_8_hl
inline SIMD_type simd_add_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_srli_8(v1, 4), simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_add_8_hh
inline SIMD_type simd_add_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_add_8(simd_srli_8(v1, 4), simd_srli_8(v2, 4));
}
#endif

#ifndef simd_add_16_xx
inline SIMD_type simd_add_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(v1, v2);
}
#endif

#ifndef simd_add_16_xl
inline SIMD_type simd_add_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(v1, simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_add_16_xh
inline SIMD_type simd_add_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_add_16_lx
inline SIMD_type simd_add_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_add_16_ll
inline SIMD_type simd_add_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(simd_andc(v1, simd_himask_16), simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_add_16_lh
inline SIMD_type simd_add_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_add_16_hx
inline SIMD_type simd_add_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_add_16_hl
inline SIMD_type simd_add_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(simd_srli_16(v1, 8), simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_add_16_hh
inline SIMD_type simd_add_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_add_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_add_32_xx
inline SIMD_type simd_add_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(v1, v2);
}
#endif

#ifndef simd_add_32_xl
inline SIMD_type simd_add_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(v1, simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_add_32_xh
inline SIMD_type simd_add_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_add_32_lx
inline SIMD_type simd_add_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_add_32_ll
inline SIMD_type simd_add_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(simd_andc(v1, simd_himask_32), simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_add_32_lh
inline SIMD_type simd_add_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_add_32_hx
inline SIMD_type simd_add_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_add_32_hl
inline SIMD_type simd_add_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(simd_srli_32(v1, 16), simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_add_32_hh
inline SIMD_type simd_add_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_add_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_pack_2_xx
inline SIMD_type simd_pack_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(v1, v2);
}
#endif

#ifndef simd_pack_2_xl
inline SIMD_type simd_pack_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(v1, v2);
}
#endif

#ifndef simd_pack_2_xh
inline SIMD_type simd_pack_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(v1, simd_srli_16(v2, 1));
}
#endif

#ifndef simd_pack_2_lx
inline SIMD_type simd_pack_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(v1, v2);
}
#endif

#ifndef simd_pack_2_ll
inline SIMD_type simd_pack_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(v1, v2);
}
#endif

#ifndef simd_pack_2_lh
inline SIMD_type simd_pack_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(v1, simd_srli_16(v2, 1));
}
#endif

#ifndef simd_pack_2_hx
inline SIMD_type simd_pack_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(simd_srli_16(v1, 1), v2);
}
#endif

#ifndef simd_pack_2_hl
inline SIMD_type simd_pack_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(simd_srli_16(v1, 1), v2);
}
#endif

#ifndef simd_pack_2_hh
inline SIMD_type simd_pack_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_2(simd_srli_16(v1, 1), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_pack_4_xx
inline SIMD_type simd_pack_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(v1, v2);
}
#endif

#ifndef simd_pack_4_xl
inline SIMD_type simd_pack_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(v1, v2);
}
#endif

#ifndef simd_pack_4_xh
inline SIMD_type simd_pack_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(v1, simd_srli_16(v2, 2));
}
#endif

#ifndef simd_pack_4_lx
inline SIMD_type simd_pack_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(v1, v2);
}
#endif

#ifndef simd_pack_4_ll
inline SIMD_type simd_pack_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(v1, v2);
}
#endif

#ifndef simd_pack_4_lh
inline SIMD_type simd_pack_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(v1, simd_srli_16(v2, 2));
}
#endif

#ifndef simd_pack_4_hx
inline SIMD_type simd_pack_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(simd_srli_16(v1, 2), v2);
}
#endif

#ifndef simd_pack_4_hl
inline SIMD_type simd_pack_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(simd_srli_16(v1, 2), v2);
}
#endif

#ifndef simd_pack_4_hh
inline SIMD_type simd_pack_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_4(simd_srli_16(v1, 2), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_pack_8_xx
inline SIMD_type simd_pack_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(v1, v2);
}
#endif

#ifndef simd_pack_8_xl
inline SIMD_type simd_pack_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(v1, v2);
}
#endif

#ifndef simd_pack_8_xh
inline SIMD_type simd_pack_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(v1, simd_srli_16(v2, 4));
}
#endif

#ifndef simd_pack_8_lx
inline SIMD_type simd_pack_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(v1, v2);
}
#endif

#ifndef simd_pack_8_ll
inline SIMD_type simd_pack_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(v1, v2);
}
#endif

#ifndef simd_pack_8_lh
inline SIMD_type simd_pack_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(v1, simd_srli_16(v2, 4));
}
#endif

#ifndef simd_pack_8_hx
inline SIMD_type simd_pack_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(simd_srli_16(v1, 4), v2);
}
#endif

#ifndef simd_pack_8_hl
inline SIMD_type simd_pack_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(simd_srli_16(v1, 4), v2);
}
#endif

#ifndef simd_pack_8_hh
inline SIMD_type simd_pack_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_8(simd_srli_16(v1, 4), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_pack_16_xx
inline SIMD_type simd_pack_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(v1, v2);
}
#endif

#ifndef simd_pack_16_xl
inline SIMD_type simd_pack_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(v1, v2);
}
#endif

#ifndef simd_pack_16_xh
inline SIMD_type simd_pack_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_pack_16_lx
inline SIMD_type simd_pack_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(v1, v2);
}
#endif

#ifndef simd_pack_16_ll
inline SIMD_type simd_pack_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(v1, v2);
}
#endif

#ifndef simd_pack_16_lh
inline SIMD_type simd_pack_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_pack_16_hx
inline SIMD_type simd_pack_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_pack_16_hl
inline SIMD_type simd_pack_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_pack_16_hh
inline SIMD_type simd_pack_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_pack_32_xx
inline SIMD_type simd_pack_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(v1, v2);
}
#endif

#ifndef simd_pack_32_xl
inline SIMD_type simd_pack_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(v1, v2);
}
#endif

#ifndef simd_pack_32_xh
inline SIMD_type simd_pack_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_pack_32_lx
inline SIMD_type simd_pack_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(v1, v2);
}
#endif

#ifndef simd_pack_32_ll
inline SIMD_type simd_pack_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(v1, v2);
}
#endif

#ifndef simd_pack_32_lh
inline SIMD_type simd_pack_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_pack_32_hx
inline SIMD_type simd_pack_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_pack_32_hl
inline SIMD_type simd_pack_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_pack_32_hh
inline SIMD_type simd_pack_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_pack_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_mergeh_2_xx
inline SIMD_type simd_mergeh_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(v1, v2);
}
#endif

#ifndef simd_mergeh_2_xl
inline SIMD_type simd_mergeh_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(v1, simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_mergeh_2_xh
inline SIMD_type simd_mergeh_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(v1, simd_srli_2(v2, 1));
}
#endif

#ifndef simd_mergeh_2_lx
inline SIMD_type simd_mergeh_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_mergeh_2_ll
inline SIMD_type simd_mergeh_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(simd_andc(v1, simd_himask_2), simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_mergeh_2_lh
inline SIMD_type simd_mergeh_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(simd_andc(v1, simd_himask_2), simd_srli_2(v2, 1));
}
#endif

#ifndef simd_mergeh_2_hx
inline SIMD_type simd_mergeh_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_mergeh_2_hl
inline SIMD_type simd_mergeh_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(simd_srli_2(v1, 1), simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_mergeh_2_hh
inline SIMD_type simd_mergeh_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_2(simd_srli_2(v1, 1), simd_srli_2(v2, 1));
}
#endif

#ifndef simd_mergeh_4_xx
inline SIMD_type simd_mergeh_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(v1, v2);
}
#endif

#ifndef simd_mergeh_4_xl
inline SIMD_type simd_mergeh_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(v1, simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_mergeh_4_xh
inline SIMD_type simd_mergeh_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(v1, simd_srli_4(v2, 2));
}
#endif

#ifndef simd_mergeh_4_lx
inline SIMD_type simd_mergeh_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_mergeh_4_ll
inline SIMD_type simd_mergeh_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(simd_andc(v1, simd_himask_4), simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_mergeh_4_lh
inline SIMD_type simd_mergeh_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(simd_andc(v1, simd_himask_4), simd_srli_4(v2, 2));
}
#endif

#ifndef simd_mergeh_4_hx
inline SIMD_type simd_mergeh_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_mergeh_4_hl
inline SIMD_type simd_mergeh_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(simd_srli_4(v1, 2), simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_mergeh_4_hh
inline SIMD_type simd_mergeh_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_4(simd_srli_4(v1, 2), simd_srli_4(v2, 2));
}
#endif

#ifndef simd_mergeh_8_xx
inline SIMD_type simd_mergeh_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(v1, v2);
}
#endif

#ifndef simd_mergeh_8_xl
inline SIMD_type simd_mergeh_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(v1, simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_mergeh_8_xh
inline SIMD_type simd_mergeh_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(v1, simd_srli_8(v2, 4));
}
#endif

#ifndef simd_mergeh_8_lx
inline SIMD_type simd_mergeh_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_mergeh_8_ll
inline SIMD_type simd_mergeh_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(simd_andc(v1, simd_himask_8), simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_mergeh_8_lh
inline SIMD_type simd_mergeh_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(simd_andc(v1, simd_himask_8), simd_srli_8(v2, 4));
}
#endif

#ifndef simd_mergeh_8_hx
inline SIMD_type simd_mergeh_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_mergeh_8_hl
inline SIMD_type simd_mergeh_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(simd_srli_8(v1, 4), simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_mergeh_8_hh
inline SIMD_type simd_mergeh_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_8(simd_srli_8(v1, 4), simd_srli_8(v2, 4));
}
#endif

#ifndef simd_mergeh_16_xx
inline SIMD_type simd_mergeh_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(v1, v2);
}
#endif

#ifndef simd_mergeh_16_xl
inline SIMD_type simd_mergeh_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(v1, simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_mergeh_16_xh
inline SIMD_type simd_mergeh_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_mergeh_16_lx
inline SIMD_type simd_mergeh_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_mergeh_16_ll
inline SIMD_type simd_mergeh_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(simd_andc(v1, simd_himask_16), simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_mergeh_16_lh
inline SIMD_type simd_mergeh_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_mergeh_16_hx
inline SIMD_type simd_mergeh_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_mergeh_16_hl
inline SIMD_type simd_mergeh_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(simd_srli_16(v1, 8), simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_mergeh_16_hh
inline SIMD_type simd_mergeh_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_mergeh_32_xx
inline SIMD_type simd_mergeh_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(v1, v2);
}
#endif

#ifndef simd_mergeh_32_xl
inline SIMD_type simd_mergeh_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(v1, simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_mergeh_32_xh
inline SIMD_type simd_mergeh_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_mergeh_32_lx
inline SIMD_type simd_mergeh_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_mergeh_32_ll
inline SIMD_type simd_mergeh_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(simd_andc(v1, simd_himask_32), simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_mergeh_32_lh
inline SIMD_type simd_mergeh_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_mergeh_32_hx
inline SIMD_type simd_mergeh_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_mergeh_32_hl
inline SIMD_type simd_mergeh_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(simd_srli_32(v1, 16), simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_mergeh_32_hh
inline SIMD_type simd_mergeh_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergeh_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_mergel_2_xx
inline SIMD_type simd_mergel_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(v1, v2);
}
#endif

#ifndef simd_mergel_2_xl
inline SIMD_type simd_mergel_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(v1, simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_mergel_2_xh
inline SIMD_type simd_mergel_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(v1, simd_srli_2(v2, 1));
}
#endif

#ifndef simd_mergel_2_lx
inline SIMD_type simd_mergel_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_mergel_2_ll
inline SIMD_type simd_mergel_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(simd_andc(v1, simd_himask_2), simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_mergel_2_lh
inline SIMD_type simd_mergel_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(simd_andc(v1, simd_himask_2), simd_srli_2(v2, 1));
}
#endif

#ifndef simd_mergel_2_hx
inline SIMD_type simd_mergel_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_mergel_2_hl
inline SIMD_type simd_mergel_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(simd_srli_2(v1, 1), simd_andc(v2, simd_himask_2));
}
#endif

#ifndef simd_mergel_2_hh
inline SIMD_type simd_mergel_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_2(simd_srli_2(v1, 1), simd_srli_2(v2, 1));
}
#endif

#ifndef simd_mergel_4_xx
inline SIMD_type simd_mergel_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(v1, v2);
}
#endif

#ifndef simd_mergel_4_xl
inline SIMD_type simd_mergel_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(v1, simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_mergel_4_xh
inline SIMD_type simd_mergel_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(v1, simd_srli_4(v2, 2));
}
#endif

#ifndef simd_mergel_4_lx
inline SIMD_type simd_mergel_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_mergel_4_ll
inline SIMD_type simd_mergel_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(simd_andc(v1, simd_himask_4), simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_mergel_4_lh
inline SIMD_type simd_mergel_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(simd_andc(v1, simd_himask_4), simd_srli_4(v2, 2));
}
#endif

#ifndef simd_mergel_4_hx
inline SIMD_type simd_mergel_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_mergel_4_hl
inline SIMD_type simd_mergel_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(simd_srli_4(v1, 2), simd_andc(v2, simd_himask_4));
}
#endif

#ifndef simd_mergel_4_hh
inline SIMD_type simd_mergel_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_4(simd_srli_4(v1, 2), simd_srli_4(v2, 2));
}
#endif

#ifndef simd_mergel_8_xx
inline SIMD_type simd_mergel_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(v1, v2);
}
#endif

#ifndef simd_mergel_8_xl
inline SIMD_type simd_mergel_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(v1, simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_mergel_8_xh
inline SIMD_type simd_mergel_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(v1, simd_srli_8(v2, 4));
}
#endif

#ifndef simd_mergel_8_lx
inline SIMD_type simd_mergel_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_mergel_8_ll
inline SIMD_type simd_mergel_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(simd_andc(v1, simd_himask_8), simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_mergel_8_lh
inline SIMD_type simd_mergel_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(simd_andc(v1, simd_himask_8), simd_srli_8(v2, 4));
}
#endif

#ifndef simd_mergel_8_hx
inline SIMD_type simd_mergel_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_mergel_8_hl
inline SIMD_type simd_mergel_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(simd_srli_8(v1, 4), simd_andc(v2, simd_himask_8));
}
#endif

#ifndef simd_mergel_8_hh
inline SIMD_type simd_mergel_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_8(simd_srli_8(v1, 4), simd_srli_8(v2, 4));
}
#endif

#ifndef simd_mergel_16_xx
inline SIMD_type simd_mergel_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(v1, v2);
}
#endif

#ifndef simd_mergel_16_xl
inline SIMD_type simd_mergel_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(v1, simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_mergel_16_xh
inline SIMD_type simd_mergel_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_mergel_16_lx
inline SIMD_type simd_mergel_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_mergel_16_ll
inline SIMD_type simd_mergel_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(simd_andc(v1, simd_himask_16), simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_mergel_16_lh
inline SIMD_type simd_mergel_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_mergel_16_hx
inline SIMD_type simd_mergel_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_mergel_16_hl
inline SIMD_type simd_mergel_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(simd_srli_16(v1, 8), simd_andc(v2, simd_himask_16));
}
#endif

#ifndef simd_mergel_16_hh
inline SIMD_type simd_mergel_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_mergel_32_xx
inline SIMD_type simd_mergel_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(v1, v2);
}
#endif

#ifndef simd_mergel_32_xl
inline SIMD_type simd_mergel_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(v1, simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_mergel_32_xh
inline SIMD_type simd_mergel_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_mergel_32_lx
inline SIMD_type simd_mergel_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_mergel_32_ll
inline SIMD_type simd_mergel_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(simd_andc(v1, simd_himask_32), simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_mergel_32_lh
inline SIMD_type simd_mergel_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_mergel_32_hx
inline SIMD_type simd_mergel_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_mergel_32_hl
inline SIMD_type simd_mergel_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(simd_srli_32(v1, 16), simd_andc(v2, simd_himask_32));
}
#endif

#ifndef simd_mergel_32_hh
inline SIMD_type simd_mergel_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_mergel_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_sll_2_xx
inline SIMD_type simd_sll_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(v1, v2);
}
#endif

#ifndef simd_sll_2_xl
inline SIMD_type simd_sll_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(v1, v2);
}
#endif

#ifndef simd_sll_2_xh
inline SIMD_type simd_sll_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(v1, simd_srli_16(v2, 1));
}
#endif

#ifndef simd_sll_2_lx
inline SIMD_type simd_sll_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_sll_2_ll
inline SIMD_type simd_sll_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_sll_2_lh
inline SIMD_type simd_sll_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(simd_andc(v1, simd_himask_2), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_sll_2_hx
inline SIMD_type simd_sll_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_sll_2_hl
inline SIMD_type simd_sll_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_sll_2_hh
inline SIMD_type simd_sll_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_2(simd_srli_2(v1, 1), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_sll_4_xx
inline SIMD_type simd_sll_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(v1, v2);
}
#endif

#ifndef simd_sll_4_xl
inline SIMD_type simd_sll_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(v1, v2);
}
#endif

#ifndef simd_sll_4_xh
inline SIMD_type simd_sll_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(v1, simd_srli_16(v2, 2));
}
#endif

#ifndef simd_sll_4_lx
inline SIMD_type simd_sll_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_sll_4_ll
inline SIMD_type simd_sll_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_sll_4_lh
inline SIMD_type simd_sll_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(simd_andc(v1, simd_himask_4), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_sll_4_hx
inline SIMD_type simd_sll_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_sll_4_hl
inline SIMD_type simd_sll_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_sll_4_hh
inline SIMD_type simd_sll_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_4(simd_srli_4(v1, 2), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_sll_8_xx
inline SIMD_type simd_sll_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(v1, v2);
}
#endif

#ifndef simd_sll_8_xl
inline SIMD_type simd_sll_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(v1, v2);
}
#endif

#ifndef simd_sll_8_xh
inline SIMD_type simd_sll_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(v1, simd_srli_16(v2, 4));
}
#endif

#ifndef simd_sll_8_lx
inline SIMD_type simd_sll_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_sll_8_ll
inline SIMD_type simd_sll_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_sll_8_lh
inline SIMD_type simd_sll_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(simd_andc(v1, simd_himask_8), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_sll_8_hx
inline SIMD_type simd_sll_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_sll_8_hl
inline SIMD_type simd_sll_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_sll_8_hh
inline SIMD_type simd_sll_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_8(simd_srli_8(v1, 4), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_sll_16_xx
inline SIMD_type simd_sll_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(v1, v2);
}
#endif

#ifndef simd_sll_16_xl
inline SIMD_type simd_sll_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(v1, v2);
}
#endif

#ifndef simd_sll_16_xh
inline SIMD_type simd_sll_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sll_16_lx
inline SIMD_type simd_sll_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_sll_16_ll
inline SIMD_type simd_sll_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_sll_16_lh
inline SIMD_type simd_sll_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sll_16_hx
inline SIMD_type simd_sll_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_sll_16_hl
inline SIMD_type simd_sll_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_sll_16_hh
inline SIMD_type simd_sll_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sll_32_xx
inline SIMD_type simd_sll_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(v1, v2);
}
#endif

#ifndef simd_sll_32_xl
inline SIMD_type simd_sll_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(v1, v2);
}
#endif

#ifndef simd_sll_32_xh
inline SIMD_type simd_sll_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_sll_32_lx
inline SIMD_type simd_sll_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_sll_32_ll
inline SIMD_type simd_sll_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_sll_32_lh
inline SIMD_type simd_sll_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_sll_32_hx
inline SIMD_type simd_sll_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_sll_32_hl
inline SIMD_type simd_sll_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_sll_32_hh
inline SIMD_type simd_sll_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sll_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_srl_2_xx
inline SIMD_type simd_srl_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(v1, v2);
}
#endif

#ifndef simd_srl_2_xl
inline SIMD_type simd_srl_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(v1, v2);
}
#endif

#ifndef simd_srl_2_xh
inline SIMD_type simd_srl_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(v1, simd_srli_16(v2, 1));
}
#endif

#ifndef simd_srl_2_lx
inline SIMD_type simd_srl_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_srl_2_ll
inline SIMD_type simd_srl_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_srl_2_lh
inline SIMD_type simd_srl_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(simd_andc(v1, simd_himask_2), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_srl_2_hx
inline SIMD_type simd_srl_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_srl_2_hl
inline SIMD_type simd_srl_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_srl_2_hh
inline SIMD_type simd_srl_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_2(simd_srli_2(v1, 1), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_srl_4_xx
inline SIMD_type simd_srl_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(v1, v2);
}
#endif

#ifndef simd_srl_4_xl
inline SIMD_type simd_srl_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(v1, v2);
}
#endif

#ifndef simd_srl_4_xh
inline SIMD_type simd_srl_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(v1, simd_srli_16(v2, 2));
}
#endif

#ifndef simd_srl_4_lx
inline SIMD_type simd_srl_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_srl_4_ll
inline SIMD_type simd_srl_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_srl_4_lh
inline SIMD_type simd_srl_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(simd_andc(v1, simd_himask_4), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_srl_4_hx
inline SIMD_type simd_srl_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_srl_4_hl
inline SIMD_type simd_srl_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_srl_4_hh
inline SIMD_type simd_srl_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_4(simd_srli_4(v1, 2), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_srl_8_xx
inline SIMD_type simd_srl_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(v1, v2);
}
#endif

#ifndef simd_srl_8_xl
inline SIMD_type simd_srl_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(v1, v2);
}
#endif

#ifndef simd_srl_8_xh
inline SIMD_type simd_srl_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(v1, simd_srli_16(v2, 4));
}
#endif

#ifndef simd_srl_8_lx
inline SIMD_type simd_srl_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_srl_8_ll
inline SIMD_type simd_srl_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_srl_8_lh
inline SIMD_type simd_srl_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(simd_andc(v1, simd_himask_8), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_srl_8_hx
inline SIMD_type simd_srl_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_srl_8_hl
inline SIMD_type simd_srl_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_srl_8_hh
inline SIMD_type simd_srl_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_8(simd_srli_8(v1, 4), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_srl_16_xx
inline SIMD_type simd_srl_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(v1, v2);
}
#endif

#ifndef simd_srl_16_xl
inline SIMD_type simd_srl_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(v1, v2);
}
#endif

#ifndef simd_srl_16_xh
inline SIMD_type simd_srl_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_srl_16_lx
inline SIMD_type simd_srl_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_srl_16_ll
inline SIMD_type simd_srl_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_srl_16_lh
inline SIMD_type simd_srl_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_srl_16_hx
inline SIMD_type simd_srl_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_srl_16_hl
inline SIMD_type simd_srl_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_srl_16_hh
inline SIMD_type simd_srl_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_srl_32_xx
inline SIMD_type simd_srl_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(v1, v2);
}
#endif

#ifndef simd_srl_32_xl
inline SIMD_type simd_srl_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(v1, v2);
}
#endif

#ifndef simd_srl_32_xh
inline SIMD_type simd_srl_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_srl_32_lx
inline SIMD_type simd_srl_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_srl_32_ll
inline SIMD_type simd_srl_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_srl_32_lh
inline SIMD_type simd_srl_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_srl_32_hx
inline SIMD_type simd_srl_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_srl_32_hl
inline SIMD_type simd_srl_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_srl_32_hh
inline SIMD_type simd_srl_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_srl_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_sra_2_xx
inline SIMD_type simd_sra_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(v1, v2);
}
#endif

#ifndef simd_sra_2_xl
inline SIMD_type simd_sra_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(v1, v2);
}
#endif

#ifndef simd_sra_2_xh
inline SIMD_type simd_sra_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(v1, simd_srli_16(v2, 1));
}
#endif

#ifndef simd_sra_2_lx
inline SIMD_type simd_sra_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_sra_2_ll
inline SIMD_type simd_sra_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_sra_2_lh
inline SIMD_type simd_sra_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(simd_andc(v1, simd_himask_2), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_sra_2_hx
inline SIMD_type simd_sra_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_sra_2_hl
inline SIMD_type simd_sra_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_sra_2_hh
inline SIMD_type simd_sra_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_2(simd_srli_2(v1, 1), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_sra_4_xx
inline SIMD_type simd_sra_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(v1, v2);
}
#endif

#ifndef simd_sra_4_xl
inline SIMD_type simd_sra_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(v1, v2);
}
#endif

#ifndef simd_sra_4_xh
inline SIMD_type simd_sra_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(v1, simd_srli_16(v2, 2));
}
#endif

#ifndef simd_sra_4_lx
inline SIMD_type simd_sra_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_sra_4_ll
inline SIMD_type simd_sra_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_sra_4_lh
inline SIMD_type simd_sra_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(simd_andc(v1, simd_himask_4), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_sra_4_hx
inline SIMD_type simd_sra_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_sra_4_hl
inline SIMD_type simd_sra_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_sra_4_hh
inline SIMD_type simd_sra_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_4(simd_srli_4(v1, 2), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_sra_8_xx
inline SIMD_type simd_sra_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(v1, v2);
}
#endif

#ifndef simd_sra_8_xl
inline SIMD_type simd_sra_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(v1, v2);
}
#endif

#ifndef simd_sra_8_xh
inline SIMD_type simd_sra_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(v1, simd_srli_16(v2, 4));
}
#endif

#ifndef simd_sra_8_lx
inline SIMD_type simd_sra_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_sra_8_ll
inline SIMD_type simd_sra_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_sra_8_lh
inline SIMD_type simd_sra_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(simd_andc(v1, simd_himask_8), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_sra_8_hx
inline SIMD_type simd_sra_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_sra_8_hl
inline SIMD_type simd_sra_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_sra_8_hh
inline SIMD_type simd_sra_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_8(simd_srli_8(v1, 4), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_sra_16_xx
inline SIMD_type simd_sra_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(v1, v2);
}
#endif

#ifndef simd_sra_16_xl
inline SIMD_type simd_sra_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(v1, v2);
}
#endif

#ifndef simd_sra_16_xh
inline SIMD_type simd_sra_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sra_16_lx
inline SIMD_type simd_sra_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_sra_16_ll
inline SIMD_type simd_sra_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_sra_16_lh
inline SIMD_type simd_sra_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sra_16_hx
inline SIMD_type simd_sra_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_sra_16_hl
inline SIMD_type simd_sra_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_sra_16_hh
inline SIMD_type simd_sra_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_sra_32_xx
inline SIMD_type simd_sra_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(v1, v2);
}
#endif

#ifndef simd_sra_32_xl
inline SIMD_type simd_sra_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(v1, v2);
}
#endif

#ifndef simd_sra_32_xh
inline SIMD_type simd_sra_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_sra_32_lx
inline SIMD_type simd_sra_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_sra_32_ll
inline SIMD_type simd_sra_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_sra_32_lh
inline SIMD_type simd_sra_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_sra_32_hx
inline SIMD_type simd_sra_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_sra_32_hl
inline SIMD_type simd_sra_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_sra_32_hh
inline SIMD_type simd_sra_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_sra_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_rotl_2_xx
inline SIMD_type simd_rotl_2_xx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(v1, v2);
}
#endif

#ifndef simd_rotl_2_xl
inline SIMD_type simd_rotl_2_xl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(v1, v2);
}
#endif

#ifndef simd_rotl_2_xh
inline SIMD_type simd_rotl_2_xh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(v1, simd_srli_16(v2, 1));
}
#endif

#ifndef simd_rotl_2_lx
inline SIMD_type simd_rotl_2_lx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_rotl_2_ll
inline SIMD_type simd_rotl_2_ll(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(simd_andc(v1, simd_himask_2), v2);
}
#endif

#ifndef simd_rotl_2_lh
inline SIMD_type simd_rotl_2_lh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(simd_andc(v1, simd_himask_2), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_rotl_2_hx
inline SIMD_type simd_rotl_2_hx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_rotl_2_hl
inline SIMD_type simd_rotl_2_hl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(simd_srli_2(v1, 1), v2);
}
#endif

#ifndef simd_rotl_2_hh
inline SIMD_type simd_rotl_2_hh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_2(simd_srli_2(v1, 1), simd_srli_16(v2, 1));
}
#endif

#ifndef simd_rotl_4_xx
inline SIMD_type simd_rotl_4_xx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(v1, v2);
}
#endif

#ifndef simd_rotl_4_xl
inline SIMD_type simd_rotl_4_xl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(v1, v2);
}
#endif

#ifndef simd_rotl_4_xh
inline SIMD_type simd_rotl_4_xh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(v1, simd_srli_16(v2, 2));
}
#endif

#ifndef simd_rotl_4_lx
inline SIMD_type simd_rotl_4_lx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_rotl_4_ll
inline SIMD_type simd_rotl_4_ll(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(simd_andc(v1, simd_himask_4), v2);
}
#endif

#ifndef simd_rotl_4_lh
inline SIMD_type simd_rotl_4_lh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(simd_andc(v1, simd_himask_4), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_rotl_4_hx
inline SIMD_type simd_rotl_4_hx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_rotl_4_hl
inline SIMD_type simd_rotl_4_hl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(simd_srli_4(v1, 2), v2);
}
#endif

#ifndef simd_rotl_4_hh
inline SIMD_type simd_rotl_4_hh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_4(simd_srli_4(v1, 2), simd_srli_16(v2, 2));
}
#endif

#ifndef simd_rotl_8_xx
inline SIMD_type simd_rotl_8_xx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(v1, v2);
}
#endif

#ifndef simd_rotl_8_xl
inline SIMD_type simd_rotl_8_xl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(v1, v2);
}
#endif

#ifndef simd_rotl_8_xh
inline SIMD_type simd_rotl_8_xh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(v1, simd_srli_16(v2, 4));
}
#endif

#ifndef simd_rotl_8_lx
inline SIMD_type simd_rotl_8_lx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_rotl_8_ll
inline SIMD_type simd_rotl_8_ll(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(simd_andc(v1, simd_himask_8), v2);
}
#endif

#ifndef simd_rotl_8_lh
inline SIMD_type simd_rotl_8_lh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(simd_andc(v1, simd_himask_8), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_rotl_8_hx
inline SIMD_type simd_rotl_8_hx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_rotl_8_hl
inline SIMD_type simd_rotl_8_hl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(simd_srli_8(v1, 4), v2);
}
#endif

#ifndef simd_rotl_8_hh
inline SIMD_type simd_rotl_8_hh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_8(simd_srli_8(v1, 4), simd_srli_16(v2, 4));
}
#endif

#ifndef simd_rotl_16_xx
inline SIMD_type simd_rotl_16_xx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(v1, v2);
}
#endif

#ifndef simd_rotl_16_xl
inline SIMD_type simd_rotl_16_xl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(v1, v2);
}
#endif

#ifndef simd_rotl_16_xh
inline SIMD_type simd_rotl_16_xh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(v1, simd_srli_16(v2, 8));
}
#endif

#ifndef simd_rotl_16_lx
inline SIMD_type simd_rotl_16_lx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_rotl_16_ll
inline SIMD_type simd_rotl_16_ll(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(simd_andc(v1, simd_himask_16), v2);
}
#endif

#ifndef simd_rotl_16_lh
inline SIMD_type simd_rotl_16_lh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(simd_andc(v1, simd_himask_16), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_rotl_16_hx
inline SIMD_type simd_rotl_16_hx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_rotl_16_hl
inline SIMD_type simd_rotl_16_hl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(simd_srli_16(v1, 8), v2);
}
#endif

#ifndef simd_rotl_16_hh
inline SIMD_type simd_rotl_16_hh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_16(simd_srli_16(v1, 8), simd_srli_16(v2, 8));
}
#endif

#ifndef simd_rotl_32_xx
inline SIMD_type simd_rotl_32_xx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(v1, v2);
}
#endif

#ifndef simd_rotl_32_xl
inline SIMD_type simd_rotl_32_xl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(v1, v2);
}
#endif

#ifndef simd_rotl_32_xh
inline SIMD_type simd_rotl_32_xh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(v1, simd_srli_32(v2, 16));
}
#endif

#ifndef simd_rotl_32_lx
inline SIMD_type simd_rotl_32_lx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_rotl_32_ll
inline SIMD_type simd_rotl_32_ll(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(simd_andc(v1, simd_himask_32), v2);
}
#endif

#ifndef simd_rotl_32_lh
inline SIMD_type simd_rotl_32_lh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(simd_andc(v1, simd_himask_32), simd_srli_32(v2, 16));
}
#endif

#ifndef simd_rotl_32_hx
inline SIMD_type simd_rotl_32_hx(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_rotl_32_hl
inline SIMD_type simd_rotl_32_hl(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(simd_srli_32(v1, 16), v2);
}
#endif

#ifndef simd_rotl_32_hh
inline SIMD_type simd_rotl_32_hh(SIMD_type v1, SIMD_type v2) {
  return simd_rotl_32(simd_srli_32(v1, 16), simd_srli_32(v2, 16));
}
#endif

