

simdutf_warn_unused bool implementation::validate_ascii(const char *src, size_t len) const noexcept {
  size_t vlmax = __riscv_vsetvlmax_e8m8();
  vint8m8_t mask = __riscv_vmv_v_x_i8m8(0, vlmax);
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e8m8(len);
    vint8m8_t v = __riscv_vle8_v_i8m8((int8_t*)src, vl);
    mask = __riscv_vor_vv_i8m8_tu(mask, mask, v, vl);
  }
  return __riscv_vfirst_m_b1(__riscv_vmslt_vx_i8m8_b1(mask, 0, vlmax), vlmax) < 0;
}

simdutf_warn_unused result implementation::validate_ascii_with_errors(const char *src, size_t len) const noexcept {
  const char *beg = src;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e8m8(len);
    vint8m8_t v = __riscv_vle8_v_i8m8((int8_t*)src, vl);
    long idx = __riscv_vfirst_m_b1(__riscv_vmslt_vx_i8m8_b1(v, 0, vl), vl);
    if (idx >= 0) return result(error_code::TOO_LARGE, src - beg + idx);
  }
  return result(error_code::SUCCESS, src - beg);
}

/* Returns a close estimation of the number of valid UTF-8 bytes up to the
 * first invalid one, but never overestimating. */
simdutf_really_inline static size_t rvv_count_valid_utf8(const char *src, size_t len) {
  const char *beg = src;
  if (len < 32) return 0;

  /* validate first three bytes */
  {
    size_t idx = 3;
    while (idx < len && (src[idx] >> 6) == 0b10)
      ++idx;
    if (idx > 3+3 || !scalar::utf8::validate(src, idx))
      return 0;
  }

  static const uint64_t err1m[] = { 0x0202020202020202, 0x4915012180808080 };
  static const uint64_t err2m[] = { 0xCBCBCB8B8383A3E7, 0xCBCBDBCBCBCBCBCB };
  static const uint64_t err3m[] = { 0x0101010101010101, 0X01010101BABAAEE6 };

  const vuint8m1_t err1tbl = __riscv_vreinterpret_v_u64m1_u8m1(__riscv_vle64_v_u64m1(err1m, 2));
  const vuint8m1_t err2tbl = __riscv_vreinterpret_v_u64m1_u8m1(__riscv_vle64_v_u64m1(err2m, 2));
  const vuint8m1_t err3tbl = __riscv_vreinterpret_v_u64m1_u8m1(__riscv_vle64_v_u64m1(err3m, 2));

  size_t tail = 3;
  size_t n = len - tail;

  for (size_t vl; n > 0; n -= vl, src += vl) {
    vl = __riscv_vsetvl_e8m4(n);
    vuint8m4_t v0 = __riscv_vle8_v_u8m4((uint8_t const*)src, vl);

    uint8_t next0 = src[vl+0];
    uint8_t next1 = src[vl+1];
    uint8_t next2 = src[vl+2];

    /* fast path: ASCII */
    if (__riscv_vfirst_m_b2(__riscv_vmsgtu_vx_u8m4_b2(v0, 0b01111111, vl), vl) < 0 && (next0|next1|next2) < 0b10000000)
      continue;

    /* see "Validating UTF-8 In Less Than One Instruction Per Byte"
     * https://arxiv.org/abs/2010.03090 */
    vuint8m4_t v1 = __riscv_vslide1down_vx_u8m4(v0, next0, vl);
    vuint8m4_t v2 = __riscv_vslide1down_vx_u8m4(v1, next1, vl);
    vuint8m4_t v3 = __riscv_vslide1down_vx_u8m4(v2, next2, vl);

    vuint8m4_t s1 = __riscv_vreinterpret_v_u16m4_u8m4(__riscv_vsrl_vx_u16m4(__riscv_vreinterpret_v_u8m4_u16m4(v2), 4, __riscv_vsetvlmax_e16m4()));
    vuint8m4_t s3 = __riscv_vreinterpret_v_u16m4_u8m4(__riscv_vsrl_vx_u16m4(__riscv_vreinterpret_v_u8m4_u16m4(v3), 4, __riscv_vsetvlmax_e16m4()));

    vuint8m4_t idx2 = __riscv_vand_vx_u8m4(v2, 0xF, vl);
    vuint8m4_t idx1 = __riscv_vand_vx_u8m4(s1, 0xF, vl);
    vuint8m4_t idx3 = __riscv_vand_vx_u8m4(s3, 0xF, vl);

    vuint8m4_t err1 = simdutf_vrgather_u8m1x4(err1tbl, idx1);
    vuint8m4_t err2 = simdutf_vrgather_u8m1x4(err2tbl, idx2);
    vuint8m4_t err3 = simdutf_vrgather_u8m1x4(err3tbl, idx3);
    vint8m4_t errs = __riscv_vreinterpret_v_u8m4_i8m4(__riscv_vand_vv_u8m4(__riscv_vand_vv_u8m4(err1, err2, vl), err3, vl));

    vbool2_t is_3 = __riscv_vmsgtu_vx_u8m4_b2(v1, 0b11100000-1, vl);
    vbool2_t is_4 = __riscv_vmsgtu_vx_u8m4_b2(v0, 0b11110000-1, vl);
    vbool2_t is_34 = __riscv_vmor_mm_b2(is_3, is_4, vl);
    vbool2_t err34 = __riscv_vmxor_mm_b2(is_34, __riscv_vmslt_vx_i8m4_b2(errs, 0, vl), vl);
    vbool2_t errm = __riscv_vmor_mm_b2(__riscv_vmsgt_vx_i8m4_b2(errs, 0, vl), err34, vl);
    if (__riscv_vfirst_m_b2(errm , vl) >= 0)
      break;
  }

  /* we need to validate the last character */
  while (tail < len && (src[0] >> 6) == 0b10) --src, ++tail;
  return src - beg;
}

