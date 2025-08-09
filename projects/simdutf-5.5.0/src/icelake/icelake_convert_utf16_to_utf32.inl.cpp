// file included directly

/*
  Returns a pair: the first unprocessed byte from buf and utf32_output
  A scalar routing should carry on the conversion of the tail.
*/
template <endianness big_endian>
std::tuple<const char16_t*, char32_t*, bool> convert_utf16_to_utf32(const char16_t* buf, size_t len, char32_t* utf32_output) {
  const char16_t* end = buf + len;
  const __m512i v_fc00 = _mm512_set1_epi16((uint16_t)0xfc00);
  const __m512i v_d800 = _mm512_set1_epi16((uint16_t)0xd800);
  const __m512i v_dc00 = _mm512_set1_epi16((uint16_t)0xdc00);
  __mmask32 carry{0};
  const __m512i byteflip = _mm512_setr_epi64(
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809,
            0x0607040502030001,
            0x0e0f0c0d0a0b0809
        );
  while (std::distance(buf,end) >= 32) {
    // Always safe because buf + 32 <= end so that end - buf >= 32 bytes:
    __m512i in = _mm512_loadu_si512((__m512i*)buf);
    if(big_endian) { in = _mm512_shuffle_epi8(in, byteflip); }

    // H - bitmask for high surrogates
    const __mmask32 H = _mm512_cmpeq_epi16_mask(_mm512_and_si512(in, v_fc00), v_d800);
    // H - bitmask for low surrogates
    const __mmask32 L = _mm512_cmpeq_epi16_mask(_mm512_and_si512(in, v_fc00), v_dc00);

    if ((H|L)) {
      // surrogate pair(s) in a register
      const __mmask32 V = (L ^ (carry | (H << 1)));   // A high surrogate must be followed by low one and a low one must be preceded by a high one.
                                                      // If valid, V should be equal to 0

      if(V == 0) {
        // valid case
        /*
            Input surrogate pair:
            |1101.11aa.aaaa.aaaa|1101.10bb.bbbb.bbbb|
                low surrogate      high surrogate
        */
        /*  1. Expand all code units to 32-bit code units
            in  |0000.0000.0000.0000.1101.11aa.aaaa.aaaa|0000.0000.0000.0000.1101.10bb.bbbb.bbbb|
        */
        const __m512i first = _mm512_cvtepu16_epi32(_mm512_castsi512_si256(in));
        const __m512i second = _mm512_cvtepu16_epi32(_mm512_extracti32x8_epi32(in,1));

        /*  2. Shift by one 16-bit word to align low surrogates with high surrogates
            in      |0000.0000.0000.0000.1101.11aa.aaaa.aaaa|0000.0000.0000.0000.1101.10bb.bbbb.bbbb|
            shifted |????.????.????.????.????.????.????.????|0000.0000.0000.0000.1101.11aa.aaaa.aaaa|
        */
        const __m512i shifted_first = _mm512_alignr_epi32(second, first, 1);
        const __m512i shifted_second = _mm512_alignr_epi32(_mm512_setzero_si512(), second, 1);

        /*  3. Align all high surrogates in first and second by shifting to the left by 10 bits
            |0000.0000.0000.0000.1101.11aa.aaaa.aaaa|0000.0011.0110.bbbb.bbbb.bb00.0000.0000|
        */
        const __m512i aligned_first = _mm512_mask_slli_epi32(first, (__mmask16)H, first, 10);
        const __m512i aligned_second = _mm512_mask_slli_epi32(second, (__mmask16)(H>>16), second, 10);

        /*  4. Remove surrogate prefixes and add offset 0x10000 by adding in, shifted and constant
            in      |0000.0000.0000.0000.1101.11aa.aaaa.aaaa|0000.0011.0110.bbbb.bbbb.bb00.0000.0000|
            shifted |????.????.????.????.????.????.????.????|0000.0000.0000.0000.1101.11aa.aaaa.aaaa|
            constant|1111.1100.1010.0000.0010.0100.0000.0000|1111.1100.1010.0000.0010.0100.0000.0000|
        */
        const __m512i constant = _mm512_set1_epi32((uint32_t)0xfca02400);
        const __m512i added_first = _mm512_mask_add_epi32(aligned_first, (__mmask16)H, aligned_first, shifted_first);
        const __m512i utf32_first = _mm512_mask_add_epi32(added_first, (__mmask16)H, added_first, constant);

        const __m512i added_second = _mm512_mask_add_epi32(aligned_second, (__mmask16)(H>>16), aligned_second, shifted_second);
        const __m512i utf32_second = _mm512_mask_add_epi32(added_second, (__mmask16)(H>>16), added_second, constant);

        //  5. Store all valid UTF-32 code units (low surrogate positions and 32nd word are invalid)
        const __mmask32 valid = ~L & 0x7fffffff;
        // We deliberately do a _mm512_maskz_compress_epi32 followed by storeu_epi32
        // to ease performance portability to Zen 4.
        const __m512i compressed_first = _mm512_maskz_compress_epi32((__mmask16)(valid), utf32_first);
        const size_t howmany1 = count_ones((uint16_t)(valid));
        _mm512_storeu_si512((__m512i *) utf32_output,  compressed_first);
        utf32_output += howmany1;
        const __m512i compressed_second = _mm512_maskz_compress_epi32((__mmask16)(valid >> 16), utf32_second);
        const size_t howmany2 = count_ones((uint16_t)(valid >> 16));
        // The following could be unsafe in some cases?
        //_mm512_storeu_epi32((__m512i *) utf32_output, compressed_second);
        _mm512_mask_storeu_epi32((__m512i *) utf32_output, __mmask16((1<<howmany2)-1), compressed_second);
        utf32_output += howmany2;
        // Only process 31 code units, but keep track if the 31st word is a high surrogate as a carry
        buf += 31;
        carry = (H >> 30) & 0x1;
      } else {
        // invalid case
        return std::make_tuple(buf+carry, utf32_output, false);
      }
    } else {
      // no surrogates
      // extend all thirty-two 16-bit code units to thirty-two 32-bit code units
      _mm512_storeu_si512((__m512i *)(utf32_output), _mm512_cvtepu16_epi32(_mm512_castsi512_si256(in)));
      _mm512_storeu_si512((__m512i *)(utf32_output) + 1, _mm512_cvtepu16_epi32(_mm512_extracti32x8_epi32(in,1)));
      utf32_output += 32;
      buf += 32;
      carry = 0;
    }
  } // while
  return std::make_tuple(buf+carry, utf32_output, true);
}
