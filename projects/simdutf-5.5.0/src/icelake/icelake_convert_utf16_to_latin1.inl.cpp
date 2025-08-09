// file included directly
template <endianness big_endian>
size_t icelake_convert_utf16_to_latin1(const char16_t *buf, size_t len,
                                       char *latin1_output) {
  const char16_t *end = buf + len;
  __m512i v_0xFF = _mm512_set1_epi16(0xff);
  __m512i byteflip = _mm512_setr_epi64(0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809);
  __m512i shufmask = _mm512_set_epi8(
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 62, 60, 58, 56, 54, 52, 50, 48, 46, 44, 42, 40, 38,
      36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 6, 4, 2, 0);
  while (end - buf >= 32) {
    __m512i in = _mm512_loadu_si512((__m512i *)buf);
    if (big_endian) {
      in = _mm512_shuffle_epi8(in, byteflip);
    }
    if (_mm512_cmpgt_epu16_mask(in, v_0xFF)) {
      return 0;
    }
    _mm256_storeu_si256(
        (__m256i *)latin1_output,
        _mm512_castsi512_si256(_mm512_permutexvar_epi8(shufmask, in)));
    latin1_output += 32;
    buf += 32;
  }
  if (buf < end) {
    uint32_t mask(uint32_t(1 << (end - buf)) - 1);
    __m512i in = _mm512_maskz_loadu_epi16(mask, buf);
    if (big_endian) {
      in = _mm512_shuffle_epi8(in, byteflip);
    }
    if (_mm512_cmpgt_epu16_mask(in, v_0xFF)) {
      return 0;
    }
    _mm256_mask_storeu_epi8(
        latin1_output, mask,
        _mm512_castsi512_si256(_mm512_permutexvar_epi8(shufmask, in)));
  }
  return len;
}

template <endianness big_endian>
std::pair<result, char *>
icelake_convert_utf16_to_latin1_with_errors(const char16_t *buf, size_t len,
                                            char *latin1_output) {
  const char16_t *end = buf + len;
  const char16_t *start = buf;
  __m512i byteflip = _mm512_setr_epi64(0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809,
                                       0x0607040502030001, 0x0e0f0c0d0a0b0809);
  __m512i v_0xFF = _mm512_set1_epi16(0xff);
  __m512i shufmask = _mm512_set_epi8(
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 62, 60, 58, 56, 54, 52, 50, 48, 46, 44, 42, 40, 38,
      36, 34, 32, 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10, 8, 6, 4, 2, 0);
  while (end - buf >= 32) {
    __m512i in = _mm512_loadu_si512((__m512i *)buf);
    if (big_endian) {
      in = _mm512_shuffle_epi8(in, byteflip);
    }
    if (_mm512_cmpgt_epu16_mask(in, v_0xFF)) {
      uint16_t word;
      while ((word = (big_endian ? scalar::utf16::swap_bytes(uint16_t(*buf))
                                 : uint16_t(*buf))) <= 0xff) {
        *latin1_output++ = uint8_t(word);
        buf++;
      }
      return std::make_pair(result(error_code::TOO_LARGE, buf - start),
                            latin1_output);
    }
    _mm256_storeu_si256(
        (__m256i *)latin1_output,
        _mm512_castsi512_si256(_mm512_permutexvar_epi8(shufmask, in)));
    latin1_output += 32;
    buf += 32;
  }
  if (buf < end) {
    uint32_t mask(uint32_t(1 << (end - buf)) - 1);
    __m512i in = _mm512_maskz_loadu_epi16(mask, buf);
    if (big_endian) {
      in = _mm512_shuffle_epi8(in, byteflip);
    }
    if (_mm512_cmpgt_epu16_mask(in, v_0xFF)) {

      uint16_t word;
      while ((word = (big_endian ? scalar::utf16::swap_bytes(uint16_t(*buf))
                                 : uint16_t(*buf))) <= 0xff) {
        *latin1_output++ = uint8_t(word);
        buf++;
      }
      return std::make_pair(result(error_code::TOO_LARGE, buf - start),
                            latin1_output);
    }
    _mm256_mask_storeu_epi8(
        latin1_output, mask,
        _mm512_castsi512_si256(_mm512_permutexvar_epi8(shufmask, in)));
  }
  return std::make_pair(result(error_code::SUCCESS, len), latin1_output);
}
