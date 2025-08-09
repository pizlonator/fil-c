std::pair<const char*, char32_t*> avx512_convert_latin1_to_utf32(const char* buf, size_t len, char32_t* utf32_output) {
    size_t rounded_len = len & ~0xF;  // Round down to nearest multiple of 16

    for (size_t i = 0; i < rounded_len; i += 16) {
        // Load 16 Latin1 characters into a 128-bit register
        __m128i in = _mm_loadu_si128((__m128i*)&buf[i]);

        // Zero extend each set of 8 Latin1 characters to 16 32-bit integers using vpmovzxbd
        __m512i out = _mm512_cvtepu8_epi32(in);

        // Store the results back to memory
        _mm512_storeu_si512((__m512i*)&utf32_output[i], out);
    }

    // Return pointers pointing to where we left off
    return std::make_pair(buf + rounded_len, utf32_output + rounded_len);
}