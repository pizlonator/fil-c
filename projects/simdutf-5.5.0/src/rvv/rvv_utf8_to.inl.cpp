template<simdutf_ByteFlip bflip>
simdutf_really_inline static size_t rvv_utf32_store_utf16_m4(uint16_t *dst, vuint32m4_t utf32, size_t vl, vbool4_t m4even) {
  /* convert [000000000000aaaa|aaaaaabbbbbbbbbb]
   * to      [110111bbbbbbbbbb|110110aaaaaaaaaa] */
  vuint32m4_t sur = __riscv_vsub_vx_u32m4(utf32, 0x10000, vl);
  sur = __riscv_vor_vv_u32m4(__riscv_vsll_vx_u32m4(sur, 16, vl),
                             __riscv_vsrl_vx_u32m4(sur, 10, vl), vl);
  sur = __riscv_vand_vx_u32m4(sur, 0x3FF03FF, vl);
  sur = __riscv_vor_vx_u32m4(sur, 0xDC00D800, vl);
  /* merge 1 byte utf32 and 2 byte sur */
  vbool8_t m4 = __riscv_vmsgtu_vx_u32m4_b8(utf32, 0xFFFF, vl);
  vuint16m4_t utf32_16 = __riscv_vreinterpret_v_u32m4_u16m4(__riscv_vmerge_vvm_u32m4(utf32, sur, m4, vl));
  /* compress and store */
  vbool4_t mOut = __riscv_vmor_mm_b4(__riscv_vmsne_vx_u16m4_b4(utf32_16, 0, vl*2), m4even, vl*2);
  vuint16m4_t vout = __riscv_vcompress_vm_u16m4(utf32_16, mOut, vl*2);
  vl = __riscv_vcpop_m_b4(mOut, vl*2);
  __riscv_vse16_v_u16m4(dst, simdutf_byteflip<bflip>(vout, vl), vl);
  return vl;
};

