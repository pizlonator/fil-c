
/*
    This upcoming macro (SIMDUTF_ICELAKE_TRANSCODE16) takes 16 + 4 bytes (of a UTF-8 string)
    and loads all possible 4-byte substring into an AVX512 register.

    For example if we have bytes abcdefgh... we create following 32-bit lanes

    [abcd|bcde|cdef|defg|efgh|...]
     ^                          ^
     byte 0 of reg              byte 63 of reg
*/
/** pshufb
        # lane{0,1,2} have got bytes: [  0,  1,  2,  3,  4,  5,  6,  8,  9, 10, 11, 12, 13, 14, 15]
        # lane3 has got bytes:        [ 16, 17, 18, 19,  4,  5,  6,  8,  9, 10, 11, 12, 13, 14, 15]

        expand_ver2 = [
            # lane 0:
            0, 1, 2, 3,
            1, 2, 3, 4,
            2, 3, 4, 5,
            3, 4, 5, 6,

            # lane 1:
            4, 5, 6, 7,
            5, 6, 7, 8,
            6, 7, 8, 9,
            7, 8, 9, 10,

            # lane 2:
             8,  9, 10, 11,
             9, 10, 11, 12,
            10, 11, 12, 13,
            11, 12, 13, 14,

            # lane 3 order: 13, 14, 15, 16 14, 15, 16, 17, 15, 16, 17, 18, 16, 17, 18, 19
            12, 13, 14, 15,
            13, 14, 15,  0,
            14, 15,  0,  1,
            15,  0,  1,  2,
        ]
*/

#define SIMDUTF_ICELAKE_TRANSCODE16(LANE0, LANE1, MASKED)                                                    \
        {                                                                                                    \
            const __m512i merged = _mm512_mask_mov_epi32(LANE0, 0x1000, LANE1);                              \
            const __m512i expand_ver2 = _mm512_setr_epi64(                                                   \
                0x0403020103020100,                                                                          \
                0x0605040305040302,                                                                          \
                0x0807060507060504,                                                                          \
                0x0a09080709080706,                                                                          \
                0x0c0b0a090b0a0908,                                                                          \
                0x0e0d0c0b0d0c0b0a,                                                                          \
                0x000f0e0d0f0e0d0c,                                                                          \
                0x0201000f01000f0e                                                                           \
            );                                                                                               \
            const __m512i input = _mm512_shuffle_epi8(merged, expand_ver2);                                  \
                                                                                                             \
            __mmask16 leading_bytes;                                                                         \
            const __m512i v_0000_00c0 = _mm512_set1_epi32(0xc0);                                             \
            const __m512i t0 = _mm512_and_si512(input, v_0000_00c0);                                         \
            const __m512i v_0000_0080 = _mm512_set1_epi32(0x80);                                             \
            leading_bytes = _mm512_cmpneq_epu32_mask(t0, v_0000_0080);                                       \
                                                                                                             \
            __m512i char_class;                                                                              \
            char_class = _mm512_srli_epi32(input, 4);                                                        \
            /*  char_class = ((input >> 4) & 0x0f) | 0x80808000 */                                           \
            const __m512i v_0000_000f = _mm512_set1_epi32(0x0f);                                             \
            const __m512i v_8080_8000 = _mm512_set1_epi32(0x80808000);                                       \
            char_class = _mm512_ternarylogic_epi32(char_class, v_0000_000f, v_8080_8000, 0xea);              \
                                                                                                             \
            const int valid_count = static_cast<int>(count_ones(leading_bytes));                             \
            const __m512i utf32 = expanded_utf8_to_utf32(char_class, input);                                 \
                                                                                                             \
            const __m512i out = _mm512_mask_compress_epi32(_mm512_setzero_si512(), leading_bytes, utf32);    \
                                                                                                             \
            if (UTF32) {                                                                                     \
                if(MASKED) {                                                                                 \
                    const __mmask16 valid = uint16_t((1 << valid_count) - 1);                                \
                    _mm512_mask_storeu_epi32((__m512i*)output, valid, out);                                  \
                } else {                                                                                     \
                    _mm512_storeu_si512((__m512i*)output, out);                                              \
                }                                                                                            \
                output += valid_count;                                                                       \
            } else {                                                                                         \
                if(MASKED) {                                                                                 \
                    output += utf32_to_utf16_masked<big_endian>(byteflip, out, valid_count, reinterpret_cast<char16_t *>(output)); \
                } else {                                                                                     \
                    output += utf32_to_utf16<big_endian>(byteflip, out, valid_count, reinterpret_cast<char16_t *>(output));        \
                }                                                                                            \
            }                                                                                                \
        }

#define SIMDUTF_ICELAKE_WRITE_UTF16_OR_UTF32(INPUT, VALID_COUNT, MASKED)                                    \
{                                                                                                           \
    if (UTF32) {                                                                                            \
        if(MASKED) {                                                                                        \
            const __mmask16 valid_mask = uint16_t((1 << VALID_COUNT) - 1);                                  \
            _mm512_mask_storeu_epi32((__m512i*)output, valid_mask, INPUT);                                  \
        } else {                                                                                            \
            _mm512_storeu_si512((__m512i*)output, INPUT);                                              \
        }                                                                                                   \
        output += VALID_COUNT;                                                                              \
    } else {                                                                                                \
        if(MASKED) {                                                                                        \
            output += utf32_to_utf16_masked<big_endian>(byteflip, INPUT, VALID_COUNT, reinterpret_cast<char16_t *>(output));      \
        } else {                                                                                            \
            output += utf32_to_utf16<big_endian>(byteflip, INPUT, VALID_COUNT, reinterpret_cast<char16_t *>(output));             \
        }                                                                                                   \
    }                                                                                                       \
}


#define SIMDUTF_ICELAKE_STORE_ASCII(UTF32, utf8, output)                                  \
        if (UTF32) {                                                                      \
                const __m128i t0 = _mm512_castsi512_si128(utf8);                          \
                const __m128i t1 = _mm512_extracti32x4_epi32(utf8, 1);                    \
                const __m128i t2 = _mm512_extracti32x4_epi32(utf8, 2);                    \
                const __m128i t3 = _mm512_extracti32x4_epi32(utf8, 3);                    \
                _mm512_storeu_si512((__m512i*)(output + 0*16), _mm512_cvtepu8_epi32(t0)); \
                _mm512_storeu_si512((__m512i*)(output + 1*16), _mm512_cvtepu8_epi32(t1)); \
                _mm512_storeu_si512((__m512i*)(output + 2*16), _mm512_cvtepu8_epi32(t2)); \
                _mm512_storeu_si512((__m512i*)(output + 3*16), _mm512_cvtepu8_epi32(t3)); \
        } else {                                                                          \
                const __m256i h0 = _mm512_castsi512_si256(utf8);                          \
                const __m256i h1 = _mm512_extracti64x4_epi64(utf8, 1);                    \
                if(big_endian) {                                                          \
                _mm512_storeu_si512((__m512i*)(output + 0*16), _mm512_shuffle_epi8(_mm512_cvtepu8_epi16(h0), byteflip)); \
                _mm512_storeu_si512((__m512i*)(output + 2*16), _mm512_shuffle_epi8(_mm512_cvtepu8_epi16(h1), byteflip)); \
                } else {                                                                  \
                _mm512_storeu_si512((__m512i*)(output + 0*16), _mm512_cvtepu8_epi16(h0)); \
                _mm512_storeu_si512((__m512i*)(output + 2*16), _mm512_cvtepu8_epi16(h1)); \
                }                                                                         \
        }
