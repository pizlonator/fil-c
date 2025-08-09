/*
    The vectorized algorithm works on single SSE register i.e., it
    loads eight 16-bit code units.

    We consider three cases:
    1. an input register contains no surrogates and each value
       is in range 0x0000 .. 0x07ff.
    2. an input register contains no surrogates and values are
       in range 0x0000 .. 0xffff.
    3. an input register contains surrogates --- i.e. codepoints
       can have 16 or 32 bits.

    Ad 1.

    When values are less than 0x0800, it means that a 16-bit code unit
    can be converted into: 1) single UTF8 byte (when it is an ASCII
    char) or 2) two UTF8 bytes.

    For this case we do only some shuffle to obtain these 2-byte
    codes and finally compress the whole SSE register with a single
    shuffle.

    We need 256-entry lookup table to get a compression pattern
    and the number of output bytes in the compressed vector register.
    Each entry occupies 17 bytes.

    Ad 2.

    When values fit in 16-bit code units, but are above 0x07ff, then
    a single word may produce one, two or three UTF8 bytes.

    We prepare data for all these three cases in two registers.
    The first register contains lower two UTF8 bytes (used in all
    cases), while the second one contains just the third byte for
    the three-UTF8-bytes case.

    Finally these two registers are interleaved forming eight-element
    array of 32-bit values. The array spans two SSE registers.
    The bytes from the registers are compressed using two shuffles.

    We need 256-entry lookup table to get a compression pattern
    and the number of output bytes in the compressed vector register.
    Each entry occupies 17 bytes.


    To summarize:
    - We need two 256-entry tables that have 8704 bytes in total.
*/


/*
  Returns a pair: the first unprocessed byte from buf and utf32_output
  A scalar routing should carry on the conversion of the tail.
*/
template <endianness big_endian>
std::pair<const char16_t*, char32_t*> avx2_convert_utf16_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) {
  const char16_t* end = buf + len;
  const __m256i v_f800 = _mm256_set1_epi16((int16_t)0xf800);
  const __m256i v_d800 = _mm256_set1_epi16((int16_t)0xd800);

  while (end - buf >= 16) {
    __m256i in = _mm256_loadu_si256((__m256i*)buf);
    if (big_endian) {
      const __m256i swap = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
                                  17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30);
      in = _mm256_shuffle_epi8(in, swap);
    }

    // 1. Check if there are any surrogate word in the input chunk.
    //    We have also deal with situation when there is a surrogate word
    //    at the end of a chunk.
    const __m256i surrogates_bytemask = _mm256_cmpeq_epi16(_mm256_and_si256(in, v_f800), v_d800);

    // bitmask = 0x0000 if there are no surrogates
    //         = 0xc000 if the last word is a surrogate
    const uint32_t surrogates_bitmask = static_cast<uint32_t>(_mm256_movemask_epi8(surrogates_bytemask));
    // It might seem like checking for surrogates_bitmask == 0xc000 could help. However,
    // it is likely an uncommon occurrence.
    if (surrogates_bitmask == 0x00000000) {
      // case: we extend all sixteen 16-bit code units to sixteen 32-bit code units
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(utf32_output), _mm256_cvtepu16_epi32(_mm256_castsi256_si128(in)));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(utf32_output + 8), _mm256_cvtepu16_epi32(_mm256_extractf128_si256(in,1)));
        utf32_output += 16;
        buf += 16;
    // surrogate pair(s) in a register
    } else {
      // Let us do a scalar fallback.
      // It may seem wasteful to use scalar code, but being efficient with SIMD
      // in the presence of surrogate pairs may require non-trivial tables.
      size_t forward = 15;
      size_t k = 0;
      if(size_t(end - buf) < forward + 1) { forward = size_t(end - buf - 1);}
      for(; k < forward; k++) {
        uint16_t word = big_endian ? scalar::utf16::swap_bytes(buf[k]) : buf[k];
        if((word &0xF800 ) != 0xD800) {
          // No surrogate pair
          *utf32_output++ = char32_t(word);
        } else {
          // must be a surrogate pair
          uint16_t diff = uint16_t(word - 0xD800);
          uint16_t next_word = big_endian ? scalar::utf16::swap_bytes(buf[k+1]) : buf[k+1];
          k++;
          uint16_t diff2 = uint16_t(next_word - 0xDC00);
          if((diff | diff2) > 0x3FF)  { return std::make_pair(nullptr, utf32_output); }
          uint32_t value = (diff << 10) + diff2 + 0x10000;
          *utf32_output++ = char32_t(value);
        }
      }
      buf += k;
    }
  } // while
  return std::make_pair(buf, utf32_output);
}


