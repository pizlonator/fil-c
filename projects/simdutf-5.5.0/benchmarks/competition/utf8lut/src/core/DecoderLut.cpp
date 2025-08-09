#include "../core/DecoderLut.h"
#include <assert.h>
#include <string.h>

static inline void SetEntry(DecoderLutEntry<false> &entry,
    const __m128i &shufAB, const __m128i &shufC, int srcStep, int dstStep,
    const __m128i &/*headerMask*/, const __m128i &/*minValues*/
) {
    entry.shufAB = shufAB;
    entry.shufC = shufC;
    entry.s.srcStep = srcStep;
    entry.s.dstStep = dstStep;
}
static inline void SetEntry(DecoderLutEntry<true> &entry,
    const __m128i &shufAB, const __m128i &shufC, int srcStep, int dstStep,
    const __m128i &headerMask, const __m128i &minValues
) {
    entry.shufAB = shufAB;
    entry.shufC = shufC;
    entry.s.
    srcStep = srcStep;
    entry.s.dstStep = dstStep;
    entry.headerMask = headerMask;
    entry.minValues = minValues;
}


template<bool Validate> void DecoderLutTable<Validate>::ComputeAll() {
    //fill tables with empty values for BAD masks
    DecoderLutEntry<Validate> empty;
    SetEntry(empty,
        _mm_set1_epi8(-1),      //produce zero symbols
        _mm_set1_epi8(-1),      //produce zero symbols
        16,                     //skip the whole 16-byte block on error
        0,                      //do not move output pointer
        _mm_set1_epi8(-1),      //forbid any bytes except 11111110
        _mm_set1_epi16(0x7FFF)  //forbid almost all output codes
    );
    for (int i = 0; i < 32768; i++)
        data[i] = empty;
    //start recursive search for valid masks
    int sizes[32];
    ComputeRec(sizes, 0, 0);    //10609 entries total
}

template<bool Validate> void DecoderLutTable<Validate>::ComputeRec(int *sizes, int num, int total) {
    if (total >= 16)
        return ComputeEntry(sizes, num);
    for (int sz = 1; sz <= 3; sz++) {
        sizes[num] = sz;
        ComputeRec(sizes, num + 1, total + sz);
    }
}

template<bool Validate> void DecoderLutTable<Validate>::ComputeEntry(const int *sizes, int num) {
    //find maximal number of chars to decode
    int cnt = num - 1;
    int preSum = 0;
    for (int i = 0; i < cnt; i++)
        preSum += sizes[i];
    assert(preSum < 16);
    //Note: generally, we can process a char only if the next byte is within XMM register
    //However, if the last char takes 3 bytes and fits the register tightly, we can take it too
    if (preSum == 13 && preSum + sizes[cnt] == 16)
        preSum += sizes[cnt++];
    //still cannot process more that 8 chars per register
    while (cnt > 8)
        preSum -= sizes[--cnt];

    //generate bitmask
    int mask = 0;
    int pos = 0;
    for (int i = 0; i < num; i++)
        for (int j = 0; j < sizes[i]; j++)
            mask |= (j>0) << (pos++);
    assert(pos >= 16);
    mask &= 0xFFFF;

    //generate shuffle masks
    char shufAB[16], shufC[16];
    memset(shufAB, -1, sizeof(shufAB));
    memset(shufC, -1, sizeof(shufC));
    pos = 0;
    for (int i = 0; i < cnt; i++) {
        int sz = sizes[i];
        for (int j = sz-1; j >= 0; j--) {
            if (j < 2)
                shufAB[2 * i + j] = char(pos++);
            else
                shufC[i] = char(pos++);
        }
    }
    assert(pos <= 16);

    //generate header masks for validation
    char headerMask[16];
    memset(headerMask, 0, sizeof(headerMask));
    pos = 0;
    for (int i = 0; i < cnt; i++) {
        int sz = sizes[i];
        for (int j = 0; j < sz; j++) {
            int bits = 2;
            if (j == 0)
                bits = (sz == 1 ? 1 : sz == 2 ? 3 : 4);
            headerMask[(pos++)%16] = -char(1 << (8 - bits));
        }
    }
    assert(pos <= 16);

    //generate min symbols values for validation 
    int16_t minValues[8];
    for (int i = 0; i < 8; i++) {
        int sz = (i < cnt ? sizes[i] : 1);
        minValues[i] = 0x8000 + (sz == 1 ? 0 : sz == 2 ? (1<<7) : (1<<11));
    }

    //store info into the lookup table
    assert(mask % 2 == 0);  mask /= 2;  //note: odd masks are removed
    SetEntry(data[mask],
        _mm_loadu_si128((__m128i*)shufAB),
        _mm_loadu_si128((__m128i*)shufC),
        preSum,
        2 * cnt,
        _mm_loadu_si128((__m128i*)headerMask),
        _mm_loadu_si128((__m128i*)minValues)
    );
}

template<bool Validate> const DecoderLutTable<Validate> *DecoderLutTable<Validate>::CreateInstance() {
    static DecoderLutTable<Validate> *singletonTable = 0;
    if (!singletonTable) {
        singletonTable = (DecoderLutTable<Validate> *)_mm_malloc(sizeof(DecoderLutTable<Validate>), CACHE_LINE);
        singletonTable->ComputeAll();
    }
    return singletonTable;
}


template struct DecoderLutTable<false>;
template struct DecoderLutTable<true>;
