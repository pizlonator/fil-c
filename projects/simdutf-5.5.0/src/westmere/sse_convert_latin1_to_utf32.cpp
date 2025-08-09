std::pair<const char*, char32_t*> sse_convert_latin1_to_utf32(const char* buf, size_t len, char32_t* utf32_output) {
    const char* end = buf + len;

    while (end - buf >= 16) {
        // Load 16 Latin1 characters (16 bytes) into a 128-bit register
        __m128i in = _mm_loadu_si128((__m128i*)buf);

        // Shift input to process next 4 bytes
        __m128i in_shifted1 = _mm_srli_si128(in, 4);
        __m128i in_shifted2 = _mm_srli_si128(in, 8);
        __m128i in_shifted3 = _mm_srli_si128(in, 12);

        // expand 8-bit to 32-bit unit
        __m128i out1 = _mm_cvtepu8_epi32(in);
        __m128i out2 = _mm_cvtepu8_epi32(in_shifted1);
        __m128i out3 = _mm_cvtepu8_epi32(in_shifted2);
        __m128i out4 = _mm_cvtepu8_epi32(in_shifted3);

        _mm_storeu_si128((__m128i*)utf32_output, out1);
        _mm_storeu_si128((__m128i*)(utf32_output + 4), out2);
        _mm_storeu_si128((__m128i*)(utf32_output + 8), out3);
        _mm_storeu_si128((__m128i*)(utf32_output + 12), out4);

        utf32_output += 16;
        buf += 16;
    }

    return std::make_pair(buf, utf32_output);
}

