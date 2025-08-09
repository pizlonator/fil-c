std::pair<const char32_t *, char *>
avx2_convert_utf32_to_latin1(const char32_t *buf, size_t len,
                             char *latin1_output) {
  const size_t rounded_len =
  len & ~0x1F; // Round down to nearest multiple of 32

  __m256i high_bytes_mask = _mm256_set1_epi32(0xFFFFFF00);

  __m256i shufmask = _mm256_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                    -1, 12, 8, 4, 0, -1, -1, -1, -1, -1, -1,
                                    -1, -1, -1, -1, -1, -1, 12, 8, 4, 0);

  for (size_t i = 0; i < rounded_len; i += 16) {
    __m256i in1 = _mm256_loadu_si256((__m256i *)buf);
    __m256i in2 = _mm256_loadu_si256((__m256i *)(buf + 8));

    __m256i check_combined = _mm256_or_si256(in1, in2);

    if (!_mm256_testz_si256(check_combined, high_bytes_mask)) {
      return std::make_pair(nullptr, latin1_output);
    }

    //Turn UTF32 bytes into latin 1 bytes
    __m256i shuffled1 = _mm256_shuffle_epi8(in1, shufmask);
    __m256i shuffled2 = _mm256_shuffle_epi8(in2, shufmask);

    //move Latin1 bytes to their correct spot
    __m256i idx1 = _mm256_set_epi32(-1, -1,-1,-1,-1,-1,4,0);
    __m256i idx2 = _mm256_set_epi32(-1, -1,-1,-1,4,0,-1,-1);
    __m256i reshuffled1 = _mm256_permutevar8x32_epi32(shuffled1, idx1);
    __m256i reshuffled2 = _mm256_permutevar8x32_epi32(shuffled2, idx2);

    __m256i result = _mm256_or_si256(reshuffled1, reshuffled2);
    _mm_storeu_si128((__m128i *)latin1_output,
                     _mm256_castsi256_si128(result));

    latin1_output += 16;
    buf += 16;
  }

  return std::make_pair(buf, latin1_output);
}
std::pair<result, char *>
avx2_convert_utf32_to_latin1_with_errors(const char32_t *buf, size_t len,
                                         char *latin1_output) {
    const size_t rounded_len =
        len & ~0x1F; // Round down to nearest multiple of 32

    __m256i high_bytes_mask = _mm256_set1_epi32(0xFFFFFF00);
    __m256i shufmask = _mm256_set_epi8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                       -1, 12, 8, 4, 0, -1, -1, -1, -1, -1, -1,
                                       -1, -1, -1, -1, -1, -1, 12, 8, 4, 0);

    const char32_t *start = buf;

    for (size_t i = 0; i < rounded_len; i += 16) {
        __m256i in1 = _mm256_loadu_si256((__m256i *)buf);
        __m256i in2 = _mm256_loadu_si256((__m256i *)(buf + 8));

        __m256i check_combined = _mm256_or_si256(in1, in2);

        if (!_mm256_testz_si256(check_combined, high_bytes_mask)) {
            // Fallback to scalar code for handling errors
            for (int k = 0; k < 8; k++) {
                char32_t codepoint = buf[k];
                if (codepoint <= 0xFF) {
                    *latin1_output++ = static_cast<char>(codepoint);
                } else {
                    return std::make_pair(result(error_code::TOO_LARGE, buf - start + k),
                                          latin1_output);
                }
            }
            buf += 8;
        } else {
            __m256i shuffled1 = _mm256_shuffle_epi8(in1, shufmask);
            __m256i shuffled2 = _mm256_shuffle_epi8(in2, shufmask);

            __m256i idx1 = _mm256_set_epi32(-1, -1, -1, -1, -1, -1, 4, 0);
            __m256i idx2 = _mm256_set_epi32(-1, -1, -1, -1, 4, 0, -1, -1);
            __m256i reshuffled1 = _mm256_permutevar8x32_epi32(shuffled1, idx1);
            __m256i reshuffled2 = _mm256_permutevar8x32_epi32(shuffled2, idx2);

            __m256i result = _mm256_or_si256(reshuffled1, reshuffled2);
            _mm_storeu_si128((__m128i *)latin1_output, _mm256_castsi256_si128(result));

            latin1_output += 16;
            buf += 16;
        }
    }

    return std::make_pair(result(error_code::SUCCESS, buf - start), latin1_output);
}