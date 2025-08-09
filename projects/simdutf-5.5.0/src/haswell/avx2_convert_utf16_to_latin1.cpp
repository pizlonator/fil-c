template <endianness big_endian>
std::pair<const char16_t *, char *>
avx2_convert_utf16_to_latin1(const char16_t *buf, size_t len,
                             char *latin1_output) {
  const char16_t *end = buf + len;
  while (end - buf >= 16) {
    // Load 16 UTF-16 characters into 256-bit AVX2 register
    __m256i in = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(buf));

    if (!match_system(big_endian)) {
      const __m256i swap = _mm256_setr_epi8(
          1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 17, 16, 19, 18,
          21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30);
      in = _mm256_shuffle_epi8(in, swap);
    }

    __m256i high_byte_mask = _mm256_set1_epi16((int16_t)0xFF00);
    if (_mm256_testz_si256(in, high_byte_mask)) {
      // Pack 16-bit characters into 8-bit and store in latin1_output
      __m128i lo = _mm256_extractf128_si256(in, 0);
      __m128i hi = _mm256_extractf128_si256(in, 1);
      __m128i latin1_packed_lo = _mm_packus_epi16(lo, lo);
      __m128i latin1_packed_hi = _mm_packus_epi16(hi, hi);
      _mm_storel_epi64(reinterpret_cast<__m128i *>(latin1_output),
                       latin1_packed_lo);
      _mm_storel_epi64(reinterpret_cast<__m128i *>(latin1_output + 8),
                       latin1_packed_hi);
      // Adjust pointers for next iteration
      buf += 16;
      latin1_output += 16;
    } else {
      return std::make_pair(nullptr, reinterpret_cast<char *>(latin1_output));
    }
  } // while
  return std::make_pair(buf, latin1_output);
}

template <endianness big_endian>
std::pair<result, char *>
avx2_convert_utf16_to_latin1_with_errors(const char16_t *buf, size_t len,
                                         char *latin1_output) {
  const char16_t *start = buf;
  const char16_t *end = buf + len;
  while (end - buf >= 16) {
    __m256i in = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(buf));

    if (!match_system(big_endian)) {
      const __m256i swap = _mm256_setr_epi8(
          1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14, 17, 16, 19, 18,
          21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30);
      in = _mm256_shuffle_epi8(in, swap);
    }

    __m256i high_byte_mask = _mm256_set1_epi16((int16_t)0xFF00);
    if (_mm256_testz_si256(in, high_byte_mask)) {
      __m128i lo = _mm256_extractf128_si256(in, 0);
      __m128i hi = _mm256_extractf128_si256(in, 1);
      __m128i latin1_packed_lo = _mm_packus_epi16(lo, lo);
      __m128i latin1_packed_hi = _mm_packus_epi16(hi, hi);
      _mm_storel_epi64(reinterpret_cast<__m128i *>(latin1_output),
                       latin1_packed_lo);
      _mm_storel_epi64(reinterpret_cast<__m128i *>(latin1_output + 8),
                       latin1_packed_hi);
      buf += 16;
      latin1_output += 16;
    } else {
      // Fallback to scalar code for handling errors
      for (int k = 0; k < 16; k++) {
        uint16_t word = !match_system(big_endian)
                            ? scalar::utf16::swap_bytes(buf[k])
                            : buf[k];
        if (word <= 0xff) {
          *latin1_output++ = char(word);
        } else {
          return std::make_pair(
              result{error_code::TOO_LARGE, (size_t)(buf - start + k)},
              latin1_output);
        }
      }
      buf += 16;
    }
  } // while
  return std::make_pair(result{error_code::SUCCESS, (size_t)(buf - start)},
                        latin1_output);
}
