#pragma once

#include <stdint.h>
#include "../base/PerfDefs.h"

namespace DfaUtf8 {
    #include "Dfa.h"
}

template<int OutputType>
FORCEINLINE bool DecodeTrivial(const char *&pSource, const char *pEnd, char *&pDest) {
    static_assert(OutputType == 2 || OutputType == 4, "Only 2-byte and 4-byte output supported");
    using namespace DfaUtf8;
    assert(pSource <= pEnd);

    const uint8_t *RESTRICT s = (const uint8_t *)pSource;
    uint16_t *RESTRICT d = (uint16_t *)pDest;
    uint32_t codepoint;
    uint32_t state = 0;

    const uint8_t *ans_s = s;
    uint16_t *ans_d = d;

    while (s < (const uint8_t *)pEnd) {
        if (decode(&state, &codepoint, *s++))
            continue;
        if (OutputType == 2) {
            if (codepoint > 0xFFFFU) {
                *d++ = (uint16_t)(0xD7C0U + (codepoint >> 10));
                *d++ = (uint16_t)(0xDC00U + (codepoint & 0x3FFU));
            } else {
                *d++ = (uint16_t)codepoint;
            }
        }
        else {
            *(uint32_t *)d = codepoint;
            d += 2;
        }
        if (state == UTF8_ACCEPT) {
            ans_s = s;
            ans_d = d;
        }
    }

    pSource = (const char *)ans_s;
    pDest = (char *)ans_d;
    return state != UTF8_REJECT;
}

template<int InputType>
FORCEINLINE bool EncodeTrivial(const char *&pSource, const char *pEnd, char *&pDest) {
    static_assert(InputType == 2 || InputType == 4, "Only 2-byte and 4-byte input supported");
    assert(pSource <= pEnd);

    const uint16_t *RESTRICT s = (const uint16_t *)pSource;
    uint8_t *RESTRICT d = (uint8_t *)pDest;
    const uint16_t *end = s + (pEnd - pSource) / InputType * (InputType / 2);

    while (s < end) {
        uint32_t codepoint;

        if (InputType == 2) {
            codepoint = *s++;

            if (codepoint - 0xD800U < 0x0800U) {
                if (s < end) {
                    uint32_t tail = *s++;
                    if ((codepoint & 0xFC00U) != 0xD800U || (tail & 0xFC00U) != 0xDC00U)
                        return false;
                    codepoint = 0x10000U + ((codepoint & 0x03FFU) << 10) + (tail & 0x03FFU);
                }
                else {
                    s--;
                    break;
                }
            }
        }
        else {
            codepoint = *(const uint32_t*)s;
            s += 2;
            if (codepoint > 0x10FFFFU || codepoint - 0xD800U < 0x0800U)
                return false;
        }

        if (codepoint <= 0x7FU)
            *d++ = uint8_t(codepoint);
        else if (codepoint <= 0x7FFU) {
            *d++ = uint8_t(0xC0U + (codepoint >> 6));
            *d++ = uint8_t(0x80U + (codepoint & 0x3FU));
        }
        else if (codepoint <= 0xFFFFU) {
            *d++ = uint8_t(0xE0U + (codepoint >> 12));
            *d++ = uint8_t(0x80U + ((codepoint >> 6) & 0x3FU));
            *d++ = uint8_t(0x80U + (codepoint & 0x3FU));
        }
        else {
            *d++ = uint8_t(0xF0U + (codepoint >> 18));
            *d++ = uint8_t(0x80U + ((codepoint >> 12) & 0x3FU));
            *d++ = uint8_t(0x80U + ((codepoint >> 6) & 0x3FU));
            *d++ = uint8_t(0x80U + (codepoint & 0x3FU));
        }

        pSource = (const char *)s;
        pDest = (char *)d;
    }

    return true;
}
