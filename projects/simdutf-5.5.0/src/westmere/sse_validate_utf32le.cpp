/* Returns:
   - pointer to the last unprocessed character (a scalar fallback should check the rest);
   - nullptr if an error was detected.
*/
const char32_t* sse_validate_utf32le(const char32_t* input, size_t size) {
    const char32_t* end = input + size;

    const __m128i standardmax = _mm_set1_epi32(0x10ffff);
    const __m128i offset = _mm_set1_epi32(0xffff2000);
    const __m128i standardoffsetmax = _mm_set1_epi32(0xfffff7ff);
    __m128i currentmax = _mm_setzero_si128();
    __m128i currentoffsetmax = _mm_setzero_si128();

    while (input + 4 < end) {
        const __m128i in = _mm_loadu_si128((__m128i *)input);
        currentmax = _mm_max_epu32(in,currentmax);
        currentoffsetmax = _mm_max_epu32(_mm_add_epi32(in, offset), currentoffsetmax);
        input += 4;
    }
    __m128i is_zero = _mm_xor_si128(_mm_max_epu32(currentmax, standardmax), standardmax);
    if(_mm_test_all_zeros(is_zero, is_zero) == 0) {
        return nullptr;
    }

    is_zero = _mm_xor_si128(_mm_max_epu32(currentoffsetmax, standardoffsetmax), standardoffsetmax);
    if(_mm_test_all_zeros(is_zero, is_zero) == 0) {
        return nullptr;
    }

    return input;
}


const result sse_validate_utf32le_with_errors(const char32_t* input, size_t size) {
    const char32_t* start = input;
    const char32_t* end = input + size;

    const __m128i standardmax = _mm_set1_epi32(0x10ffff);
    const __m128i offset = _mm_set1_epi32(0xffff2000);
    const __m128i standardoffsetmax = _mm_set1_epi32(0xfffff7ff);
    __m128i currentmax = _mm_setzero_si128();
    __m128i currentoffsetmax = _mm_setzero_si128();

    while (input + 4 < end) {
        const __m128i in = _mm_loadu_si128((__m128i *)input);
        currentmax = _mm_max_epu32(in,currentmax);
        currentoffsetmax = _mm_max_epu32(_mm_add_epi32(in, offset), currentoffsetmax);

        __m128i is_zero = _mm_xor_si128(_mm_max_epu32(currentmax, standardmax), standardmax);
        if(_mm_test_all_zeros(is_zero, is_zero) == 0) {
            return result(error_code::TOO_LARGE, input - start);
        }

        is_zero = _mm_xor_si128(_mm_max_epu32(currentoffsetmax, standardoffsetmax), standardoffsetmax);
        if(_mm_test_all_zeros(is_zero, is_zero) == 0) {
            return result(error_code::SURROGATE, input - start);
        }
        input += 4;
    }

    return result(error_code::SUCCESS, input - start);
}
