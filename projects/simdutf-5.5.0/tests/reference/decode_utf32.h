#pragma once

#include <cstdio>

namespace simdutf { namespace tests { namespace reference { namespace utf32 {

    enum class Error {
        too_large,
        forbidden_range
    };

    template <typename CONSUMER, typename ERROR_HANDLER>
    bool decode(const char32_t* codepoints, size_t size, CONSUMER consumer, ERROR_HANDLER error_handler) {
        const char32_t* curr = codepoints;
        const char32_t* end = codepoints + size;

        while (curr != end) {
            const uint32_t word = *curr;

            if (word > 0x10FFFF) {
                if (!error_handler(codepoints, curr, Error::too_large))
                    return false;

                continue;
            }

            if (word >= 0xD800 && word <= 0xDFFF) { // required the next word, but we're already at the end of data
                if (!error_handler(codepoints, curr, Error::forbidden_range))
                    return false;

                break;
            }

            consumer(word);

            curr ++;
        }

        return true;
    }

}}}}
