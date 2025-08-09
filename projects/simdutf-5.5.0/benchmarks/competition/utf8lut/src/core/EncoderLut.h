#pragma once

#include <stdint.h>
#include <emmintrin.h>
#include "../base/PerfDefs.h"

//a single entry of each LUT is defined
struct m128pair { __m128i __a, __b; };
struct EncoderLutEntry {
    __m128i shuf;                       //shuffling mask to move bytes into position
    __m128i headerMask;                 //mask of bits which represents header (for each byte)
    union {
        uint32_t dstStep;               //number of bytes processed in output buffer
        m128pair s;   //padding up to 64 bytes
    };
};

//a whole LUT table type
template<bool ThreeBytes> struct EncoderLutTable {
    CACHEALIGN EncoderLutEntry data[256];

    static const EncoderLutTable<ThreeBytes> *CreateInstance();
    inline static const EncoderLutEntry *GetArray() { return CreateInstance()->data; }
private:
    void ComputeAll();
    void ComputeEntry(int lensMask);
};
