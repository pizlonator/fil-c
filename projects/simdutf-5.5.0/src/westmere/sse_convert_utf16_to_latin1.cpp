template <endianness big_endian>
std::pair<const char16_t*, char*> sse_convert_utf16_to_latin1(const char16_t* buf, size_t len, char* latin1_output) {
  const char16_t* end = buf + len;
  while (end - buf >= 8) {
    // Load 8 UTF-16 characters into 128-bit SSE register
    __m128i in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(buf));

    if (!match_system(big_endian)) {
      const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
      in = _mm_shuffle_epi8(in, swap);
    }

    __m128i high_byte_mask = _mm_set1_epi16((int16_t)0xFF00);
    if (_mm_testz_si128(in, high_byte_mask)) {
      // Pack 16-bit characters into 8-bit and store in latin1_output
      __m128i latin1_packed = _mm_packus_epi16(in, in);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(latin1_output), latin1_packed);
      // Adjust pointers for next iteration
      buf += 8;
      latin1_output += 8;
    } else {
      return std::make_pair(nullptr, reinterpret_cast<char*>(latin1_output));
    }
  } // while
  return std::make_pair(buf, latin1_output);
}

template <endianness big_endian>
std::pair<result, char*> sse_convert_utf16_to_latin1_with_errors(const char16_t* buf, size_t len, char* latin1_output) {
  const char16_t* start = buf;
  const char16_t* end = buf + len;
  while (end - buf >= 8) {
    __m128i in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(buf));

    if (!match_system(big_endian)) {
      const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
      in = _mm_shuffle_epi8(in, swap);
    }

    __m128i high_byte_mask = _mm_set1_epi16((int16_t)0xFF00);
    if (_mm_testz_si128(in, high_byte_mask)) {
      __m128i latin1_packed = _mm_packus_epi16(in, in);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(latin1_output), latin1_packed);
      buf += 8;
      latin1_output += 8;
    } else {
      // Fallback to scalar code for handling errors
      for(int k = 0; k < 8; k++) {
        uint16_t word = !match_system(big_endian) ? scalar::utf16::swap_bytes(buf[k]) : buf[k];
        if(word <= 0xff) {
          *latin1_output++ = char(word);
        } else {
          return std::make_pair(result(error_code::TOO_LARGE, buf - start + k), latin1_output);
        }
      }
      buf += 8;
    }
  } // while
  return std::make_pair(result(error_code::SUCCESS, buf - start), latin1_output);
}