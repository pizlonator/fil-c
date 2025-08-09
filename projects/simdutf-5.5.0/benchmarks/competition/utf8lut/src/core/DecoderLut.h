#pragma once

#include <stdint.h>
#include <emmintrin.h>
#include "../base/PerfDefs.h"

struct shufstep {
    uint32_t _shufC_part0;
    uint32_t _shufC_part1;
    uint32_t srcStep;           //number of bytes processed in input buffer
    uint32_t dstStep;           //number of symbols produced in output buffer (doubled)
};

struct DecoderCoreInfo {
    __m128i shufAB;                     //shuffling mask to get lower two bytes of symbols
    union {
        __m128i shufC;                  //shuffling mask to get third bytes of symbols
        shufstep s;
    };
};
struct DecoderValidationInfo {
    __m128i headerMask;                 //mask of "111..10" bits required in each byte
    __m128i minValues;                  //minimal value allowed for not being overlong (sign-shifted, 16-bit)
};

//a single entry of each LUT is defined
template<bool Validate> struct DecoderLutEntry {};
template<> struct DecoderLutEntry<false> : DecoderCoreInfo {};
template<> struct DecoderLutEntry<true> : DecoderCoreInfo, DecoderValidationInfo {};

//a whole LUT table type
template<bool Validate> struct DecoderLutTable {
    //note: odd-indexed entries are removed (they are impossible with correct input)
    CACHEALIGN DecoderLutEntry<Validate> data[32768];

    static const DecoderLutTable<Validate> *CreateInstance();
    inline static const DecoderLutEntry<Validate> *GetArray() { return CreateInstance()->data; }
private:
    void ComputeAll();
    void ComputeRec(int *sizes, int num, int total);
    void ComputeEntry(const int *sizes, int num);
};