simdutf_warn_unused bool implementation::validate_utf8(const char *src, size_t len) const noexcept {
  size_t count = rvv_count_valid_utf8(src, len);
  return scalar::utf8::validate(src + count, len - count);
}

simdutf_warn_unused result implementation::validate_utf8_with_errors(const char *src, size_t len) const noexcept {
  size_t count = rvv_count_valid_utf8(src, len);
  result res = scalar::utf8::validate_with_errors(src + count, len - count);
  return result(res.error, count + res.count);
}

simdutf_warn_unused bool implementation::validate_utf16le(const char16_t *src, size_t len) const noexcept {
  return validate_utf16le_with_errors(src, len).error == error_code::SUCCESS;
}

simdutf_warn_unused bool implementation::validate_utf16be(const char16_t *src, size_t len) const noexcept {
  return validate_utf16be_with_errors(src, len).error == error_code::SUCCESS;
}

template<simdutf_ByteFlip bflip>
simdutf_really_inline static result rvv_validate_utf16_with_errors(const char16_t *src, size_t len) {
  const char16_t *beg = src;
  uint16_t last = 0;
  for (size_t vl; len > 0; len -= vl, src += vl, last = simdutf_byteflip<bflip>(src[-1])) {
    vl = __riscv_vsetvl_e16m8(len);
    vuint16m8_t v1 = __riscv_vle16_v_u16m8((const uint16_t*)src, vl);
    v1 = simdutf_byteflip<bflip>(v1, vl);
    vuint16m8_t v0 = __riscv_vslide1up_vx_u16m8(v1, last, vl);

    vbool2_t surhi = __riscv_vmseq_vx_u16m8_b2(__riscv_vand_vx_u16m8(v0, 0xFC00, vl), 0xD800, vl);
    vbool2_t surlo = __riscv_vmseq_vx_u16m8_b2(__riscv_vand_vx_u16m8(v1, 0xFC00, vl), 0xDC00, vl);

    long idx = __riscv_vfirst_m_b2(__riscv_vmxor_mm_b2(surhi, surlo, vl), vl);
    if (idx >= 0) {
      last = idx > 0 ? simdutf_byteflip<bflip>(src[idx-1]) : last;
      return result(error_code::SURROGATE, src - beg + idx - (last - 0xD800u < 0x400u));
      break;
    }
  }
  if (last - 0xD800u < 0x400u) {
    return result(error_code::SURROGATE, src - beg - 1); /* end on high surrogate */
  } else {
    return result(error_code::SUCCESS, src - beg);
  }
}

simdutf_warn_unused result implementation::validate_utf16le_with_errors(const char16_t *src, size_t len) const noexcept {
  return rvv_validate_utf16_with_errors<simdutf_ByteFlip::NONE>(src, len);
}

simdutf_warn_unused result implementation::validate_utf16be_with_errors(const char16_t *src, size_t len) const noexcept {
  if (supports_zvbb())
    return rvv_validate_utf16_with_errors<simdutf_ByteFlip::ZVBB>(src, len);
  else
    return rvv_validate_utf16_with_errors<simdutf_ByteFlip::V>(src, len);
}

simdutf_warn_unused bool implementation::validate_utf32(const char32_t *src, size_t len) const noexcept {
  size_t vlmax = __riscv_vsetvlmax_e32m8();
  vuint32m8_t max    = __riscv_vmv_v_x_u32m8(0x10FFFF, vlmax);
  vuint32m8_t maxOff = __riscv_vmv_v_x_u32m8(0xFFFFF7FF, vlmax);
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e32m8(len);
    vuint32m8_t v = __riscv_vle32_v_u32m8((uint32_t*)src, vl);
    vuint32m8_t off = __riscv_vadd_vx_u32m8(v, 0xFFFF2000, vl);
    max    = __riscv_vmaxu_vv_u32m8_tu(max, max, v, vl);
    maxOff = __riscv_vmaxu_vv_u32m8_tu(maxOff, maxOff, off, vl);
  }
  return __riscv_vfirst_m_b4(__riscv_vmor_mm_b4(
             __riscv_vmsne_vx_u32m8_b4(max, 0x10FFFF, vlmax),
             __riscv_vmsne_vx_u32m8_b4(maxOff, 0xFFFFF7FF, vlmax), vlmax), vlmax) < 0;
}

simdutf_warn_unused result implementation::validate_utf32_with_errors(const char32_t *src, size_t len) const noexcept {
  const char32_t *beg = src;
  for (size_t vl; len > 0; len -= vl, src += vl) {
    vl = __riscv_vsetvl_e32m8(len);
    vuint32m8_t v = __riscv_vle32_v_u32m8((uint32_t*)src, vl);
    vuint32m8_t off = __riscv_vadd_vx_u32m8(v, 0xFFFF2000, vl);
    long idx1 = __riscv_vfirst_m_b4(__riscv_vmsgtu_vx_u32m8_b4(v, 0x10FFFF, vl), vl);
    long idx2 = __riscv_vfirst_m_b4(__riscv_vmsgtu_vx_u32m8_b4(off, 0xFFFFF7FF, vl), vl);
    if(idx1 >= 0 && idx2 >= 0) {
      if(idx1 <= idx2) { return result(error_code::TOO_LARGE, src - beg + idx1); }
      else { return result(error_code::SURROGATE, src - beg + idx2); }
    }
    if (idx1 >= 0) { return result(error_code::TOO_LARGE, src - beg + idx1); }
    if (idx2 >= 0) { return result(error_code::SURROGATE, src - beg + idx2); }
  }
  return result(error_code::SUCCESS, src - beg);
}

