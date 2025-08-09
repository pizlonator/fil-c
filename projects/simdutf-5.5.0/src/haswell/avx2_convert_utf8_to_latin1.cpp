// depends on "tables/utf8_to_utf16_tables.h"

// Convert up to 12 bytes from utf8 to latin1 using a mask indicating the
// end of the code points. Only the least significant 12 bits of the mask
// are accessed.
// It returns how many bytes were consumed (up to 12).
size_t convert_masked_utf8_to_latin1(const char *input,
                           uint64_t utf8_end_of_code_point_mask,
                           char *&latin1_output) {
  // we use an approach where we try to process up to 12 input bytes.
  // Why 12 input bytes and not 16? Because we are concerned with the size of
  // the lookup tables. Also 12 is nicely divisible by two and three.
  //
  //
  // Optimization note: our main path below is load-latency dependent. Thus it is maybe
  // beneficial to have fast paths that depend on branch prediction but have less latency.
  // This results in more instructions but, potentially, also higher speeds.
  //
  const __m128i in = _mm_loadu_si128((__m128i *)input);

  const uint16_t input_utf8_end_of_code_point_mask =
      utf8_end_of_code_point_mask & 0xfff; // we are only processing 12 bytes in case it is not all ASCII

  if(utf8_end_of_code_point_mask == 0xfff) {
    // We process the data in chunks of 12 bytes.
    _mm_storeu_si128(reinterpret_cast<__m128i *>(latin1_output), in);
    latin1_output += 12; // We wrote 12 characters.
    return 12; // We consumed 1 bytes.
  }
  /// We do not have a fast path available, so we fallback.
  const uint8_t idx =
      tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][0];
  const uint8_t consumed =
      tables::utf8_to_utf16::utf8bigindex[input_utf8_end_of_code_point_mask][1];
  // this indicates an invalid input:
  if(idx >= 64) { return consumed; }
  // Here we should have (idx < 64), if not, there is a bug in the validation or elsewhere.
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
  const __m128i latin1_packed = _mm_packus_epi16(composed,composed);
  // writing 8 bytes even though we only care about the first 6 bytes.
  // performance note: it would be faster to use _mm_storeu_si128, we should investigate.
  _mm_storel_epi64((__m128i *)latin1_output, latin1_packed);
  latin1_output += 6; // We wrote 6 bytes.
  return consumed;
}
