#pragma once

#include "../base/PerfDefs.h"
#include <tmmintrin.h>

/** template params:
 * MaxBytes = 1, 2, 3
 * CheckExceed = false, true
 * InputType = 2, 4         //UTF16/32
 */

template<int MaxBytes, bool CheckExceed, int InputType>
struct EncoderCore {
    FORCEINLINE bool operator()(const char *&ptrSource, char *&ptrDest, const EncoderLutEntry *RESTRICT lutTable) {
        const char *RESTRICT pSource = ptrSource;
        char *RESTRICT pDest = ptrDest;
        
        //read block of 8 symbols
        __m128i reg;
        if (InputType == 2)
            reg = _mm_loadu_si128((const __m128i *)pSource);
        else {
            //read in two halves
            __m128i reg0 = _mm_loadu_si128((const __m128i *)pSource + 0);
            __m128i reg1 = _mm_loadu_si128((const __m128i *)pSource + 1);
            __m128i shuf = _mm_setr_epi8(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
            //shuffle all lower 16-bit words to lower halves of registers
            reg0 = _mm_shuffle_epi8(reg0, shuf);
            reg1 = _mm_shuffle_epi8(reg1, shuf);
            //get all 8 lower 16-bit words
            reg = _mm_unpacklo_epi64(reg0, reg1);
            //if required, check that higher 16-bit words are zero
            if (CheckExceed && !_mm_cmp_allone(_mm_cmpeq_epi16(_mm_unpackhi_epi64(reg0, reg1), _mm_setzero_si128())))
                return false;
        }

        if (MaxBytes == 1) {
            if (CheckExceed && !_mm_cmp_allone(_mm_cmpeq_epi16(_mm_andnot_si128(_mm_set1_epi16(0x007FU), reg), _mm_setzero_si128())))
                return false;
            __m128i res = _mm_packus_epi16(reg, _mm_setzero_si128());
            _mm_storeu_si128((__m128i *)pDest, res);
            pDest += 8;
        }
        else if (MaxBytes == 2) {
            //levels of bytes
            __m128i levelA = reg;                                                   //abcdefgh|ABCDEFGH
            __m128i levelB = _mm_srli_epi16(reg, 6);                                //ghABCDEF|GH......
            //put all bytes of each half into a register
            __m128i levBA = _mm_xor_si128(levelB, _mm_slli_epi16(levelA, 8));       //ghABC,,,|abcdefgh

            //check which symbols are long
            __m128i lenGe2 = _mm_cmpgt_epi16(levelB, _mm_set1_epi16(0x0001U));
            //check if there are three+ bytes symbols
            if (CheckExceed && !_mm_cmp_allzero(_mm_cmpgt_epi16(levelB, _mm_set1_epi16(0x001FU))))
                return false;
            //compose lens mask for lookup (with a 6-byte shift)
            __m128i lensAll = _mm_shuffle_epi8(lenGe2, _mm_setr_epi8(-1, -1, -1, -1, -1, -1, 0, 2, 4, 6, 8, 10, 12, 14, -1, -1));
            //get byte offset into lookup table (i.e. index multiplied by 64 = entry size)
            uint32_t offset = _mm_movemask_epi8(lensAll);
            static_assert(sizeof(EncoderLutEntry) == 64, "Wrong size of EncoderLutEntry");
        
            //load info from LUT (using byte offset)
            const EncoderLutEntry *RESTRICT lookup = TPNT(lutTable, EncoderLutEntry, offset);
            //shuffle bytes to compact layout
            __m128i res = _mm_shuffle_epi8(levBA, lookup->shuf);
            //add headers to all bytes
            __m128i header = lookup->headerMask;
            res = _mm_andnot_si128(header, res);
            res = _mm_xor_si128(res, _mm_add_epi8(header, header)); //add_epi8
    
            //write results
            _mm_storeu_si128((__m128i *)pDest, res);
            pDest += lookup->dstStep;
        }
        else if (MaxBytes == 3) {
            //levels of bytes
            __m128i levAC = _mm_maddubs_epi16(_mm_and_si128(reg, _mm_set1_epi16((short int)0xF0FFU)), _mm_set1_epi16(0x1001U));
            __m128i levelB = _mm_srli_epi16(reg, 6);
            //put all bytes of each half into a separate register
            __m128i levels0 = _mm_unpacklo_epi64(levAC, levelB);
            __m128i levels1 = _mm_unpackhi_epi64(levAC, levelB);

            if (CheckExceed) {
                //check if there are any surrogates
                __m128i diff = _mm_sub_epi16(reg, _mm_set1_epi16(0x5800U));
                if (!_mm_cmp_allzero(_mm_cmplt_epi16(diff, _mm_set1_epi16((short int)0x8800U))))
                    return false;
            }

            //check for symbols with len at least 2 and 3
            __m128i lenGe2 = _mm_cmpgt_epi16(levelB, _mm_set1_epi16(0x0001U));
            __m128i lenGe3 = _mm_cmpgt_epi16(levelB, _mm_set1_epi16(0x001FU));
            //get a single mask from the two comparison results
            __m128i lensMix = _mm_xor_si128(_mm_srli_epi16(lenGe3, 8), lenGe2);
            uint32_t allMask = _mm_movemask_epi8(lensMix);
            //each half of the mask corresponds to a LUT index
            uint32_t offset0 = (allMask & 255U) * sizeof(EncoderLutEntry);
            uint32_t offset1 = (allMask >> 8U) * sizeof(EncoderLutEntry);
        
            //load info from LUT
            const EncoderLutEntry *RESTRICT lookup0 = TPNT(lutTable, EncoderLutEntry, offset0);
            const EncoderLutEntry *RESTRICT lookup1 = TPNT(lutTable, EncoderLutEntry, offset1);
            //shuffle bytes of each half to compact layout
            __m128i res0 = _mm_shuffle_epi8(levels0, lookup0->shuf);
            __m128i res1 = _mm_shuffle_epi8(levels1, lookup1->shuf);
            //add headers to all bytes
            __m128i header0 = lookup0->headerMask;
            __m128i header1 = lookup1->headerMask;
            res0 = _mm_andnot_si128(header0, res0);
            res1 = _mm_andnot_si128(header1, res1);
            res0 = _mm_xor_si128(res0, _mm_add_epi8(header0, header0));
            res1 = _mm_xor_si128(res1, _mm_add_epi8(header1, header1));
            
            //write results
            _mm_storeu_si128((__m128i *)pDest, res0);
            pDest += lookup0->dstStep;
            _mm_storeu_si128((__m128i *)pDest, res1);
            pDest += lookup1->dstStep;
        }

        //save new addresses
        ptrSource += 8 * InputType;
        ptrDest = pDest;
        return true;
    }
};
