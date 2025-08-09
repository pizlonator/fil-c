template <endianness big_endian>
std::pair<const char32_t*, char16_t*> sse_convert_utf32_to_utf16(const char32_t* buf, size_t len, char16_t* utf16_output) {

  const char32_t* end = buf + len;

  const __m128i v_0000 = _mm_setzero_si128();
  const __m128i v_ffff0000 = _mm_set1_epi32((int32_t)0xffff0000);
  __m128i forbidden_bytemask = _mm_setzero_si128();

  while (end - buf >= 8) {
    __m128i in = _mm_loadu_si128((__m128i*)buf);
    __m128i nextin = _mm_loadu_si128((__m128i*)buf+1);
    const __m128i saturation_bytemask = _mm_cmpeq_epi32(_mm_and_si128(_mm_or_si128(in, nextin), v_ffff0000), v_0000);
    const uint32_t saturation_bitmask = static_cast<uint32_t>(_mm_movemask_epi8(saturation_bytemask));

    // Check if no bits set above 16th
    if (saturation_bitmask == 0xffff) {
      // Pack UTF-32 to UTF-16
      __m128i utf16_packed = _mm_packus_epi32(in, nextin);

      const __m128i v_f800 = _mm_set1_epi16((uint16_t)0xf800);
      const __m128i v_d800 = _mm_set1_epi16((uint16_t)0xd800);
      forbidden_bytemask = _mm_or_si128(forbidden_bytemask, _mm_cmpeq_epi16(_mm_and_si128(utf16_packed, v_f800), v_d800));

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
  if (static_cast<uint32_t>(_mm_movemask_epi8(forbidden_bytemask)) != 0) { return std::make_pair(nullptr, utf16_output); }

  return std::make_pair(buf, utf16_output);
}


template <endianness big_endian>
std::pair<result, char16_t*> sse_convert_utf32_to_utf16_with_errors(const char32_t* buf, size_t len, char16_t* utf16_output) {
  const char32_t* start = buf;
  const char32_t* end = buf + len;

  const __m128i v_0000 = _mm_setzero_si128();
  const __m128i v_ffff0000 = _mm_set1_epi32((int32_t)0xffff0000);

  while (end - buf >= 8) {
    __m128i in = _mm_loadu_si128((__m128i*)buf);
    __m128i nextin = _mm_loadu_si128((__m128i*)buf+1);
    const __m128i saturation_bytemask = _mm_cmpeq_epi32(_mm_and_si128(_mm_or_si128(in, nextin), v_ffff0000), v_0000);
    const uint32_t saturation_bitmask = static_cast<uint32_t>(_mm_movemask_epi8(saturation_bytemask));

    // Check if no bits set above 16th
    if (saturation_bitmask == 0xffff) {
      // Pack UTF-32 to UTF-16
      __m128i utf16_packed = _mm_packus_epi32(in, nextin);

      const __m128i v_f800 = _mm_set1_epi16((uint16_t)0xf800);
      const __m128i v_d800 = _mm_set1_epi16((uint16_t)0xd800);
      const __m128i forbidden_bytemask = _mm_cmpeq_epi16(_mm_and_si128(utf16_packed, v_f800), v_d800);
      if (static_cast<uint32_t>(_mm_movemask_epi8(forbidden_bytemask)) != 0) {
        return std::make_pair(result(error_code::SURROGATE, buf - start), utf16_output);
      }

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