/*
    The vectorized algorithm works on single SSE register i.e., it
    loads eight 16-bit code units.

    We consider three cases:
    1. an input register contains no surrogates and each value
       is in range 0x0000 .. 0x07ff.
    2. an input register contains no surrogates and values are
       is in range 0x0000 .. 0xffff.
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
  Returns a pair: the first unprocessed byte from buf and utf8_output
  A scalar routing should carry on the conversion of the tail.
*/
template <endianness big_endian>
std::pair<const char16_t*, char*> sse_convert_utf16_to_utf8(const char16_t* buf, size_t len, char* utf8_output) {

  const char16_t* end = buf + len;

  const __m128i v_0000 = _mm_setzero_si128();
  const __m128i v_f800 = _mm_set1_epi16((int16_t)0xf800);
  const __m128i v_d800 = _mm_set1_epi16((int16_t)0xd800);
  const size_t safety_margin = 12; // to avoid overruns, see issue https://github.com/simdutf/simdutf/issues/92

  while (end - buf >= std::ptrdiff_t(16 + safety_margin)) {
    __m128i in = _mm_loadu_si128((__m128i*)buf);
    if (big_endian) {
      const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
      in = _mm_shuffle_epi8(in, swap);
    }
    // a single 16-bit UTF-16 word can yield 1, 2 or 3 UTF-8 bytes
    const __m128i v_ff80 = _mm_set1_epi16((int16_t)0xff80);
    if(_mm_testz_si128(in, v_ff80)) { // ASCII fast path!!!!
        __m128i nextin = _mm_loadu_si128((__m128i*)buf+1);
        if (big_endian) {
          const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
          nextin = _mm_shuffle_epi8(nextin, swap);
        }
        if(!_mm_testz_si128(nextin, v_ff80)) {
          // 1. pack the bytes
          // obviously suboptimal.
          const __m128i utf8_packed = _mm_packus_epi16(in,in);
          // 2. store (16 bytes)
          _mm_storeu_si128((__m128i*)utf8_output, utf8_packed);
          // 3. adjust pointers
          buf += 8;
          utf8_output += 8;
          in = nextin;
        } else {
          // 1. pack the bytes
          // obviously suboptimal.
          const __m128i utf8_packed = _mm_packus_epi16(in,nextin);
          // 2. store (16 bytes)
          _mm_storeu_si128((__m128i*)utf8_output, utf8_packed);
          // 3. adjust pointers
          buf += 16;
          utf8_output += 16;
          continue; // we are done for this round!
        }
    }

    // no bits set above 7th bit
    const __m128i one_byte_bytemask = _mm_cmpeq_epi16(_mm_and_si128(in, v_ff80), v_0000);
    const uint16_t one_byte_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(one_byte_bytemask));

    // no bits set above 11th bit
    const __m128i one_or_two_bytes_bytemask = _mm_cmpeq_epi16(_mm_and_si128(in, v_f800), v_0000);
    const uint16_t one_or_two_bytes_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(one_or_two_bytes_bytemask));

    if (one_or_two_bytes_bitmask == 0xffff) {
      internal::westmere::write_v_u16_11bits_to_utf8(in, utf8_output, one_byte_bytemask, one_byte_bitmask);
      buf += 8;
      continue;
    }

    // 1. Check if there are any surrogate word in the input chunk.
    //    We have also deal with situation when there is a surrogate word
    //    at the end of a chunk.
    const __m128i surrogates_bytemask = _mm_cmpeq_epi16(_mm_and_si128(in, v_f800), v_d800);

    // bitmask = 0x0000 if there are no surrogates
    //         = 0xc000 if the last word is a surrogate
    const uint16_t surrogates_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(surrogates_bytemask));
    // It might seem like checking for surrogates_bitmask == 0xc000 could help. However,
    // it is likely an uncommon occurrence.
    if (surrogates_bitmask == 0x0000) {
      // case: code units from register produce either 1, 2 or 3 UTF-8 bytes
        const __m128i dup_even = _mm_setr_epi16(0x0000, 0x0202, 0x0404, 0x0606,
                                                0x0808, 0x0a0a, 0x0c0c, 0x0e0e);

        /* In this branch we handle three cases:
           1. [0000|0000|0ccc|cccc] => [0ccc|cccc]                           - single UFT-8 byte
           2. [0000|0bbb|bbcc|cccc] => [110b|bbbb], [10cc|cccc]              - two UTF-8 bytes
           3. [aaaa|bbbb|bbcc|cccc] => [1110|aaaa], [10bb|bbbb], [10cc|cccc] - three UTF-8 bytes

          We expand the input word (16-bit) into two code units (32-bit), thus
          we have room for four bytes. However, we need five distinct bit
          layouts. Note that the last byte in cases #2 and #3 is the same.

          We precompute byte 1 for case #1 and the common byte for cases #2 & #3
          in register t2.

          We precompute byte 1 for case #3 and -- **conditionally** -- precompute
          either byte 1 for case #2 or byte 2 for case #3. Note that they
          differ by exactly one bit.

          Finally from these two code units we build proper UTF-8 sequence, taking
          into account the case (i.e, the number of bytes to write).
        */
        /**
         * Given [aaaa|bbbb|bbcc|cccc] our goal is to produce:
         * t2 => [0ccc|cccc] [10cc|cccc]
         * s4 => [1110|aaaa] ([110b|bbbb] OR [10bb|bbbb])
         */
#define simdutf_vec(x) _mm_set1_epi16(static_cast<uint16_t>(x))
        // [aaaa|bbbb|bbcc|cccc] => [bbcc|cccc|bbcc|cccc]
        const __m128i t0 = _mm_shuffle_epi8(in, dup_even);
        // [bbcc|cccc|bbcc|cccc] => [00cc|cccc|0bcc|cccc]
        const __m128i t1 = _mm_and_si128(t0, simdutf_vec(0b0011111101111111));
        // [00cc|cccc|0bcc|cccc] => [10cc|cccc|0bcc|cccc]
        const __m128i t2 = _mm_or_si128 (t1, simdutf_vec(0b1000000000000000));

        // [aaaa|bbbb|bbcc|cccc] =>  [0000|aaaa|bbbb|bbcc]
        const __m128i s0 = _mm_srli_epi16(in, 4);
        // [0000|aaaa|bbbb|bbcc] => [0000|aaaa|bbbb|bb00]
        const __m128i s1 = _mm_and_si128(s0, simdutf_vec(0b0000111111111100));
        // [0000|aaaa|bbbb|bb00] => [00bb|bbbb|0000|aaaa]
        const __m128i s2 = _mm_maddubs_epi16(s1, simdutf_vec(0x0140));
        // [00bb|bbbb|0000|aaaa] => [11bb|bbbb|1110|aaaa]
        const __m128i s3 = _mm_or_si128(s2, simdutf_vec(0b1100000011100000));
        const __m128i m0 = _mm_andnot_si128(one_or_two_bytes_bytemask, simdutf_vec(0b0100000000000000));
        const __m128i s4 = _mm_xor_si128(s3, m0);
#undef simdutf_vec

        // 4. expand code units 16-bit => 32-bit
        const __m128i out0 = _mm_unpacklo_epi16(t2, s4);
        const __m128i out1 = _mm_unpackhi_epi16(t2, s4);

        // 5. compress 32-bit code units into 1, 2 or 3 bytes -- 2 x shuffle
        const uint16_t mask = (one_byte_bitmask & 0x5555) |
                              (one_or_two_bytes_bitmask & 0xaaaa);
        if(mask == 0) {
          // We only have three-byte code units. Use fast path.
          const __m128i shuffle = _mm_setr_epi8(2,3,1,6,7,5,10,11,9,14,15,13,-1,-1,-1,-1);
          const __m128i utf8_0 = _mm_shuffle_epi8(out0, shuffle);
          const __m128i utf8_1 = _mm_shuffle_epi8(out1, shuffle);
          _mm_storeu_si128((__m128i*)utf8_output, utf8_0);
          utf8_output += 12;
          _mm_storeu_si128((__m128i*)utf8_output, utf8_1);
          utf8_output += 12;
          buf += 8;
          continue;
        }
        const uint8_t mask0 = uint8_t(mask);

        const uint8_t* row0 = &simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes[mask0][0];
        const __m128i shuffle0 = _mm_loadu_si128((__m128i*)(row0 + 1));
        const __m128i utf8_0 = _mm_shuffle_epi8(out0, shuffle0);

        const uint8_t mask1 = static_cast<uint8_t>(mask >> 8);

        const uint8_t* row1 = &simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes[mask1][0];
        const __m128i shuffle1 = _mm_loadu_si128((__m128i*)(row1 + 1));
        const __m128i utf8_1 = _mm_shuffle_epi8(out1, shuffle1);

        _mm_storeu_si128((__m128i*)utf8_output, utf8_0);
        utf8_output += row0[0];
        _mm_storeu_si128((__m128i*)utf8_output, utf8_1);
        utf8_output += row1[0];

        buf += 8;
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
        if((word & 0xFF80)==0) {
          *utf8_output++ = char(word);
        } else if((word & 0xF800)==0) {
          *utf8_output++ = char((word>>6) | 0b11000000);
          *utf8_output++ = char((word & 0b111111) | 0b10000000);
        } else if((word &0xF800 ) != 0xD800) {
          *utf8_output++ = char((word>>12) | 0b11100000);
          *utf8_output++ = char(((word>>6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((word & 0b111111) | 0b10000000);
        } else {
          // must be a surrogate pair
          uint16_t diff = uint16_t(word - 0xD800);
          uint16_t next_word = big_endian ? scalar::utf16::swap_bytes(buf[k+1]) : buf[k+1];
          k++;
          uint16_t diff2 = uint16_t(next_word - 0xDC00);
          if((diff | diff2) > 0x3FF)  { return std::make_pair(nullptr, utf8_output); }
          uint32_t value = (diff << 10) + diff2 + 0x10000;
          *utf8_output++ = char((value>>18) | 0b11110000);
          *utf8_output++ = char(((value>>12) & 0b111111) | 0b10000000);
          *utf8_output++ = char(((value>>6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((value & 0b111111) | 0b10000000);
        }
      }
      buf += k;
    }
  } // while

  return std::make_pair(buf, utf8_output);
}


/*
  Returns a pair: a result struct and utf8_output.
  If there is an error, the count field of the result is the position of the error.
  Otherwise, it is the position of the first unprocessed byte in buf (even if finished).
  A scalar routing should carry on the conversion of the tail if needed.
*/
template <endianness big_endian>
std::pair<result, char*> sse_convert_utf16_to_utf8_with_errors(const char16_t* buf, size_t len, char* utf8_output) {
  const char16_t* start = buf;
  const char16_t* end = buf + len;

  const __m128i v_0000 = _mm_setzero_si128();
  const __m128i v_f800 = _mm_set1_epi16((int16_t)0xf800);
  const __m128i v_d800 = _mm_set1_epi16((int16_t)0xd800);
  const size_t safety_margin = 12; // to avoid overruns, see issue https://github.com/simdutf/simdutf/issues/92

  while (end - buf >= std::ptrdiff_t(16 + safety_margin)) {
    __m128i in = _mm_loadu_si128((__m128i*)buf);
    if (big_endian) {
      const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
      in = _mm_shuffle_epi8(in, swap);
    }
    // a single 16-bit UTF-16 word can yield 1, 2 or 3 UTF-8 bytes
    const __m128i v_ff80 = _mm_set1_epi16((int16_t)0xff80);
    if(_mm_testz_si128(in, v_ff80)) { // ASCII fast path!!!!
        __m128i nextin = _mm_loadu_si128((__m128i*)buf+1);
        if (big_endian) {
          const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
          nextin = _mm_shuffle_epi8(nextin, swap);
        }
        if(!_mm_testz_si128(nextin, v_ff80)) {
          // 1. pack the bytes
          // obviously suboptimal.
          const __m128i utf8_packed = _mm_packus_epi16(in,in);
          // 2. store (16 bytes)
          _mm_storeu_si128((__m128i*)utf8_output, utf8_packed);
          // 3. adjust pointers
          buf += 8;
          utf8_output += 8;
          in = nextin;
        } else {
          // 1. pack the bytes
          // obviously suboptimal.
          const __m128i utf8_packed = _mm_packus_epi16(in,nextin);
          // 2. store (16 bytes)
          _mm_storeu_si128((__m128i*)utf8_output, utf8_packed);
          // 3. adjust pointers
          buf += 16;
          utf8_output += 16;
          continue; // we are done for this round!
        }
    }

    // no bits set above 7th bit
    const __m128i one_byte_bytemask = _mm_cmpeq_epi16(_mm_and_si128(in, v_ff80), v_0000);
    const uint16_t one_byte_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(one_byte_bytemask));

    // no bits set above 11th bit
    const __m128i one_or_two_bytes_bytemask = _mm_cmpeq_epi16(_mm_and_si128(in, v_f800), v_0000);
    const uint16_t one_or_two_bytes_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(one_or_two_bytes_bytemask));

    if (one_or_two_bytes_bitmask == 0xffff) {
      internal::westmere::write_v_u16_11bits_to_utf8(in, utf8_output, one_byte_bytemask, one_byte_bitmask);
      buf += 8;
      continue;
    }

    // 1. Check if there are any surrogate word in the input chunk.
    //    We have also deal with situation when there is a surrogate word
    //    at the end of a chunk.
    const __m128i surrogates_bytemask = _mm_cmpeq_epi16(_mm_and_si128(in, v_f800), v_d800);

    // bitmask = 0x0000 if there are no surrogates
    //         = 0xc000 if the last word is a surrogate
    const uint16_t surrogates_bitmask = static_cast<uint16_t>(_mm_movemask_epi8(surrogates_bytemask));
    // It might seem like checking for surrogates_bitmask == 0xc000 could help. However,
    // it is likely an uncommon occurrence.
    if (surrogates_bitmask == 0x0000) {
      // case: code units from register produce either 1, 2 or 3 UTF-8 bytes
        const __m128i dup_even = _mm_setr_epi16(0x0000, 0x0202, 0x0404, 0x0606,
                                                0x0808, 0x0a0a, 0x0c0c, 0x0e0e);

        /* In this branch we handle three cases:
           1. [0000|0000|0ccc|cccc] => [0ccc|cccc]                           - single UFT-8 byte
           2. [0000|0bbb|bbcc|cccc] => [110b|bbbb], [10cc|cccc]              - two UTF-8 bytes
           3. [aaaa|bbbb|bbcc|cccc] => [1110|aaaa], [10bb|bbbb], [10cc|cccc] - three UTF-8 bytes

          We expand the input word (16-bit) into two code units (32-bit), thus
          we have room for four bytes. However, we need five distinct bit
          layouts. Note that the last byte in cases #2 and #3 is the same.

          We precompute byte 1 for case #1 and the common byte for cases #2 & #3
          in register t2.

          We precompute byte 1 for case #3 and -- **conditionally** -- precompute
          either byte 1 for case #2 or byte 2 for case #3. Note that they
          differ by exactly one bit.

          Finally from these two code units we build proper UTF-8 sequence, taking
          into account the case (i.e, the number of bytes to write).
        */
        /**
         * Given [aaaa|bbbb|bbcc|cccc] our goal is to produce:
         * t2 => [0ccc|cccc] [10cc|cccc]
         * s4 => [1110|aaaa] ([110b|bbbb] OR [10bb|bbbb])
         */
#define simdutf_vec(x) _mm_set1_epi16(static_cast<uint16_t>(x))
        // [aaaa|bbbb|bbcc|cccc] => [bbcc|cccc|bbcc|cccc]
        const __m128i t0 = _mm_shuffle_epi8(in, dup_even);
        // [bbcc|cccc|bbcc|cccc] => [00cc|cccc|0bcc|cccc]
        const __m128i t1 = _mm_and_si128(t0, simdutf_vec(0b0011111101111111));
        // [00cc|cccc|0bcc|cccc] => [10cc|cccc|0bcc|cccc]
        const __m128i t2 = _mm_or_si128 (t1, simdutf_vec(0b1000000000000000));

        // [aaaa|bbbb|bbcc|cccc] =>  [0000|aaaa|bbbb|bbcc]
        const __m128i s0 = _mm_srli_epi16(in, 4);
        // [0000|aaaa|bbbb|bbcc] => [0000|aaaa|bbbb|bb00]
        const __m128i s1 = _mm_and_si128(s0, simdutf_vec(0b0000111111111100));
        // [0000|aaaa|bbbb|bb00] => [00bb|bbbb|0000|aaaa]
        const __m128i s2 = _mm_maddubs_epi16(s1, simdutf_vec(0x0140));
        // [00bb|bbbb|0000|aaaa] => [11bb|bbbb|1110|aaaa]
        const __m128i s3 = _mm_or_si128(s2, simdutf_vec(0b1100000011100000));
        const __m128i m0 = _mm_andnot_si128(one_or_two_bytes_bytemask, simdutf_vec(0b0100000000000000));
        const __m128i s4 = _mm_xor_si128(s3, m0);
#undef simdutf_vec

        // 4. expand code units 16-bit => 32-bit
        const __m128i out0 = _mm_unpacklo_epi16(t2, s4);
        const __m128i out1 = _mm_unpackhi_epi16(t2, s4);

        // 5. compress 32-bit code units into 1, 2 or 3 bytes -- 2 x shuffle
        const uint16_t mask = (one_byte_bitmask & 0x5555) |
                              (one_or_two_bytes_bitmask & 0xaaaa);
        if(mask == 0) {
          // We only have three-byte code units. Use fast path.
          const __m128i shuffle = _mm_setr_epi8(2,3,1,6,7,5,10,11,9,14,15,13,-1,-1,-1,-1);
          const __m128i utf8_0 = _mm_shuffle_epi8(out0, shuffle);
          const __m128i utf8_1 = _mm_shuffle_epi8(out1, shuffle);
          _mm_storeu_si128((__m128i*)utf8_output, utf8_0);
          utf8_output += 12;
          _mm_storeu_si128((__m128i*)utf8_output, utf8_1);
          utf8_output += 12;
          buf += 8;
          continue;
        }
        const uint8_t mask0 = uint8_t(mask);

        const uint8_t* row0 = &simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes[mask0][0];
        const __m128i shuffle0 = _mm_loadu_si128((__m128i*)(row0 + 1));
        const __m128i utf8_0 = _mm_shuffle_epi8(out0, shuffle0);

        const uint8_t mask1 = static_cast<uint8_t>(mask >> 8);

        const uint8_t* row1 = &simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes[mask1][0];
        const __m128i shuffle1 = _mm_loadu_si128((__m128i*)(row1 + 1));
        const __m128i utf8_1 = _mm_shuffle_epi8(out1, shuffle1);

        _mm_storeu_si128((__m128i*)utf8_output, utf8_0);
        utf8_output += row0[0];
        _mm_storeu_si128((__m128i*)utf8_output, utf8_1);
        utf8_output += row1[0];

        buf += 8;
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
        if((word & 0xFF80)==0) {
          *utf8_output++ = char(word);
        } else if((word & 0xF800)==0) {
          *utf8_output++ = char((word>>6) | 0b11000000);
          *utf8_output++ = char((word & 0b111111) | 0b10000000);
        } else if((word &0xF800 ) != 0xD800) {
          *utf8_output++ = char((word>>12) | 0b11100000);
          *utf8_output++ = char(((word>>6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((word & 0b111111) | 0b10000000);
        } else {
          // must be a surrogate pair
          uint16_t diff = uint16_t(word - 0xD800);
          uint16_t next_word = big_endian ? scalar::utf16::swap_bytes(buf[k+1]) : buf[k+1];
          k++;
          uint16_t diff2 = uint16_t(next_word - 0xDC00);
          if((diff | diff2) > 0x3FF)  { return std::make_pair(result(error_code::SURROGATE, buf - start + k - 1), utf8_output); }
          uint32_t value = (diff << 10) + diff2 + 0x10000;
          *utf8_output++ = char((value>>18) | 0b11110000);
          *utf8_output++ = char(((value>>12) & 0b111111) | 0b10000000);
          *utf8_output++ = char(((value>>6) & 0b111111) | 0b10000000);
          *utf8_output++ = char((value & 0b111111) | 0b10000000);
        }
      }
      buf += k;
    }
  } // while

  return std::make_pair(result(error_code::SUCCESS, buf - start), utf8_output);
}