template<typename Tdst, simdutf_ByteFlip bflip, bool validate=true>
simdutf_really_inline static size_t rvv_utf8_to_common(char const *src, size_t len, Tdst *dst) {
  static_assert(std::is_same<Tdst, uint16_t>() || std::is_same<Tdst, uint32_t>(), "invalid type");
  constexpr bool is16 = std::is_same<Tdst, uint16_t>();
  constexpr endianness endian = bflip == simdutf_ByteFlip::NONE ? endianness::LITTLE : endianness::BIG;
  const auto scalar = [](char const *in, size_t count, Tdst *out) {
    return is16 ? scalar::utf8_to_utf16::convert<endian>(in, count, (char16_t*)out)
                : scalar::utf8_to_utf32::convert(in, count, (char32_t*)out);
  };

  if (len < 32) return scalar(src, len, dst);

  /* validate first three bytes */
  if (validate) {
    size_t idx = 3;
    while (idx < len && (src[idx] >> 6) == 0b10)
      ++idx;
    if (idx > 3+3 || !scalar::utf8::validate(src, idx))
      return 0;
  }

  size_t tail = 3;
  size_t n = len - tail;
  Tdst *beg = dst;

  static const uint64_t err1m[] = { 0x0202020202020202, 0x4915012180808080 };
  static const uint64_t err2m[] = { 0xCBCBCB8B8383A3E7, 0xCBCBDBCBCBCBCBCB };
  static const uint64_t err3m[] = { 0x0101010101010101, 0X01010101BABAAEE6 };

  const vuint8m1_t err1tbl = __riscv_vreinterpret_v_u64m1_u8m1(__riscv_vle64_v_u64m1(err1m, 2));
  const vuint8m1_t err2tbl = __riscv_vreinterpret_v_u64m1_u8m1(__riscv_vle64_v_u64m1(err2m, 2));
  const vuint8m1_t err3tbl = __riscv_vreinterpret_v_u64m1_u8m1(__riscv_vle64_v_u64m1(err3m, 2));

  size_t vl8m2 = __riscv_vsetvlmax_e8m2();
  vbool4_t m4even = __riscv_vmseq_vx_u8m2_b4(__riscv_vand_vx_u8m2(__riscv_vid_v_u8m2(vl8m2), 1, vl8m2), 0, vl8m2);

  for (size_t vl, vlOut; n > 0; n -= vl, src += vl, dst += vlOut) {
    vl = __riscv_vsetvl_e8m2(n);

    vuint8m2_t v0 = __riscv_vle8_v_u8m2((uint8_t const*)src, vl);
    uint64_t max = __riscv_vmv_x_s_u8m1_u8(__riscv_vredmaxu_vs_u8m2_u8m1(v0, __riscv_vmv_s_x_u8m1(0, vl), vl));

    uint8_t next0 = src[vl+0];
    uint8_t next1 = src[vl+1];
    uint8_t next2 = src[vl+2];

    /* fast path: ASCII */
    if ((max|next0|next1|next2) < 0b10000000) {
      vlOut = vl;
      if (is16) __riscv_vse16_v_u16m4((uint16_t*)dst, simdutf_byteflip<bflip>(__riscv_vzext_vf2_u16m4(v0, vlOut), vlOut), vlOut);
      else      __riscv_vse32_v_u32m8((uint32_t*)dst, __riscv_vzext_vf4_u32m8(v0, vlOut), vlOut);
      continue;
    }

    /* see "Validating UTF-8 In Less Than One Instruction Per Byte"
     * https://arxiv.org/abs/2010.03090 */
    vuint8m2_t v1 = __riscv_vslide1down_vx_u8m2(v0, next0, vl);
    vuint8m2_t v2 = __riscv_vslide1down_vx_u8m2(v1, next1, vl);
    vuint8m2_t v3 = __riscv_vslide1down_vx_u8m2(v2, next2, vl);

    if (validate) {
      vuint8m2_t s1 = __riscv_vreinterpret_v_u16m2_u8m2(__riscv_vsrl_vx_u16m2(__riscv_vreinterpret_v_u8m2_u16m2(v2), 4, __riscv_vsetvlmax_e16m2()));
      vuint8m2_t s3 = __riscv_vreinterpret_v_u16m2_u8m2(__riscv_vsrl_vx_u16m2(__riscv_vreinterpret_v_u8m2_u16m2(v3), 4, __riscv_vsetvlmax_e16m2()));

      vuint8m2_t idx2 = __riscv_vand_vx_u8m2(v2, 0xF, vl);
      vuint8m2_t idx1 = __riscv_vand_vx_u8m2(s1, 0xF, vl);
      vuint8m2_t idx3 = __riscv_vand_vx_u8m2(s3, 0xF, vl);

      vuint8m2_t err1 = simdutf_vrgather_u8m1x2(err1tbl, idx1);
      vuint8m2_t err2 = simdutf_vrgather_u8m1x2(err2tbl, idx2);
      vuint8m2_t err3 = simdutf_vrgather_u8m1x2(err3tbl, idx3);
      vint8m2_t errs = __riscv_vreinterpret_v_u8m2_i8m2(__riscv_vand_vv_u8m2(__riscv_vand_vv_u8m2(err1, err2, vl), err3, vl));

      vbool4_t is_3 = __riscv_vmsgtu_vx_u8m2_b4(v1, 0b11100000-1, vl);
      vbool4_t is_4 = __riscv_vmsgtu_vx_u8m2_b4(v0, 0b11110000-1, vl);
      vbool4_t is_34 = __riscv_vmor_mm_b4(is_3, is_4, vl);
      vbool4_t err34 = __riscv_vmxor_mm_b4(is_34, __riscv_vmslt_vx_i8m2_b4(errs, 0, vl), vl);
      vbool4_t errm = __riscv_vmor_mm_b4(__riscv_vmsgt_vx_i8m2_b4(errs, 0, vl), err34, vl);
      if (__riscv_vfirst_m_b4(errm , vl) >= 0)
        return 0;
    }

    /* decoding */

    /* mask of non continuation bytes */
    vbool4_t m = __riscv_vmsgt_vx_i8m2_b4(__riscv_vreinterpret_v_u8m2_i8m2(v0), -65, vl);
    vlOut = __riscv_vcpop_m_b4(m, vl);

    /* extract first and second bytes */
    vuint8m2_t b1 = __riscv_vcompress_vm_u8m2(v0, m, vl);
    vuint8m2_t b2 = __riscv_vcompress_vm_u8m2(v1, m, vl);

    /* fast path: one and two byte */
    if (max < 0b11100000) {
      b2 = __riscv_vand_vx_u8m2(b2, 0b00111111, vlOut);

      vbool4_t m1 = __riscv_vmsgtu_vx_u8m2_b4(b1, 0b10111111, vlOut);
      b1 = __riscv_vand_vx_u8m2_mu(m1, b1, b1, 63, vlOut);

      vuint16m4_t b12 = __riscv_vwmulu_vv_u16m4(b1, __riscv_vmerge_vxm_u8m2(__riscv_vmv_v_x_u8m2(1, vlOut), 1<<6, m1, vlOut), vlOut);
      b12 = __riscv_vwaddu_wv_u16m4_mu(m1, b12, b12, b2, vlOut);
      if (is16) __riscv_vse16_v_u16m4((uint16_t*)dst, simdutf_byteflip<bflip>(b12, vlOut), vlOut);
      else      __riscv_vse32_v_u32m8((uint32_t*)dst, __riscv_vzext_vf2_u32m8(b12, vlOut), vlOut);
      continue;
    }

    /* fast path: one, two and three byte */
    if (max < 0b11110000) {
      vuint8m2_t b3 = __riscv_vcompress_vm_u8m2(v2, m, vl);

      b2 = __riscv_vand_vx_u8m2(b2, 0b00111111, vlOut);
      b3 = __riscv_vand_vx_u8m2(b3, 0b00111111, vlOut);

      vbool4_t m1 = __riscv_vmsgtu_vx_u8m2_b4(b1, 0b10111111, vlOut);
      vbool4_t m3 = __riscv_vmsgtu_vx_u8m2_b4(b1, 0b11011111, vlOut);

      vuint8m2_t t1 = __riscv_vand_vx_u8m2_mu(m1, b1, b1, 63, vlOut);
      b1 = __riscv_vand_vx_u8m2_mu(m3, t1, b1, 15, vlOut);

      vuint16m4_t b12 = __riscv_vwmulu_vv_u16m4(b1, __riscv_vmerge_vxm_u8m2(__riscv_vmv_v_x_u8m2(1, vlOut), 1<<6, m1, vlOut), vlOut);
      b12 = __riscv_vwaddu_wv_u16m4_mu(m1, b12, b12, b2, vlOut);
      vuint16m4_t b123 = __riscv_vwaddu_wv_u16m4_mu(m3, b12, __riscv_vsll_vx_u16m4_mu(m3, b12, b12, 6, vlOut), b3, vlOut);
      if (is16) __riscv_vse16_v_u16m4((uint16_t*)dst, simdutf_byteflip<bflip>(b123, vlOut), vlOut);
      else      __riscv_vse32_v_u32m8((uint32_t*)dst, __riscv_vzext_vf2_u32m8(b123, vlOut), vlOut);
      continue;
    }

    /* extract third and fourth bytes */
    vuint8m2_t b3 = __riscv_vcompress_vm_u8m2(v2, m, vl);
    vuint8m2_t b4 = __riscv_vcompress_vm_u8m2(v3, m, vl);

     /* remove prefix from leading bytes
      *
      * We could also use vrgather here, but it increases register pressure,
      * and its performance varies widely on current platforms. It might be
      * worth reconsidering, though, once there is more hardware available.
      * Same goes for the __riscv_vsrl_vv_u32m4 correction step.
      *
      * We shift left and then right by the number of bytes in the prefix,
      * which can be calculated as follows:
      *         x                                max(x-10, 0)
      * 0xxx -> 0000-0111 -> sift by 0 or 1   -> 0
      * 10xx -> 1000-1011 -> don't care
      * 110x -> 1100,1101 -> sift by 3        -> 2,3
      * 1110 -> 1110      -> sift by 4        -> 4
      * 1111 -> 1111      -> sift by 5        -> 5
      *
      * vssubu.vx v, 10, (max(x-10, 0)) almost gives us what we want, we
      * just need to manually detect and handle the one special case:
      */
    #define SIMDUTF_RVV_UTF8_TO_COMMON_M1(idx) \
      vuint8m1_t c1 = __riscv_vget_v_u8m2_u8m1(b1, idx); \
      vuint8m1_t c2 = __riscv_vget_v_u8m2_u8m1(b2, idx); \
      vuint8m1_t c3 = __riscv_vget_v_u8m2_u8m1(b3, idx); \
      vuint8m1_t c4 = __riscv_vget_v_u8m2_u8m1(b4, idx); \
      /* remove prefix from trailing bytes */ \
      c2 = __riscv_vand_vx_u8m1(c2, 0b00111111, vlOut); \
      c3 = __riscv_vand_vx_u8m1(c3, 0b00111111, vlOut); \
      c4 = __riscv_vand_vx_u8m1(c4, 0b00111111, vlOut);  \
      vuint8m1_t shift = __riscv_vsrl_vx_u8m1(c1, 4, vlOut); \
      shift = __riscv_vmerge_vxm_u8m1(__riscv_vssubu_vx_u8m1(shift, 10, vlOut), 3, __riscv_vmseq_vx_u8m1_b8(shift, 12, vlOut), vlOut); \
      c1 = __riscv_vsll_vv_u8m1(c1, shift, vlOut); \
      c1 = __riscv_vsrl_vv_u8m1(c1, shift, vlOut); \
      /* unconditionally widen and combine to c1234 */ \
      vuint16m2_t c34 = __riscv_vwaddu_wv_u16m2(__riscv_vwmulu_vx_u16m2(c3, 1<<6, vlOut), c4, vlOut); \
      vuint16m2_t c12 = __riscv_vwaddu_wv_u16m2(__riscv_vwmulu_vx_u16m2(c1, 1<<6, vlOut), c2, vlOut); \
      vuint32m4_t c1234 = __riscv_vwaddu_wv_u32m4(__riscv_vwmulu_vx_u32m4(c12, 1 << 12, vlOut), c34, vlOut); \
      /* derive required right-shift amount from `shift` to reduce
       * c1234 to the required number of bytes */ \
      c1234 = __riscv_vsrl_vv_u32m4(c1234, __riscv_vzext_vf4_u32m4(__riscv_vmul_vx_u8m1( \
              __riscv_vrsub_vx_u8m1( __riscv_vssubu_vx_u8m1(shift, 2, vlOut), 3, vlOut), 6, vlOut), vlOut), vlOut); \
      /* store result in desired format */ \
      if (is16) vlDst = rvv_utf32_store_utf16_m4<bflip>((uint16_t*)dst, c1234, vlOut, m4even); \
      else      vlDst = vlOut, __riscv_vse32_v_u32m4((uint32_t*)dst, c1234, vlOut);

    /* Unrolling this manually reduces register pressure and allows
     * us to terminate early. */
    {
      size_t vlOutm2 = vlOut, vlDst;
      vlOut = __riscv_vsetvl_e8m1(vlOut);
      SIMDUTF_RVV_UTF8_TO_COMMON_M1(0)
      if (vlOutm2 == vlOut) {
        vlOut = vlDst;
        continue;
      }

      dst += vlDst;
      vlOut = vlOutm2 - vlOut;
    }
    {
      size_t vlDst;
      SIMDUTF_RVV_UTF8_TO_COMMON_M1(1)
      vlOut = vlDst;
    }

#undef SIMDUTF_RVV_UTF8_TO_COMMON_M1
  }

  /* validate the last character and reparse it + tail */
  if (len > tail) {
    if ((src[0] >> 6) == 0b10)
      --dst;
    while ((src[0] >> 6) == 0b10 && tail < len)
      --src, ++tail;
    if (is16) {
      /* go back one more, when on high surrogate */
      if (simdutf_byteflip<bflip>((uint16_t)dst[-1]) >= 0xD800 && simdutf_byteflip<bflip>((uint16_t)dst[-1]) <= 0xDBFF)
        --dst;
    }
  }
  size_t ret = scalar(src, tail, dst);
  if (ret == 0) return 0;
  return (size_t)(dst - beg) + ret;
}


