template <endianness big_endian>
std::pair<const char*, char16_t*> sse_convert_latin1_to_utf16(const char *latin1_input, size_t len,
                                                              char16_t *utf16_output) {
    size_t rounded_len = len & ~0xF; // Round down to nearest multiple of 16
    for (size_t i = 0; i < rounded_len; i += 16) {
        // Load 16 Latin1 characters into a 128-bit register
        __m128i in = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&latin1_input[i]));
        __m128i out1 = big_endian ? _mm_unpacklo_epi8(_mm_setzero_si128(), in)
                         : _mm_unpacklo_epi8(in, _mm_setzero_si128());
        __m128i out2 = big_endian ? _mm_unpackhi_epi8(_mm_setzero_si128(), in)
                         : _mm_unpackhi_epi8(in, _mm_setzero_si128());
        // Zero extend each Latin1 character to 16-bit integers and store the results back to memory
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&utf16_output[i]), out1);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&utf16_output[i + 8]), out2);
    }
    // return pointers pointing to where we left off
    return std::make_pair(latin1_input + rounded_len, utf16_output + rounded_len);
}
