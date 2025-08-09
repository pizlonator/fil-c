// file included directly
size_t icelake_convert_utf32_to_latin1(const char32_t *buf, size_t len,
                                       char *latin1_output) {
  const char32_t *end = buf + len;
  __m512i v_0xFF = _mm512_set1_epi32(0xff);
  __m512i shufmask = _mm512_set_epi8(
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60,
      56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0);
  while (end - buf >= 16) {
    __m512i in = _mm512_loadu_si512((__m512i *)buf);
    if (_mm512_cmpgt_epu32_mask(in, v_0xFF)) {
      return 0;
    }
    _mm_storeu_si128((__m128i *)latin1_output,
                     _mm512_castsi512_si128(_mm512_permutexvar_epi8(shufmask, in)));
    latin1_output += 16;
    buf += 16;
  }
  if (buf < end) {
    uint16_t mask = uint16_t((1 << (end - buf)) - 1);
    __m512i in = _mm512_maskz_loadu_epi32(mask, buf);
    if (_mm512_cmpgt_epu32_mask(in, v_0xFF)) {
      return 0;
    }
    _mm_mask_storeu_epi8(
        latin1_output, mask,
        _mm512_castsi512_si128(_mm512_permutexvar_epi8(shufmask, in)));
  }
  return len;
}

std::pair<result, char *>
icelake_convert_utf32_to_latin1_with_errors(const char32_t *buf, size_t len,
                                            char *latin1_output) {
  const char32_t *end = buf + len;
  const char32_t *start = buf;
  __m512i v_0xFF = _mm512_set1_epi32(0xff);
  __m512i shufmask = _mm512_set_epi8(
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 60,
      56, 52, 48, 44, 40, 36, 32, 28, 24, 20, 16, 12, 8, 4, 0);
  while (end - buf >= 16) {
    __m512i in = _mm512_loadu_si512((__m512i *)buf);
    if (_mm512_cmpgt_epu32_mask(in, v_0xFF)) {
      while (uint32_t(*buf) <= 0xff) {
        *latin1_output++ = uint8_t(*buf++);
      }
      return std::make_pair(result(error_code::TOO_LARGE, buf - start),
                            latin1_output);
    }
    _mm_storeu_si128((__m128i *)latin1_output,
                     _mm512_castsi512_si128(_mm512_permutexvar_epi8(shufmask, in)));
    latin1_output += 16;
    buf += 16;
  }
  if (buf < end) {
    uint16_t mask = uint16_t((1 << (end - buf)) - 1);
    __m512i in = _mm512_maskz_loadu_epi32(mask, buf);
    if (_mm512_cmpgt_epu32_mask(in, v_0xFF)) {
      while (uint32_t(*buf) <= 0xff) {
        *latin1_output++ = uint8_t(*buf++);
      }
      return std::make_pair(result(error_code::TOO_LARGE, buf - start),
                            latin1_output);
    }
    _mm_mask_storeu_epi8(
        latin1_output, mask,
        _mm512_castsi512_si128(_mm512_permutexvar_epi8(shufmask, in)));
  }
  return std::make_pair(result(error_code::SUCCESS, len), latin1_output);
}
