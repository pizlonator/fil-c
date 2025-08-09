template <endianness big_endian>
std::pair<const char*, char16_t*> avx2_convert_latin1_to_utf16(const char* latin1_input, size_t len, char16_t* utf16_output) {
    size_t rounded_len = len & ~0xF; // Round down to nearest multiple of 32

    size_t i = 0;
    for (; i < rounded_len; i += 16) {
        // Load 16 bytes from the address (input + i) into a xmm register
        __m128i xmm0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(latin1_input + i));

        // Zero extend each byte in xmm0 to word and put it in another xmm register
        __m128i xmm1 = _mm_cvtepu8_epi16(xmm0);

        // Shift xmm0 to the right by 8 bytes
        xmm0 = _mm_srli_si128(xmm0, 8);

        // Zero extend each byte in the shifted xmm0 to word in xmm0
        xmm0 = _mm_cvtepu8_epi16(xmm0);

        if (big_endian) {
            const __m128i swap = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
            xmm0 = _mm_shuffle_epi8(xmm0, swap);
            xmm1 = _mm_shuffle_epi8(xmm1, swap);
        }

        // Store the contents of xmm1 into the address pointed by (output + i)
        _mm_storeu_si128(reinterpret_cast<__m128i*>(utf16_output + i), xmm1);

        // Store the contents of xmm0 into the address pointed by (output + i + 8)
        _mm_storeu_si128(reinterpret_cast<__m128i*>(utf16_output + i + 8), xmm0);
    }

    return std::make_pair(latin1_input + rounded_len, utf16_output + rounded_len);

}
