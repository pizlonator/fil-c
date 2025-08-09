// depends on "tables/utf8_to_utf16_tables.h"


// Convert up to 12 bytes from utf8 to utf16 using a mask indicating the
// end of the code points. Only the least significant 12 bits of the mask
// are accessed.
// It returns how many bytes were consumed (up to 12).
template <endianness big_endian>
size_t convert_masked_utf8_to_utf16(const char *input,
                           uint64_t utf8_end_of_code_point_mask,
                           char16_t *&utf16_output) {
  // we use an approach where we try to process up to 12 input bytes.
  // Why 12 input bytes and not 16? Because we are concerned with the size of
  // the lookup tables. Also 12 is nicely divisible by two and three.
  //
  //
  // Optimization note: our main path below is load-latency dependent. Thus it is maybe
  // beneficial to have fast paths that depend on branch prediction but have less latency.
  // This results in more instructions but, potentially, also higher speeds.
  //
  // We first try a few fast paths.
  const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
  const __m128i in = _mm_loadu_si128((__m128i *)input);
  const uint16_t input_utf8_end_of_code_point_mask =
      utf8_end_of_code_point_mask & 0xfff;
  if(utf8_end_of_code_point_mask == 0xfff) {
    // We process the data in chunks of 12 bytes.
    // Note: using 16 bytes is unsafe, see issue_ossfuzz_71218
    __m128i ascii_first = _mm_cvtepu8_epi16(in);
    __m128i ascii_second = _mm_cvtepu8_epi16(_mm_srli_si128(in,8));
    if (big_endian) {
      ascii_first = _mm_shuffle_epi8(ascii_first, swap);
      ascii_second = _mm_shuffle_epi8(ascii_second, swap);
    }
    _mm_storeu_si128(reinterpret_cast<__m128i *>(utf16_output), ascii_first);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(utf16_output + 8), ascii_second);
    utf16_output += 12; // We wrote 12 16-bit characters.
    return 12; // We consumed 12 bytes.
  }
  if(((utf8_end_of_code_point_mask & 0xFFFF) == 0xaaaa)) {
    // We want to take 8 2-byte UTF-8 code units and turn them into 8 2-byte UTF-16 code units.
    // There is probably a more efficient sequence, but the following might do.
    const __m128i sh = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
    const __m128i perm = _mm_shuffle_epi8(in, sh);
    const __m128i ascii = _mm_and_si128(perm, _mm_set1_epi16(0x7f));
    const __m128i highbyte = _mm_and_si128(perm, _mm_set1_epi16(0x1f00));
    __m128i composed = _mm_or_si128(ascii, _mm_srli_epi16(highbyte, 2));
    if (big_endian) composed = _mm_shuffle_epi8(composed, swap);
    _mm_storeu_si128((__m128i *)utf16_output, composed);
    utf16_output += 8; // We wrote 16 bytes, 8 code points.
    return 16;
  }
  if(input_utf8_end_of_code_point_mask == 0x924) {
    // We want to take 4 3-byte UTF-8 code units and turn them into 4 2-byte UTF-16 code units.
    // There is probably a more efficient sequence, but the following might do.
    const __m128i sh = _mm_setr_epi8(2, 1, 0, -1, 5, 4, 3, -1, 8, 7, 6, -1, 11, 10, 9, -1);
    const __m128i perm = _mm_shuffle_epi8(in, sh);
    const __m128i ascii =
        _mm_and_si128(perm, _mm_set1_epi32(0x7f)); // 7 or 6 bits
    const __m128i middlebyte =
        _mm_and_si128(perm, _mm_set1_epi32(0x3f00)); // 5 or 6 bits
    const __m128i middlebyte_shifted = _mm_srli_epi32(middlebyte, 2);
    const __m128i highbyte =
        _mm_and_si128(perm, _mm_set1_epi32(0x0f0000)); // 4 bits
    const __m128i highbyte_shifted = _mm_srli_epi32(highbyte, 4);
    const __m128i composed =
        _mm_or_si128(_mm_or_si128(ascii, middlebyte_shifted), highbyte_shifted);
    __m128i composed_repacked = _mm_packus_epi32(composed, composed);
    if (big_endian) composed_repacked = _mm_shuffle_epi8(composed_repacked, swap);
    _mm_storeu_si128((__m128i *)utf16_output, composed_repacked);
    utf16_output += 4;
    return 12;
  }
  /// We do not have a fast path available, so we fallback.

  const uint8_t idx =
      tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][0];
  const uint8_t consumed =
      tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][1];
  if (idx < 64) {
    // SIX (6) input code-code units
    // this is a relatively easy scenario
    // we process SIX (6) input code-code units. The max length in bytes of six code
    // code units spanning between 1 and 2 bytes each is 12 bytes. On processors
    // where pdep/pext is fast, we might be able to use a small lookup table.
    const __m128i sh =
        _mm_loadu_si128((const __m128i *)tables::utf8_to_utf16::shufutf8[idx]);
    const __m128i perm = _mm_shuffle_epi8(in, sh);
    const __m128i ascii = _mm_and_si128(perm, _mm_set1_epi16(0x7f));
    const __m128i highbyte = _mm_and_si128(perm, _mm_set1_epi16(0x1f00));
    __m128i composed = _mm_or_si128(ascii, _mm_srli_epi16(highbyte, 2));
    if (big_endian) composed = _mm_shuffle_epi8(composed, swap);
    _mm_storeu_si128((__m128i *)utf16_output, composed);
    utf16_output += 6; // We wrote 12 bytes, 6 code points.
  } else if (idx < 145) {
    // FOUR (4) input code-code units
    const __m128i sh =
        _mm_loadu_si128((const __m128i *)tables::utf8_to_utf16::shufutf8[idx]);
    const __m128i perm = _mm_shuffle_epi8(in, sh);
    const __m128i ascii =
        _mm_and_si128(perm, _mm_set1_epi32(0x7f)); // 7 or 6 bits
    const __m128i middlebyte =
        _mm_and_si128(perm, _mm_set1_epi32(0x3f00)); // 5 or 6 bits
    const __m128i middlebyte_shifted = _mm_srli_epi32(middlebyte, 2);
    const __m128i highbyte =
        _mm_and_si128(perm, _mm_set1_epi32(0x0f0000)); // 4 bits
    const __m128i highbyte_shifted = _mm_srli_epi32(highbyte, 4);
    const __m128i composed =
        _mm_or_si128(_mm_or_si128(ascii, middlebyte_shifted), highbyte_shifted);
     __m128i composed_repacked = _mm_packus_epi32(composed, composed);
    if (big_endian) composed_repacked = _mm_shuffle_epi8(composed_repacked, swap);
    _mm_storeu_si128((__m128i *)utf16_output, composed_repacked);
    utf16_output += 4;
  } else if (idx < 209) {
    // TWO (2) input code-code units
    //////////////
    // There might be garbage inputs where a leading byte mascarades as a four-byte
    // leading byte (by being followed by 3 continuation byte), but is not greater than
    // 0xf0. This could trigger a buffer overflow if we only counted leading
    // bytes of the form 0xf0 as generating surrogate pairs, without further UTF-8 validation.
    // Thus we must be careful to ensure that only leading bytes at least as large as 0xf0 generate surrogate pairs.
    // We do as at the cost of an extra mask.
    /////////////
    const __m128i sh =
        _mm_loadu_si128((const __m128i *)tables::utf8_to_utf16::shufutf8[idx]);
    const __m128i perm = _mm_shuffle_epi8(in, sh);
    const __m128i ascii = _mm_and_si128(perm, _mm_set1_epi32(0x7f));
    const __m128i middlebyte = _mm_and_si128(perm, _mm_set1_epi32(0x3f00));
    const __m128i middlebyte_shifted = _mm_srli_epi32(middlebyte, 2);
    __m128i middlehighbyte = _mm_and_si128(perm, _mm_set1_epi32(0x3f0000));
    // correct for spurious high bit
    const __m128i correct =
        _mm_srli_epi32(_mm_and_si128(perm, _mm_set1_epi32(0x400000)), 1);
    middlehighbyte = _mm_xor_si128(correct, middlehighbyte);
    const __m128i middlehighbyte_shifted = _mm_srli_epi32(middlehighbyte, 4);
    // We deliberately carry the leading four bits in highbyte if they are present,
    // we remove them later when computing hightenbits.
    const __m128i highbyte = _mm_and_si128(perm, _mm_set1_epi32(0xff000000));
    const __m128i highbyte_shifted = _mm_srli_epi32(highbyte, 6);
    // When we need to generate a surrogate pair (leading byte > 0xF0), then
    // the corresponding 32-bit value in 'composed'  will be greater than
    // > (0xff00000>>6) or > 0x3c00000. This can be used later to identify the
    // location of the surrogate pairs.
    const __m128i composed =
        _mm_or_si128(_mm_or_si128(ascii, middlebyte_shifted),
                     _mm_or_si128(highbyte_shifted, middlehighbyte_shifted));
    const __m128i composedminus =
        _mm_sub_epi32(composed, _mm_set1_epi32(0x10000));
    const __m128i lowtenbits =
        _mm_and_si128(composedminus, _mm_set1_epi32(0x3ff));
    // Notice the 0x3ff mask:
    const __m128i hightenbits = _mm_and_si128(_mm_srli_epi32(composedminus, 10), _mm_set1_epi32(0x3ff));
    const __m128i lowtenbitsadd =
        _mm_add_epi32(lowtenbits, _mm_set1_epi32(0xDC00));
    const __m128i hightenbitsadd =
        _mm_add_epi32(hightenbits, _mm_set1_epi32(0xD800));
    const __m128i lowtenbitsaddshifted = _mm_slli_epi32(lowtenbitsadd, 16);
    __m128i surrogates =
        _mm_or_si128(hightenbitsadd, lowtenbitsaddshifted);
    uint32_t basic_buffer[4];
    uint32_t basic_buffer_swap[4];
    if (big_endian) {
      _mm_storeu_si128((__m128i *)basic_buffer_swap, _mm_shuffle_epi8(composed, swap));
      surrogates = _mm_shuffle_epi8(surrogates, swap);
    }
    _mm_storeu_si128((__m128i *)basic_buffer, composed);
    uint32_t surrogate_buffer[4];
    _mm_storeu_si128((__m128i *)surrogate_buffer, surrogates);
    for (size_t i = 0; i < 3; i++) {
      if(basic_buffer[i] > 0x3c00000) {
        utf16_output[0] = uint16_t(surrogate_buffer[i] & 0xffff);
        utf16_output[1] = uint16_t(surrogate_buffer[i] >> 16);
        utf16_output += 2;
      } else {
        utf16_output[0] = big_endian ? uint16_t(basic_buffer_swap[i]) : uint16_t(basic_buffer[i]);
        utf16_output++;
      }
    }
  } else {
    // here we know that there is an error but we do not handle errors
  }
  return consumed;
}