/*
  Returns a pair: a result struct and utf8_output.
  If there is an error, the count field of the result is the position of the error.
  Otherwise, it is the position of the first unprocessed byte in buf (even if finished).
  A scalar routing should carry on the conversion of the tail if needed.
*/
template <endianness big_endian>
std::pair<result, char32_t*> avx2_convert_utf16_to_utf32_with_errors(const char16_t* buf, size_t len, char32_t* utf32_output) {
  const char16_t* start = buf;
  const char16_t* end = buf + len;
  const __m256i v_f800 = _mm256_set1_epi16((int16_t)0xf800);
  const __m256i v_d800 = _mm256_set1_epi16((int16_t)0xd800);

  while (end - buf >= 16) {
    __m256i in = _mm256_loadu_si256((__m256i*)buf);
    if (big_endian) {
      const __m256i swap = _mm256_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
                                  17, 16, 19, 18, 21, 20, 23, 22, 25, 24, 27, 26, 29, 28, 31, 30);
      in = _mm256_shuffle_epi8(in, swap);
    }

    // 1. Check if there are any surrogate word in the input chunk.
    //    We have also deal with situation when there is a surrogate word
    //    at the end of a chunk.
    const __m256i surrogates_bytemask = _mm256_cmpeq_epi16(_mm256_and_si256(in, v_f800), v_d800);

    // bitmask = 0x0000 if there are no surrogates
    //         = 0xc000 if the last word is a surrogate
    const uint32_t surrogates_bitmask = static_cast<uint32_t>(_mm256_movemask_epi8(surrogates_bytemask));
    // It might seem like checking for surrogates_bitmask == 0xc000 could help. However,
    // it is likely an uncommon occurrence.
    if (surrogates_bitmask == 0x00000000) {
      // case: we extend all sixteen 16-bit code units to sixteen 32-bit code units
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(utf32_output), _mm256_cvtepu16_epi32(_mm256_castsi256_si128(in)));
        _mm256_storeu_si256(reinterpret_cast<__m256i *>(utf32_output + 8), _mm256_cvtepu16_epi32(_mm256_extractf128_si256(in,1)));
        utf32_output += 16;
        buf += 16;
    // surrogate pair(s) in a register
    } else {
      // Let us do a scalar fallback.
      // It may seem wasteful to use scalar code, but being efficient with SIMD
      // in the presence of surrogate pairs may require non-trivial tables.
      size_t forward = 15;
      size_t k = 0;
      if(size_t(end - buf) < forward + 1) { forward = size_t(end - buf - 1);}
      for(; k < forward; k++) {
        uint16_t word = big_endian ? scalar::utf16::swap_bytes(buf[k]) : buf[k];
        if((word &0xF800 ) != 0xD800) {
          // No surrogate pair
          *utf32_output++ = char32_t(word);
        } else {
          // must be a surrogate pair
          uint16_t diff = uint16_t(word - 0xD800);
          uint16_t next_word = big_endian ? scalar::utf16::swap_bytes(buf[k+1]) : buf[k+1];
          k++;
          uint16_t diff2 = uint16_t(next_word - 0xDC00);
          if((diff | diff2) > 0x3FF)  { return std::make_pair(result(error_code::SURROGATE, buf - start + k - 1), utf32_output); }
          uint32_t value = (diff << 10) + diff2 + 0x10000;
          *utf32_output++ = char32_t(value);
        }
      }
      buf += k;
    }
  } // while
  return std::make_pair(result(error_code::SUCCESS, buf - start), utf32_output);
}
