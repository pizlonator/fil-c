#pragma once

#include <stdint.h>
#include <tmmintrin.h>
#include "../base/PerfDefs.h"
#include "../core/DecoderLut.h"

/** template params:
 * MaxBytes = 1, 2, 3
 * CheckExceed = false, true
 * Validate = false, true
 * OutputType = 2, 4        //UTF16/32
 */

template<int MaxBytes, bool CheckExceed, bool Validate, int OutputType>
struct DecoderCore {
    FORCEINLINE bool operator()(const char *&ptrSource, char *&ptrDest, const DecoderLutEntry<Validate> *RESTRICT lutTable) {
        static_assert(!Validate || CheckExceed, "Validate core mode requires CheckExceed enabled");
        const char *RESTRICT pSource = ptrSource;
        char *RESTRICT pDest = ptrDest;

        if (MaxBytes == 1) {
            __m128i reg = _mm_loadu_si128((__m128i*)pSource);
            __m128i zero = _mm_setzero_si128();
            if (CheckExceed && _mm_movemask_epi8(reg))
                return false;
            __m128i half0 = _mm_unpacklo_epi8(reg, zero);
            __m128i half1 = _mm_unpackhi_epi8(reg, zero);
            if (OutputType == 2) {
                _mm_storeu_si128((__m128i*)pDest + 0, half0);
                _mm_storeu_si128((__m128i*)pDest + 1, half1);
            }
            else {
                _mm_storeu_si128((__m128i*)pDest + 0, _mm_unpacklo_epi16(half0, zero));
                _mm_storeu_si128((__m128i*)pDest + 1, _mm_unpackhi_epi16(half0, zero));
                _mm_storeu_si128((__m128i*)pDest + 2, _mm_unpacklo_epi16(half1, zero));
                _mm_storeu_si128((__m128i*)pDest + 3, _mm_unpackhi_epi16(half1, zero));
            }
            ptrSource += 16;
            ptrDest += 16 * OutputType;
            return true;
        }
        else {  //MaxBytes = 2 or 3
            __m128i reg = _mm_loadu_si128((__m128i*)pSource);
            if (CheckExceed && !Validate) {
                __m128i pl = _mm_xor_si128(reg, _mm_set1_epi8(char(0x80U)));  //_mm_sub_epi8
                __m128i cmpRes = _mm_cmpgt_epi8(pl, _mm_set1_epi8(MaxBytes == 3 ? 0x6F : 0x5F));
                if (!_mm_cmp_allzero(cmpRes))
                    return false;
            }

            uint32_t mask = _mm_movemask_epi8(_mm_cmplt_epi8(reg, _mm_set1_epi8(char(0xC0U))));
            if (Validate && (mask & 1))
                return false;
            //note: optimized half-index access
            const DecoderLutEntry<Validate> *RESTRICT lookup = TPNT(lutTable, DecoderLutEntry<Validate>, mask * (sizeof(lutTable[0]) / 2));

            __m128i Rab = _mm_shuffle_epi8(reg, lookup->shufAB);
            Rab = _mm_and_si128(Rab, _mm_set1_epi16(0x3F7F));
            Rab = _mm_maddubs_epi16(Rab, _mm_set1_epi16(0x4001));
            __m128i sum = Rab;

            if (MaxBytes == 3) {
                __m128i shufC = _mm_unpacklo_epi8(lookup->shufC, lookup->shufC);
                __m128i Rc = _mm_shuffle_epi8(reg, shufC);
                Rc = _mm_slli_epi16(Rc, 12);
                sum = _mm_add_epi16(sum, Rc);
            }

            if (Validate) {
                const DecoderLutEntry<true> *RESTRICT lookupX = (const DecoderLutEntry<true> *)lookup;
                __m128i byteMask = lookupX->headerMask;
                __m128i header = _mm_and_si128(reg, byteMask);
                __m128i hdrRef = _mm_add_epi8(byteMask, byteMask);
                __m128i hdrCorrect = _mm_cmpeq_epi8(header, hdrRef);
                __m128i overlongSymbol = _mm_cmplt_epi16(_mm_xor_si128(sum, _mm_set1_epi16((short int)0x8000U)), lookupX->minValues);
                __m128i surrogate = _mm_cmpgt_epi16(_mm_sub_epi16(sum, _mm_set1_epi16(0x6000)), _mm_set1_epi16(0x77FF));
                if (MaxBytes == 2) {
                    __m128i shufC = _mm_unpacklo_epi8(lookupX->shufC, lookupX->shufC);
                    hdrCorrect = _mm_and_si128(hdrCorrect, shufC); //forbid 3-byte symbols
                }
                __m128i allCorr = _mm_andnot_si128(_mm_or_si128(overlongSymbol, surrogate), hdrCorrect);
                if (!_mm_cmp_allone(allCorr))
                    return false;
            }

            if (OutputType == 2)
                _mm_storeu_si128((__m128i*)pDest, sum);
            else {
                __m128i zero = _mm_setzero_si128();
                _mm_storeu_si128((__m128i*)pDest + 0, _mm_unpacklo_epi16(sum, zero));
                _mm_storeu_si128((__m128i*)pDest + 1, _mm_unpackhi_epi16(sum, zero));
            }
            ptrSource += lookup->s.srcStep;
            ptrDest += lookup->s.dstStep * (OutputType/2);

            return true;
        }
    }
};