simdutf_warn_unused size_t implementation::convert_utf8_to_latin1(const char *src, size_t len, char *dst) const noexcept {
  const char *beg = dst;
  uint8_t last = 0;
  for (size_t vl, vlOut; len > 0; len -= vl, src += vl, dst += vlOut, last = src[-1]) {
    vl = __riscv_vsetvl_e8m2(len);
    vuint8m2_t v1 = __riscv_vle8_v_u8m2((uint8_t*)src, vl);
    // check which bytes are ASCII
    vbool4_t ascii = __riscv_vmsltu_vx_u8m2_b4(v1, 0b10000000, vl);
    // count ASCII bytes
    vlOut = __riscv_vcpop_m_b4(ascii, vl);
    // The original code would only enter the next block after this check:
    //   vbool4_t m = __riscv_vmsltu_vx_u8m2_b4(v1, 0b11000000, vl);
    //   vlOut = __riscv_vcpop_m_b4(m, vl);
    //   if (vlOut != vl || last > 0b01111111) {...}q
    // So that everything is ASCII or continuation bytes, we just proceeded
    // without any processing, going straight to __riscv_vse8_v_u8m2.
    // But you need the __riscv_vslide1up_vx_u8m2 whenever there is a non-ASCII byte.
    if (vlOut != vl) { // If not pure ASCII
      // Non-ASCII characters
      // We now want to mark the ascii and continuation bytes
      vbool4_t m = __riscv_vmsltu_vx_u8m2_b4(v1, 0b11000000, vl);
      // We count them, that's our new vlOut (output vector length)
      vlOut = __riscv_vcpop_m_b4(m, vl);

      vuint8m2_t v0 = __riscv_vslide1up_vx_u8m2(v1, last, vl);

      vbool4_t leading0  = __riscv_vmsgtu_vx_u8m2_b4(v0, 0b10111111, vl);
      vbool4_t trailing1 = __riscv_vmslt_vx_i8m2_b4(__riscv_vreinterpret_v_u8m2_i8m2(v1), (uint8_t)0b11000000, vl);
      // -62 i 0b11000010, so we check whether any of v0 is too big
      vbool4_t tobig = __riscv_vmand_mm_b4(leading0, __riscv_vmsgtu_vx_u8m2_b4(__riscv_vxor_vx_u8m2(v0, (uint8_t)-62, vl), 1, vl), vl);
      if (__riscv_vfirst_m_b4(__riscv_vmor_mm_b4(tobig, __riscv_vmxor_mm_b4(leading0, trailing1, vl), vl), vl) >= 0)
        return 0;

      v1 = __riscv_vor_vx_u8m2_mu(__riscv_vmseq_vx_u8m2_b4(v0, 0b11000011, vl), v1, v1, 0b01000000, vl);
      v1 = __riscv_vcompress_vm_u8m2(v1, m, vl);
    } else if (last >= 0b11000000) { // If last byte is a leading  byte and we got only ASCII, error!
      return 0;
    }
    __riscv_vse8_v_u8m2((uint8_t*)dst, v1, vlOut);
  }
  if (last > 0b10111111)
    return 0;
  return dst - beg;
}

