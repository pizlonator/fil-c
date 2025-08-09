template <endianness big_endian>
std::pair<const char32_t*, char16_t*> avx2_convert_utf32_to_utf16(const char32_t* buf, size_t len, char16_t* utf16_output) {
  const char32_t* end = buf + len;

  const size_t safety_margin = 12; // to avoid overruns, see issue https://github.com/simdutf/simdutf/issues/92
  __m256i forbidden_bytemask = _mm256_setzero_si256();


  while (end - buf >= std::ptrdiff_t(8 + safety_margin)) {
    __m256i in = _mm256_loadu_si256((__m256i*)buf);

    const __m256i v_00000000 = _mm256_setzero_si256();
    const __m256i v_ffff0000 = _mm256_set1_epi32((int32_t)0xffff0000);

    // no bits set above 16th bit <=> can pack to UTF16 without surrogate pairs
    const __m256i saturation_bytemask = _mm256_cmpeq_epi32(_mm256_and_si256(in, v_ffff0000), v_00000000);
    const uint32_t saturation_bitmask = static_cast<uint32_t>(_mm256_movemask_epi8(saturation_bytemask));

    if (saturation_bitmask == 0xffffffff) {
      const __m256i v_f800 = _mm256_set1_epi32((uint32_t)0xf800);
      const __m256i v_d800 = _mm256_set1_epi32((uint32_t)0xd800);
      forbidden_bytemask = _mm256_or_si256(forbidden_bytemask, _mm256_cmpeq_epi32(_mm256_and_si256(in, v_f800), v_d800));

      __m128i utf16_packed = _mm_packus_epi32(_mm256_castsi256_si128(in),_mm256_extractf128_si256(in,1));
      if (big_endian) {
        const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
        utf16_packed = _mm_shuffle_epi8(utf16_packed, swap);
      }
      _mm_storeu_si128((__m128i*)utf16_output, utf16_packed);
      utf16_output += 8;
      buf += 8;
    } else {
      size_t forward = 7;
      size_t k = 0;
      if(size_t(end - buf) < forward + 1) { forward = size_t(end - buf - 1);}
      for(; k < forward; k++) {
        uint32_t word = buf[k];
        if((word & 0xFFFF0000)==0) {
          // will not generate a surrogate pair
          if (word >= 0xD800 && word <= 0xDFFF) { return std::make_pair(nullptr, utf16_output); }
          *utf16_output++ = big_endian ? char16_t((uint16_t(word) >> 8) | (uint16_t(word) << 8)) : char16_t(word);
        } else {
          // will generate a surrogate pair
          if (word > 0x10FFFF) { return std::make_pair(nullptr, utf16_output); }
          word -= 0x10000;
          uint16_t high_surrogate = uint16_t(0xD800 + (word >> 10));
          uint16_t low_surrogate = uint16_t(0xDC00 + (word & 0x3FF));
          if (big_endian) {
            high_surrogate = uint16_t((high_surrogate >> 8) | (high_surrogate << 8));
            low_surrogate = uint16_t((low_surrogate >> 8) | (low_surrogate << 8));
          }
          *utf16_output++ = char16_t(high_surrogate);
          *utf16_output++ = char16_t(low_surrogate);
        }
      }
      buf += k;
    }
  }

  // check for invalid input
  if (static_cast<uint32_t>(_mm256_movemask_epi8(forbidden_bytemask)) != 0) { return std::make_pair(nullptr, utf16_output); }

  return std::make_pair(buf, utf16_output);
}


template <endianness big_endian>
std::pair<result, char16_t*> avx2_convert_utf32_to_utf16_with_errors(const char32_t* buf, size_t len, char16_t* utf16_output) {
  const char32_t* start = buf;
  const char32_t* end = buf + len;

  const size_t safety_margin = 12; // to avoid overruns, see issue https://github.com/simdutf/simdutf/issues/92

  while (end - buf >= std::ptrdiff_t(8 + safety_margin)) {
    __m256i in = _mm256_loadu_si256((__m256i*)buf);

    const __m256i v_00000000 = _mm256_setzero_si256();
    const __m256i v_ffff0000 = _mm256_set1_epi32((int32_t)0xffff0000);

    // no bits set above 16th bit <=> can pack to UTF16 without surrogate pairs
    const __m256i saturation_bytemask = _mm256_cmpeq_epi32(_mm256_and_si256(in, v_ffff0000), v_00000000);
    const uint32_t saturation_bitmask = static_cast<uint32_t>(_mm256_movemask_epi8(saturation_bytemask));

    if (saturation_bitmask == 0xffffffff) {
      const __m256i v_f800 = _mm256_set1_epi32((uint32_t)0xf800);
      const __m256i v_d800 = _mm256_set1_epi32((uint32_t)0xd800);
      const __m256i forbidden_bytemask = _mm256_cmpeq_epi32(_mm256_and_si256(in, v_f800), v_d800);
      if (static_cast<uint32_t>(_mm256_movemask_epi8(forbidden_bytemask)) != 0x0) {
        return std::make_pair(result(error_code::SURROGATE, buf - start), utf16_output);
      }

      __m128i utf16_packed = _mm_packus_epi32(_mm256_castsi256_si128(in),_mm256_extractf128_si256(in,1));
      if (big_endian) {
        const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
        utf16_packed = _mm_shuffle_epi8(utf16_packed, swap);
      }
      _mm_storeu_si128((__m128i*)utf16_output, utf16_packed);
      utf16_output += 8;
      buf += 8;
    } else {
      size_t forward = 7;
      size_t k = 0;
      if(size_t(end - buf) < forward + 1) { forward = size_t(end - buf - 1);}
      for(; k < forward; k++) {
        uint32_t word = buf[k];
        if((word & 0xFFFF0000)==0) {
          // will not generate a surrogate pair
          if (word >= 0xD800 && word <= 0xDFFF) { return std::make_pair(result(error_code::SURROGATE, buf - start + k), utf16_output); }
          *utf16_output++ = big_endian ? char16_t((uint16_t(word) >> 8) | (uint16_t(word) << 8)) : char16_t(word);
        } else {
          // will generate a surrogate pair
          if (word > 0x10FFFF) { return std::make_pair(result(error_code::TOO_LARGE, buf - start + k), utf16_output); }
          word -= 0x10000;
          uint16_t high_surrogate = uint16_t(0xD800 + (word >> 10));
          uint16_t low_surrogate = uint16_t(0xDC00 + (word & 0x3FF));
          if (big_endian) {
            high_surrogate = uint16_t((high_surrogate >> 8) | (high_surrogate << 8));
            low_surrogate = uint16_t((low_surrogate >> 8) | (low_surrogate << 8));
          }
          *utf16_output++ = char16_t(high_surrogate);
          *utf16_output++ = char16_t(low_surrogate);
        }
      }
      buf += k;
    }
  }

  return std::make_pair(result(error_code::SUCCESS, buf - start), utf16_output);
}