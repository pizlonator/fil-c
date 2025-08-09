
simdutf_warn_unused size_t implementation::convert_latin1_to_utf8(const char *src, size_t len, char *dst) const noexcept {
  char *beg = dst;
  for (size_t vl, vlOut; len > 0; len -= vl, src += vl, dst += vlOut) {
    vl = __riscv_vsetvl_e8m2(len);
    vuint8m2_t v1 = __riscv_vle8_v_u8m2((uint8_t*)src, vl);
    vbool4_t nascii = __riscv_vmslt_vx_i8m2_b4(__riscv_vreinterpret_v_u8m2_i8m2(v1), 0, vl);
    size_t cnt = __riscv_vcpop_m_b4(nascii, vl);
    vlOut = vl + cnt;
    if (cnt == 0) {
      __riscv_vse8_v_u8m2((uint8_t*)dst, v1, vlOut);
      continue;
    }

    vuint8m2_t v0 = __riscv_vor_vx_u8m2(__riscv_vsrl_vx_u8m2(v1, 6, vl), 0b11000000, vl);
    v1 = __riscv_vand_vx_u8m2_mu(nascii, v1, v1, 0b10111111, vl);

    vuint8m4_t wide = __riscv_vreinterpret_v_u16m4_u8m4(__riscv_vwmaccu_vx_u16m4(__riscv_vwaddu_vv_u16m4(v0, v1, vl), 0xFF, v1, vl));
    vbool2_t mask = __riscv_vmsgtu_vx_u8m4_b2(__riscv_vsub_vx_u8m4(wide, 0b11000000, vl*2), 1, vl*2);
    vuint8m4_t comp = __riscv_vcompress_vm_u8m4(wide, mask, vl*2);

    __riscv_vse8_v_u8m4((uint8_t*)dst, comp, vlOut);
  }
  return dst - beg;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16le(const char *src, size_t len, char16_t *dst) const noexcept {
  char16_t *beg = dst;
  for (size_t vl; len > 0; len -= vl, src += vl, dst += vl) {
    vl = __riscv_vsetvl_e8m4(len);
    vuint8m4_t v = __riscv_vle8_v_u8m4((uint8_t*)src, vl);
    __riscv_vse16_v_u16m8((uint16_t*)dst, __riscv_vzext_vf2_u16m8(v, vl), vl);
  }
  return dst - beg;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16be(const char *src, size_t len, char16_t *dst) const noexcept {
  char16_t *beg = dst;
  for (size_t vl; len > 0; len -= vl, src += vl, dst += vl) {
    vl = __riscv_vsetvl_e8m4(len);
    vuint8m4_t v = __riscv_vle8_v_u8m4((uint8_t*)src, vl);
    __riscv_vse16_v_u16m8((uint16_t*)dst, __riscv_vsll_vx_u16m8(__riscv_vzext_vf2_u16m8(v, vl), 8, vl), vl);
  }
  return dst - beg;
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf32(const char *src, size_t len, char32_t *dst) const noexcept {
  char32_t *beg = dst;
  for (size_t vl; len > 0; len -= vl, src += vl, dst += vl) {
    vl = __riscv_vsetvl_e8m2(len);
    vuint8m2_t v = __riscv_vle8_v_u8m2((uint8_t*)src, vl);
    __riscv_vse32_v_u32m8((uint32_t*)dst, __riscv_vzext_vf4_u32m8(v, vl), vl);
  }
  return dst - beg;
}

