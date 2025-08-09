std::pair<const char*, char32_t*> avx2_convert_latin1_to_utf32(const char* buf, size_t len, char32_t* utf32_output) {
    size_t rounded_len = ((len | 7) ^ 7);  // Round down to nearest multiple of 8

    for (size_t i = 0; i < rounded_len; i += 8) {
        // Load 8 Latin1 characters into a 64-bit register
        __m128i in = _mm_loadl_epi64((__m128i*)&buf[i]);

        // Zero extend each set of 8 Latin1 characters to 8 32-bit integers using vpmovzxbd
        __m256i out = _mm256_cvtepu8_epi32(in);

        // Store the results back to memory
        _mm256_storeu_si256((__m256i*)&utf32_output[i], out);
    }

    // return pointers pointing to where we left off
    return std::make_pair(buf + rounded_len, utf32_output + rounded_len);
}