simdutf_warn_unused result implementation::convert_utf8_to_latin1_with_errors(const char *src, size_t len, char *dst) const noexcept {
  size_t res = convert_utf8_to_latin1(src, len, dst);
  if (res) return result(error_code::SUCCESS, res);
  return scalar::utf8_to_latin1::convert_with_errors(src, len, dst);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_latin1(const char *src, size_t len, char *dst) const noexcept {
  const char *beg = dst;
  uint8_t last = 0;
  for (size_t vl, vlOut; len > 0; len -= vl, src += vl, dst += vlOut, last = src[-1]) {
    vl = __riscv_vsetvl_e8m2(len);
    vuint8m2_t v1 = __riscv_vle8_v_u8m2((uint8_t*)src, vl);
    vbool4_t ascii = __riscv_vmsltu_vx_u8m2_b4(v1, 0b10000000, vl);
    vlOut = __riscv_vcpop_m_b4(ascii, vl);
    if (vlOut != vl) { // If not pure ASCII
      vbool4_t m = __riscv_vmsltu_vx_u8m2_b4(v1, 0b11000000, vl);
      vlOut = __riscv_vcpop_m_b4(m, vl);
      vuint8m2_t v0 = __riscv_vslide1up_vx_u8m2(v1, last, vl);
      v1 = __riscv_vor_vx_u8m2_mu(__riscv_vmseq_vx_u8m2_b4(v0, 0b11000011, vl), v1, v1, 0b01000000, vl);
      v1 = __riscv_vcompress_vm_u8m2(v1, m, vl);
    }
    __riscv_vse8_v_u8m2((uint8_t*)dst, v1, vlOut);
  }
  return dst - beg;
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16le(const char *src, size_t len, char16_t *dst) const noexcept {
  return rvv_utf8_to_common<uint16_t, simdutf_ByteFlip::NONE>(src, len, (uint16_t*)dst);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16be(const char *src, size_t len, char16_t *dst) const noexcept {
  if (supports_zvbb())
    return rvv_utf8_to_common<uint16_t, simdutf_ByteFlip::ZVBB>(src, len, (uint16_t*)dst);
  else
    return rvv_utf8_to_common<uint16_t, simdutf_ByteFlip::V>(src, len, (uint16_t*)dst);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16le_with_errors(const char *src, size_t len, char16_t *dst) const noexcept {
  size_t res = convert_utf8_to_utf16le(src, len, dst);
  if (res) return result(error_code::SUCCESS, res);
  return scalar::utf8_to_utf16::convert_with_errors<endianness::LITTLE>(src, len, dst);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16be_with_errors(const char *src, size_t len, char16_t *dst) const noexcept {
  size_t res = convert_utf8_to_utf16be(src, len, dst);
  if (res) return result(error_code::SUCCESS, res);
  return scalar::utf8_to_utf16::convert_with_errors<endianness::BIG>(src, len, dst);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16le(const char *src, size_t len, char16_t *dst) const noexcept {
  return rvv_utf8_to_common<uint16_t, simdutf_ByteFlip::NONE, false>(src, len, (uint16_t*)dst);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16be(const char *src, size_t len, char16_t *dst) const noexcept {
  if (supports_zvbb())
    return rvv_utf8_to_common<uint16_t, simdutf_ByteFlip::ZVBB, false>(src, len, (uint16_t*)dst);
  else
    return rvv_utf8_to_common<uint16_t, simdutf_ByteFlip::V, false>(src, len, (uint16_t*)dst);
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf32(const char *src, size_t len, char32_t *dst) const noexcept {
  return rvv_utf8_to_common<uint32_t, simdutf_ByteFlip::NONE>(src, len, (uint32_t*)dst);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf32_with_errors(const char *src, size_t len, char32_t *dst) const noexcept {
  size_t res = convert_utf8_to_utf32(src, len, dst);
  if (res) return result(error_code::SUCCESS, res);
  return scalar::utf8_to_utf32::convert_with_errors(src, len, dst);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf32(const char *src, size_t len, char32_t *dst) const noexcept {
  return rvv_utf8_to_common<uint32_t, simdutf_ByteFlip::NONE, false>(src, len, (uint32_t*)dst);
}

