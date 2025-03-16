#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdfil.h>
#include "utils.h"

__attribute__((target("avx512bw"))) int main()
{
    char* buf = opaque(malloc(64));
    unsigned i;
    for (i = 0; i < 64; ++i)
        buf[i] = 'a' + (i % 5);
    __m512i v;
    for (i = 0; i < 64; ++i)
        ((char*)&v)[i] = 'M' + (i % 5);
    _mm512_mask_storeu_epi8(buf, 6148914691236517205, v);
    for (i = 0; i < 64; ++i) {
        char c = buf[i];
        if (i & 1)
            ZASSERT(c == 'a' + (i % 5));
        else
            ZASSERT(c == 'M' + (i % 5));
    }
    return 0;
}
