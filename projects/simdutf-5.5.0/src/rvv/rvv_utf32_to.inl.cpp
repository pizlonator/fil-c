
simdutf_warn_unused size_t implementation::convert_utf32_to_latin1(const char32_t *src, size_t len, char *dst) const noexcept {
  result res = convert_utf32_to_latin1_with_errors(src, len, dst);
  return res.error == error_code::SUCCESS ? res.count : 0;
}

simdutf_warn_unused result implementation::convert_utf32_to_latin1_with_errors(const char32_t *src, size_t len, char *dst) const noexcept {
  const char32_t *const beg = src;
  for (size_t vl; len > 0; len -= vl, src += vl, dst += vl) {
    vl = __riscv_vsetvl_e32m8(len);
    vuint32m8_t v = __riscv_vle32_v_u32m8((uint32_t*)src, vl);
    long idx = __riscv_vfirst_m_b4(__riscv_vmsgtu_vx_u32m8_b4(v, 255, vl), vl);
    if (idx >= 0)
      return result(error_code::TOO_LARGE, src - beg + idx);
      /* We don't use vcompress here, because its performance varies widely on current platforms.
       * This might be worth reconsidering once there is more hardware available. */
    __riscv_vse8_v_u8m2((uint8_t*)dst, __riscv_vncvt_x_x_w_u8m2(__riscv_vncvt_x_x_w_u16m4(v, vl), vl), vl);
  }
  return result(error_code::SUCCESS, src - beg);
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_latin1(const char32_t *src, size_t len, char *dst) const noexcept {
  return convert_utf32_to_latin1(src, len, dst);
}

simdutf_warn_unused result implementation::convert_utf32_to_utf8_with_errors(const char32_t *src, size_t len, char *dst) const noexcept {
  size_t n = len;
  const char32_t *srcBeg = src;
  const char *dstBeg = dst;
  size_t vl8m4 = __riscv_vsetvlmax_e8m4();
  vbool2_t m4mulp2 = __riscv_vmseq_vx_u8m4_b2(__riscv_vand_vx_u8m4(__riscv_vid_v_u8m4(vl8m4), 3, vl8m4), 2, vl8m4);

  for (size_t vl, vlOut; n > 0; ) {
    vl = __riscv_vsetvl_e32m4(n);

    vuint32m4_t v = __riscv_vle32_v_u32m4((uint32_t const*)src, vl);
    vbool8_t m234 = __riscv_vmsgtu_vx_u32m4_b8(v, 0x80-1, vl);
    vuint16m2_t vn = __riscv_vncvt_x_x_w_u16m2(v, vl);

    if (__riscv_vfirst_m_b8(m234, vl) < 0) { /* 1 byte utf8 */
      vlOut = vl;
      __riscv_vse8_v_u8m1((uint8_t*)dst, __riscv_vncvt_x_x_w_u8m1(vn, vlOut), vlOut);
      n -= vl, src += vl, dst += vlOut;
      continue;
    }

    vbool8_t m34  = __riscv_vmsgtu_vx_u32m4_b8(v, 0x800-1, vl);

    if (__riscv_vfirst_m_b8(m34,vl) < 0) { /* 1/2 byte utf8 */
      /* 0: [     aaa|aabbbbbb]
       * 1: [aabbbbbb|        ] vsll 8
       * 2: [        |   aaaaa] vsrl 6
       * 3: [00111111|00111111]
       * 4: [  bbbbbb|000aaaaa] (1|2)&3
       * 5: [10000000|11000000]
       * 6: [10bbbbbb|110aaaaa] 4|5 */
      vuint16m2_t twoByte  =
        __riscv_vand_vx_u16m2(__riscv_vor_vv_u16m2(
          __riscv_vsll_vx_u16m2(vn, 8, vl),
          __riscv_vsrl_vx_u16m2(vn, 6, vl),
        vl), 0b0011111100111111, vl);
      vuint16m2_t vout16 = __riscv_vor_vx_u16m2_mu(m234, vn, twoByte, 0b1000000011000000, vl);
      vuint8m2_t vout = __riscv_vreinterpret_v_u16m2_u8m2(vout16);

      /* Every high byte that is zero should be compressed
       * low bytes should never be compressed, so we set them
       * to all ones, and then create a non-zero bytes mask */
      vbool4_t mcomp = __riscv_vmsne_vx_u8m2_b4(__riscv_vreinterpret_v_u16m2_u8m2(__riscv_vor_vx_u16m2(vout16, 0xFF, vl)), 0, vl*2);
      vlOut = __riscv_vcpop_m_b4(mcomp, vl*2);

      vout = __riscv_vcompress_vm_u8m2(vout, mcomp, vl*2);
      __riscv_vse8_v_u8m2((uint8_t*)dst, vout, vlOut);

      n -= vl, src += vl, dst += vlOut;
      continue;
    }
    long idx1 = __riscv_vfirst_m_b8(__riscv_vmsgtu_vx_u32m4_b8(v, 0x10FFFF, vl), vl);
    vbool8_t sur = __riscv_vmseq_vx_u32m4_b8(__riscv_vand_vx_u32m4(v, 0xFFFFF800, vl), 0xD800, vl);
    long idx2 = __riscv_vfirst_m_b8(sur, vl);
    if(idx1 >= 0 && idx2 >= 0) {
      if(idx1 <= idx2) { return result(error_code::TOO_LARGE, src - srcBeg + idx1); }
      else { return result(error_code::SURROGATE, src - srcBeg + idx2); }
    }
    if (idx1 >= 0) { return result(error_code::TOO_LARGE, src - srcBeg + idx1); }
    if (idx2 >= 0) { return result(error_code::SURROGATE, src - srcBeg + idx2); }

    vbool8_t m4 = __riscv_vmsgtu_vx_u32m4_b8(v, 0x10000-1, vl);
    long first = __riscv_vfirst_m_b8(m4, vl);
    size_t tail = vl - first;
    vl = first < 0 ? vl : first;

    if (vl > 0) { /* 1/2/3 byte utf8 */
      /* vn: [aaaabbbb|bbcccccc]
       * v1: [0bcccccc|        ] vsll  8
       * v1: [10cccccc|        ] vsll  8 & 0b00111111 | 0b10000000
       * v2: [        |110bbbbb] vsrl  6 & 0b00111111 | 0b11000000
       * v2: [        |10bbbbbb] vsrl  6 & 0b00111111 | 0b10000000
       * v3: [        |1110aaaa] vsrl 12 | 0b11100000
       *  1: [00000000|0bcccccc|00000000|00000000] => [0bcccccc]
       *  2: [00000000|10cccccc|110bbbbb|00000000] => [110bbbbb] [10cccccc]
       *  3: [00000000|10cccccc|10bbbbbb|1110aaaa] => [1110aaaa] [10bbbbbb] [10cccccc]
       */
      vuint16m2_t v1, v2, v3, v12;
      v1 = __riscv_vor_vx_u16m2_mu(m234, vn, __riscv_vand_vx_u16m2(vn, 0b00111111, vl), 0b10000000, vl);
      v1 = __riscv_vsll_vx_u16m2(v1, 8, vl);

      v2 = __riscv_vor_vx_u16m2(__riscv_vand_vx_u16m2(__riscv_vsrl_vx_u16m2(vn, 6, vl), 0b00111111, vl), 0b10000000, vl);
      v2 = __riscv_vor_vx_u16m2_mu(__riscv_vmnot_m_b8(m34,vl), v2, v2, 0b01000000, vl);
      v3 = __riscv_vor_vx_u16m2(__riscv_vsrl_vx_u16m2(vn, 12, vl), 0b11100000, vl);
      v12 = __riscv_vor_vv_u16m2_mu(m234, v1, v1, v2, vl);

      vuint32m4_t w12 = __riscv_vwmulu_vx_u32m4(v12, 1<<8, vl);
      vuint32m4_t w123 = __riscv_vwaddu_wv_u32m4_mu(m34, w12, w12, v3, vl);
      vuint8m4_t vout = __riscv_vreinterpret_v_u32m4_u8m4(w123);

      vbool2_t mcomp = __riscv_vmor_mm_b2(m4mulp2, __riscv_vmsne_vx_u8m4_b2(vout, 0, vl*4), vl*4);
      vlOut = __riscv_vcpop_m_b2(mcomp, vl*4);

      vout = __riscv_vcompress_vm_u8m4(vout, mcomp, vl*4);
      __riscv_vse8_v_u8m4((uint8_t*)dst, vout, vlOut);

      n -= vl, src += vl, dst += vlOut;
    }

    if (tail) while (n) {
      uint32_t word = src[0];
      if (word < 0x10000) break;
      if (word > 0x10FFFF) return result(error_code::TOO_LARGE, src - srcBeg);
      *dst++ = (uint8_t)(( word>>18)             | 0b11110000);
      *dst++ = (uint8_t)(((word>>12) & 0b111111) | 0b10000000);
      *dst++ = (uint8_t)(((word>> 6) & 0b111111) | 0b10000000);
      *dst++ = (uint8_t)(( word      & 0b111111) | 0b10000000);
      ++src;
      --n;
    }
  }

  return result(error_code::SUCCESS, dst - dstBeg);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf8(const char32_t *src, size_t len, char *dst) const noexcept {
  result res = convert_utf32_to_utf8_with_errors(src, len, dst);
  return res.error == error_code::SUCCESS ? res.count : 0;
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf8(const char32_t *src, size_t len, char *dst) const noexcept {
  return convert_utf32_to_utf8(src, len, dst);
}

template<simdutf_ByteFlip bflip>
simdutf_really_inline static result rvv_convert_utf32_to_utf16_with_errors(const char32_t *src, size_t len, char16_t *dst) {
  size_t vl8m2 = __riscv_vsetvlmax_e8m2();
  vbool4_t m4even = __riscv_vmseq_vx_u8m2_b4(__riscv_vand_vx_u8m2(__riscv_vid_v_u8m2(vl8m2), 1, vl8m2), 0, vl8m2);
  const char16_t *dstBeg = dst;
  const char32_t *srcBeg = src;
  for (size_t vl, vlOut; len > 0; len -= vl, src += vl, dst += vlOut) {
    vl = __riscv_vsetvl_e32m4(len);
    vuint32m4_t v = __riscv_vle32_v_u32m4((uint32_t*)src, vl);
    vuint32m4_t off = __riscv_vadd_vx_u32m4(v, 0xFFFF2000, vl);
    long idx1 = __riscv_vfirst_m_b8(__riscv_vmsgtu_vx_u32m4_b8(v, 0x10FFFF, vl), vl);
    long idx2 = __riscv_vfirst_m_b8(__riscv_vmsgtu_vx_u32m4_b8(off, 0xFFFFF7FF, vl), vl);
    if (idx1 >= 0 && idx2 >= 0) {
      if (idx1 <= idx2) return result(error_code::TOO_LARGE, src - srcBeg + idx1);
      return result(error_code::SURROGATE, src - srcBeg + idx2);
    }
    if (idx1 >= 0) return result(error_code::TOO_LARGE, src - srcBeg + idx1);
    if (idx2 >= 0) return result(error_code::SURROGATE, src - srcBeg + idx2);
    long idx = __riscv_vfirst_m_b8(__riscv_vmsgtu_vx_u32m4_b8(v, 0xFFFF, vl), vl);
    if (idx < 0) {
      vlOut = vl;
      vuint16m2_t n = simdutf_byteflip<bflip>(__riscv_vncvt_x_x_w_u16m2(v, vlOut), vlOut);
      __riscv_vse16_v_u16m2((uint16_t*)dst, n, vlOut);
      continue;
    }
    vlOut = rvv_utf32_store_utf16_m4<bflip>((uint16_t*)dst, v, vl, m4even);
  }
  return result(error_code::SUCCESS, dst - dstBeg);
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf16le(const char32_t *src, size_t len, char16_t *dst) const noexcept {
  result res = convert_utf32_to_utf16le_with_errors(src, len, dst);
  return res.error == error_code::SUCCESS ? res.count : 0;
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf16be(const char32_t *src, size_t len, char16_t *dst) const noexcept {
  result res = convert_utf32_to_utf16be_with_errors(src, len, dst);
  return res.error == error_code::SUCCESS ? res.count : 0;
}

simdutf_warn_unused result implementation::convert_utf32_to_utf16le_with_errors(const char32_t *src, size_t len, char16_t *dst) const noexcept {
  return rvv_convert_utf32_to_utf16_with_errors<simdutf_ByteFlip::NONE>(src, len, dst);
}

simdutf_warn_unused result implementation::convert_utf32_to_utf16be_with_errors(const char32_t *src, size_t len, char16_t *dst) const noexcept {
  if (supports_zvbb())
    return rvv_convert_utf32_to_utf16_with_errors<simdutf_ByteFlip::ZVBB>(src, len, dst);
  else
    return rvv_convert_utf32_to_utf16_with_errors<simdutf_ByteFlip::V>(src, len, dst);
}

template<simdutf_ByteFlip bflip>
simdutf_really_inline static size_t rvv_convert_valid_utf32_to_utf16(const char32_t *src, size_t len, char16_t *dst) {
  size_t vl8m2 = __riscv_vsetvlmax_e8m2();
  vbool4_t m4even = __riscv_vmseq_vx_u8m2_b4(__riscv_vand_vx_u8m2(__riscv_vid_v_u8m2(vl8m2), 1, vl8m2), 0, vl8m2);
  char16_t *dstBeg = dst;
  for (size_t vl, vlOut; len > 0; len -= vl, src += vl, dst += vlOut) {
    vl = __riscv_vsetvl_e32m4(len);
    vuint32m4_t v = __riscv_vle32_v_u32m4((uint32_t*)src, vl);
    if (__riscv_vfirst_m_b8(__riscv_vmsgtu_vx_u32m4_b8(v, 0xFFFF, vl), vl) < 0) {
      vlOut = vl;
      vuint16m2_t n = simdutf_byteflip<bflip>(__riscv_vncvt_x_x_w_u16m2(v, vlOut), vlOut);
      __riscv_vse16_v_u16m2((uint16_t*)dst, n, vlOut);
      continue;
    }
    vlOut = rvv_utf32_store_utf16_m4<bflip>((uint16_t*)dst, v, vl, m4even);
  }
  return dst - dstBeg;
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf16le(const char32_t *src, size_t len, char16_t *dst) const noexcept {
  return rvv_convert_valid_utf32_to_utf16<simdutf_ByteFlip::NONE>(src, len, dst);
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf16be(const char32_t *src, size_t len, char16_t *dst) const noexcept {
  if (supports_zvbb())
    return rvv_convert_valid_utf32_to_utf16<simdutf_ByteFlip::ZVBB>(src, len, dst);
  else
    return rvv_convert_valid_utf32_to_utf16<simdutf_ByteFlip::V>(src, len, dst);
}
