#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdfil.h>
#include "utils.h"
#include <inttypes.h>

__attribute__((target("avx512bw"))) int main()
{
    uint16_t* buf = opaque(malloc(64));
    unsigned i;
    for (i = 0; i < 32; ++i)
        buf[i] = i * 666;
    __m512i v;
    for (i = 0; i < 32; ++i)
        ((uint16_t*)&v)[i] = i * 1410;
    _mm512_mask_storeu_epi16(buf, 1431655765, v);
    for (i = 0; i < 32; ++i) {
        uint16_t c = buf[i];
        if (i & 1)
            ZASSERT(c == i * 666);
        else
            ZASSERT(c == i * 1410);
    }
    return 0;
}
