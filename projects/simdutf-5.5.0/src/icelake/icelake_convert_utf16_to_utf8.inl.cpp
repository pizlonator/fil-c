// file included directly

/**
 * This function converts the input (inbuf, inlen), assumed to be valid
 * UTF16 (little endian) into UTF-8 (to outbuf). The number of code units written
 * is written to 'outlen' and the function reports the number of input word
 * consumed.
 */
template <endianness big_endian>
size_t utf16_to_utf8_avx512i(const char16_t *inbuf, size_t inlen,
                               unsigned char *outbuf, size_t *outlen) {
  __m512i in;
  __mmask32 inmask = _cvtu32_mask32(0x7fffffff);
  __m512i byteflip = _mm512_setr_epi64(
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809
        );
  const char16_t * const inbuf_orig = inbuf;
  const unsigned char * const outbuf_orig = outbuf;
  int adjust = 0;
  int carry = 0;

  while (inlen >= 32) {
    in = _mm512_loadu_si512(inbuf);
    if(big_endian) { in = _mm512_shuffle_epi8(in, byteflip); }
    inlen -= 31;
  lastiteration:
    inbuf += 31;

  failiteration:
    const __mmask32 is234byte = _mm512_mask_cmp_epu16_mask(
      inmask, in, _mm512_set1_epi16(0x0080), _MM_CMPINT_NLT);

    if (_ktestz_mask32_u8(inmask, is234byte)) {
      // fast path for ASCII only
      _mm512_mask_cvtepi16_storeu_epi8(outbuf, inmask, in);
      outbuf += 31;
      carry = 0;

      if (inlen < 32) {
        goto tail;
      } else {
        continue;
      }
    }

    const __mmask32 is12byte =
        _mm512_cmp_epu16_mask(in, _mm512_set1_epi16(0x0800), _MM_CMPINT_LT);

    if (_ktestc_mask32_u8(is12byte, inmask)) {
      // fast path for 1 and 2 byte only

      const __m512i twobytes = _mm512_ternarylogic_epi32(
          _mm512_slli_epi16(in, 8), _mm512_srli_epi16(in, 6),
          _mm512_set1_epi16(0x3f3f), 0xa8); // (A|B)&C
      in = _mm512_mask_add_epi16(in, is234byte, twobytes,
                                 _mm512_set1_epi16(int16_t(0x80c0)));
      const __m512i cmpmask =
          _mm512_mask_blend_epi16(inmask, _mm512_set1_epi16(int16_t(0xffff)),
                                  _mm512_set1_epi16(0x0800));
      const __mmask64 smoosh = _mm512_cmp_epu8_mask(in, cmpmask, _MM_CMPINT_NLT);
      const __m512i out = _mm512_maskz_compress_epi8(smoosh, in);
      _mm512_mask_storeu_epi8(outbuf, _cvtu64_mask64(_pext_u64(_cvtmask64_u64(smoosh), _cvtmask64_u64(smoosh))),
                              out);
      outbuf += 31 + _mm_popcnt_u32(_cvtmask32_u32(is234byte));
      carry = 0;

      if (inlen < 32) {
        goto tail;
      } else {
        continue;
      }
    }
    __m512i lo = _mm512_cvtepu16_epi32(_mm512_castsi512_si256(in));
    __m512i hi = _mm512_cvtepu16_epi32(_mm512_extracti32x8_epi32(in, 1));


    __m512i taglo = _mm512_set1_epi32(0x8080e000);
    __m512i taghi = taglo;

    const __m512i fc00masked = _mm512_and_epi32(in, _mm512_set1_epi16(int16_t(0xfc00)));
    const __mmask32 hisurr = _mm512_mask_cmp_epu16_mask(
        inmask, fc00masked, _mm512_set1_epi16(int16_t(0xd800)), _MM_CMPINT_EQ);
    const __mmask32 losurr = _mm512_cmp_epu16_mask(
        fc00masked, _mm512_set1_epi16(int16_t(0xdc00)), _MM_CMPINT_EQ);

    int carryout = 0;
    if (!_kortestz_mask32_u8(hisurr, losurr)) {
      // handle surrogates

      __m512i los = _mm512_alignr_epi32(hi, lo, 1);
      __m512i his = _mm512_alignr_epi32(lo, hi, 1);

      const __mmask32 hisurrhi = _kshiftri_mask32(hisurr, 16);
      taglo =
          _mm512_mask_mov_epi32(taglo,__mmask16(hisurr), _mm512_set1_epi32(0x808080f0));
      taghi =
          _mm512_mask_mov_epi32(taghi, __mmask16(hisurrhi), _mm512_set1_epi32(0x808080f0));

      lo = _mm512_mask_slli_epi32(lo, __mmask16(hisurr), lo, 10);
      hi = _mm512_mask_slli_epi32(hi, __mmask16(hisurrhi), hi, 10);
      los = _mm512_add_epi32(los, _mm512_set1_epi32(0xfca02400));
      his = _mm512_add_epi32(his, _mm512_set1_epi32(0xfca02400));
      lo = _mm512_mask_add_epi32(lo, __mmask16(hisurr), lo, los);
      hi = _mm512_mask_add_epi32(hi, __mmask16(hisurrhi), hi, his);

      carryout = _cvtu32_mask32(_kshiftri_mask32(hisurr, 30));

      const uint32_t  h = _cvtmask32_u32(hisurr);
      const uint32_t  l = _cvtmask32_u32(losurr);
      // check for mismatched surrogates
      if ((h + h + carry) ^ l) {
        const uint32_t lonohi = l & ~(h + h + carry);
        const uint32_t hinolo = h & ~(l >> 1);
        inlen = _tzcnt_u32(hinolo | lonohi);
        inmask = __mmask32(0x7fffffff & ((1U << inlen) - 1));
        in = _mm512_maskz_mov_epi16(inmask, in);
        adjust = (int)inlen - 31;
        inlen = 0;
        goto failiteration;
      }
    }

    hi = _mm512_maskz_mov_epi32(_cvtu32_mask16(0x7fff),hi);
    carry = carryout;

    __m512i mslo =
        _mm512_multishift_epi64_epi8(_mm512_set1_epi64(0x20262c3200060c12), lo);

    __m512i mshi =
        _mm512_multishift_epi64_epi8(_mm512_set1_epi64(0x20262c3200060c12), hi);

    const __mmask32 outmask = __mmask32(_kandn_mask64(losurr, inmask));
    const __mmask64 outmhi = _kshiftri_mask64(outmask, 16);

    const __mmask32 is1byte = __mmask32(_knot_mask64(is234byte));
    const __mmask64 is1bhi = _kshiftri_mask64(is1byte, 16);
    const __mmask64 is12bhi = _kshiftri_mask64(is12byte, 16);

    taglo =
        _mm512_mask_mov_epi32(taglo, __mmask16(is12byte), _mm512_set1_epi32(0x80c00000));
    taghi =
        _mm512_mask_mov_epi32(taghi, __mmask16(is12bhi), _mm512_set1_epi32(0x80c00000));
    __m512i magiclo = _mm512_mask_blend_epi32(__mmask16(outmask), _mm512_set1_epi32(0xffffffff),
                                      _mm512_set1_epi32(0x00010101));
    __m512i magichi = _mm512_mask_blend_epi32(__mmask16(outmhi), _mm512_set1_epi32(0xffffffff),
                                      _mm512_set1_epi32(0x00010101));


    magiclo = _mm512_mask_blend_epi32(__mmask16(outmask), _mm512_set1_epi32(0xffffffff),
                                      _mm512_set1_epi32(0x00010101));
    magichi = _mm512_mask_blend_epi32(__mmask16(outmhi), _mm512_set1_epi32(0xffffffff),
                                      _mm512_set1_epi32(0x00010101));

    mslo = _mm512_ternarylogic_epi32(mslo, _mm512_set1_epi32(0x3f3f3f3f), taglo,
                                     0xea); // A&B|C
    mshi = _mm512_ternarylogic_epi32(mshi, _mm512_set1_epi32(0x3f3f3f3f), taghi,
                                     0xea);
    mslo = _mm512_mask_slli_epi32(mslo, __mmask16(is1byte), lo, 24);

    mshi = _mm512_mask_slli_epi32(mshi, __mmask16(is1bhi), hi, 24);

    const __mmask64 wantlo = _mm512_cmp_epu8_mask(mslo, magiclo, _MM_CMPINT_NLT);
    const __mmask64 wanthi = _mm512_cmp_epu8_mask(mshi, magichi, _MM_CMPINT_NLT);
    const __m512i outlo = _mm512_maskz_compress_epi8(wantlo, mslo);
    const __m512i outhi = _mm512_maskz_compress_epi8(wanthi, mshi);
    const uint64_t wantlo_uint64 = _cvtmask64_u64(wantlo);
    const uint64_t wanthi_uint64 = _cvtmask64_u64(wanthi);

    uint64_t advlo = _mm_popcnt_u64(wantlo_uint64);
    uint64_t advhi = _mm_popcnt_u64(wanthi_uint64);

    _mm512_mask_storeu_epi8(outbuf, _cvtu64_mask64(_pext_u64(wantlo_uint64, wantlo_uint64)), outlo);
    _mm512_mask_storeu_epi8(outbuf + advlo, _cvtu64_mask64(_pext_u64(wanthi_uint64, wanthi_uint64)), outhi);
    outbuf += advlo + advhi;
  }
  outbuf += -adjust;

tail:
  if (inlen != 0) {
    // We must have inlen < 31.
    inmask = _cvtu32_mask32((1U << inlen) - 1);
    in = _mm512_maskz_loadu_epi16(inmask, inbuf);
    if(big_endian) { in = _mm512_shuffle_epi8(in, byteflip); }
    adjust = (int)inlen - 31;
    inlen = 0;
    goto lastiteration;
  }
  *outlen = (outbuf - outbuf_orig) + adjust;
  return ((inbuf - inbuf_orig) + adjust);
}