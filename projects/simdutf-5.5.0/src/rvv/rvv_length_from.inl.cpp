
simdutf_warn_unused size_t implementation::count_utf16le(const char16_t *src, size_t len) const noexcept {
  return utf32_length_from_utf16le(src, len);
}

simdutf_warn_unused size_t implementation::count_utf16be(const char16_t *src, size_t len) const noexcept {
  return utf32_length_from_utf16be(src, len);
}

simdutf_warn_unused size_t implementation::count_utf8(const char *src, size_t len) const noexcept {
  return utf32_length_from_utf8(src, len);
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf8(const char *src, size_t len) const noexcept {
  return utf32_length_from_utf8(src, len);
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf16(size_t len) const noexcept {
  return len;
}

simdutf_warn_unused size_t implementation::latin1_length_from_utf32(size_t len) const noexcept {
  return len;
}

simdutf_warn_unused size_t implementation::utf16_length_from_latin1(size_t len) const noexcept {
  return len;
}

simdutf_warn_unused size_t implementation::utf32_length_from_latin1(size_t len) const noexcept {
  return len;
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf8(const char *src, size_t len) const noexcept {
  size_t count = 0;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e8m8(len);
    vint8m8_t v = __riscv_vle8_v_i8m8((int8_t*)src, vl);
    vbool1_t mask = __riscv_vmsgt_vx_i8m8_b1(v, -65, vl);
    count += __riscv_vcpop_m_b1(mask, vl);
  }
  return count;
}

template<simdutf_ByteFlip bflip>
simdutf_really_inline static size_t rvv_utf32_length_from_utf16(const char16_t *src, size_t len) {
  size_t count = 0;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e16m8(len);
    vuint16m8_t v = __riscv_vle16_v_u16m8((uint16_t*)src, vl);
    v = simdutf_byteflip<bflip>(v, vl);
    vbool2_t notHigh = __riscv_vmor_mm_b2(
            __riscv_vmsgtu_vx_u16m8_b2(v, 0xDFFF, vl),
            __riscv_vmsltu_vx_u16m8_b2(v, 0xDC00, vl), vl);
    count += __riscv_vcpop_m_b2(notHigh, vl);
  }
  return count;
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf16le(const char16_t *src, size_t len) const noexcept {
  return rvv_utf32_length_from_utf16<simdutf_ByteFlip::NONE>(src, len);
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf16be(const char16_t *src, size_t len) const noexcept {
  if (supports_zvbb())
    return rvv_utf32_length_from_utf16<simdutf_ByteFlip::ZVBB>(src, len);
  else
    return rvv_utf32_length_from_utf16<simdutf_ByteFlip::V>(src, len);
}

simdutf_warn_unused size_t implementation::utf8_length_from_latin1(const char *src, size_t len) const noexcept {
  size_t count = len;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e8m8(len);
    vint8m8_t v = __riscv_vle8_v_i8m8((int8_t*)src, vl);
    count += __riscv_vcpop_m_b1(__riscv_vmslt_vx_i8m8_b1(v, 0, vl), vl);
  }
  return count;
}

template<simdutf_ByteFlip bflip>
simdutf_really_inline static size_t rvv_utf8_length_from_utf16(const char16_t *src, size_t len) {
  size_t count = 0;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e16m8(len);
    vuint16m8_t v = __riscv_vle16_v_u16m8((uint16_t*)src, vl);
    v = simdutf_byteflip<bflip>(v, vl);
    vbool2_t m234 = __riscv_vmsgtu_vx_u16m8_b2(v, 0x7F, vl);
    vbool2_t m34 = __riscv_vmsgtu_vx_u16m8_b2(v, 0x7FF, vl);
    vbool2_t notSur = __riscv_vmor_mm_b2(
            __riscv_vmsltu_vx_u16m8_b2(v, 0xD800, vl),
            __riscv_vmsgtu_vx_u16m8_b2(v, 0xDFFF, vl), vl);
    vbool2_t m3 = __riscv_vmand_mm_b2(m34, notSur, vl);
    count += vl + __riscv_vcpop_m_b2(m234, vl) + __riscv_vcpop_m_b2(m3, vl);
  }
  return count;
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf16le(const char16_t *src, size_t len) const noexcept {
  return rvv_utf8_length_from_utf16<simdutf_ByteFlip::NONE>(src, len);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf16be(const char16_t *src, size_t len) const noexcept {
  if (supports_zvbb())
    return rvv_utf8_length_from_utf16<simdutf_ByteFlip::ZVBB>(src, len);
  else
    return rvv_utf8_length_from_utf16<simdutf_ByteFlip::V>(src, len);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf32(const char32_t *src, size_t len) const noexcept {
  size_t count = 0;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e32m8(len);
    vuint32m8_t v = __riscv_vle32_v_u32m8((uint32_t*)src, vl);
    vbool4_t m234 = __riscv_vmsgtu_vx_u32m8_b4(v, 0x7F, vl);
    vbool4_t m34  = __riscv_vmsgtu_vx_u32m8_b4(v, 0x7FF, vl);
    vbool4_t m4   = __riscv_vmsgtu_vx_u32m8_b4(v, 0xFFFF, vl);
    count += vl + __riscv_vcpop_m_b4(m234, vl) + __riscv_vcpop_m_b4(m34, vl) + __riscv_vcpop_m_b4(m4, vl);
  }
  return count;
}

simdutf_warn_unused size_t implementation::utf16_length_from_utf8(const char *src, size_t len) const noexcept {
  size_t count = 0;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e8m8(len);
    vint8m8_t v = __riscv_vle8_v_i8m8((int8_t*)src, vl);
    vbool1_t m1234 = __riscv_vmsgt_vx_i8m8_b1(v, -65, vl);
    vbool1_t m4 = __riscv_vmsgtu_vx_u8m8_b1(
            __riscv_vreinterpret_u8m8(v), (uint8_t)0b11101111, vl);
    count += __riscv_vcpop_m_b1(m1234, vl) + __riscv_vcpop_m_b1(m4, vl);
  }
  return count;
}

simdutf_warn_unused size_t implementation::utf16_length_from_utf32(const char32_t *src, size_t len) const noexcept {
  size_t count = 0;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e32m8(len);
    vuint32m8_t v = __riscv_vle32_v_u32m8((uint32_t*)src, vl);
    vbool4_t m4 = __riscv_vmsgtu_vx_u32m8_b4(v, 0xFFFF, vl);
    count += vl + __riscv_vcpop_m_b4(m4, vl);
  }
  return count;
}

