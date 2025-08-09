std::pair<const char *, char *> avx2_convert_latin1_to_utf8(const char *latin1_input, size_t len,
                           char *utf8_output) {
  const char *end = latin1_input + len;
  const __m256i v_0000 = _mm256_setzero_si256();
  const __m256i v_c080 = _mm256_set1_epi16((int16_t)0xc080);
  const __m256i v_ff80 = _mm256_set1_epi16((int16_t)0xff80);
  const size_t safety_margin = 12;

  while (end - latin1_input >= std::ptrdiff_t(16 + safety_margin)) {
    __m128i in8 = _mm_loadu_si128((__m128i *)latin1_input);
    // a single 16-bit UTF-16 word can yield 1, 2 or 3 UTF-8 bytes
    const __m128i v_80 = _mm_set1_epi8((char)0x80);
    if (_mm_testz_si128(in8, v_80)) { // ASCII fast path!!!!
      // 1. store (16 bytes)
      _mm_storeu_si128((__m128i *)utf8_output, in8);
      // 2. adjust pointers
      latin1_input += 16;
      utf8_output += 16;
      continue; // we are done for this round!
    }
    // We proceed only with the first 16 bytes.
    const __m256i in = _mm256_cvtepu8_epi16((in8));

    // 1. prepare 2-byte values
    // input 16-bit word : [0000|0000|aabb|bbbb] x 8
    // expected output   : [1100|00aa|10bb|bbbb] x 8
    const __m256i v_1f00 = _mm256_set1_epi16((int16_t)0x1f00);
    const __m256i v_003f = _mm256_set1_epi16((int16_t)0x003f);

    // t0 = [0000|00aa|bbbb|bb00]
    const __m256i t0 = _mm256_slli_epi16(in, 2);
    // t1 = [0000|00aa|0000|0000]
    const __m256i t1 = _mm256_and_si256(t0, v_1f00);
    // t2 = [0000|0000|00bb|bbbb]
    const __m256i t2 = _mm256_and_si256(in, v_003f);
    // t3 = [000a|aaaa|00bb|bbbb]
    const __m256i t3 = _mm256_or_si256(t1, t2);
    // t4 = [1100|00aa|10bb|bbbb]
    const __m256i t4 = _mm256_or_si256(t3, v_c080);

    // 2. merge ASCII and 2-byte codewords

    // no bits set above 7th bit
    const __m256i one_byte_bytemask = _mm256_cmpeq_epi16(_mm256_and_si256(in, v_ff80), v_0000);
    const uint32_t one_byte_bitmask = static_cast<uint32_t>(_mm256_movemask_epi8(one_byte_bytemask));

    const __m256i utf8_unpacked = _mm256_blendv_epi8(t4, in, one_byte_bytemask);

    // 3. prepare bitmask for 8-bit lookup
    const uint32_t M0 = one_byte_bitmask & 0x55555555;
    const uint32_t M1 = M0 >> 7;
    const uint32_t M2 = (M1 | M0) & 0x00ff00ff;
    // 4. pack the bytes

    const uint8_t *row =
        &simdutf::tables::utf16_to_utf8::pack_1_2_utf8_bytes[uint8_t(M2)][0];
    const uint8_t *row_2 =
        &simdutf::tables::utf16_to_utf8::pack_1_2_utf8_bytes[uint8_t(M2 >> 16)]
                                                            [0];

    const __m128i shuffle = _mm_loadu_si128((__m128i *)(row + 1));
    const __m128i shuffle_2 = _mm_loadu_si128((__m128i *)(row_2 + 1));

    const __m256i utf8_packed = _mm256_shuffle_epi8(
        utf8_unpacked, _mm256_setr_m128i(shuffle, shuffle_2));
    // 5. store bytes
    _mm_storeu_si128((__m128i *)utf8_output,
                     _mm256_castsi256_si128(utf8_packed));
    utf8_output += row[0];
    _mm_storeu_si128((__m128i *)utf8_output,
                     _mm256_extractf128_si256(utf8_packed, 1));
    utf8_output += row_2[0];

    // 6. adjust pointers
    latin1_input += 16;
    continue;

  } // while
  return std::make_pair(latin1_input, utf8_output);
}
