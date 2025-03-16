#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdfil.h>
#include "utils.h"

__attribute__((target("avx512bw"))) int main()
{
    char* buf = opaque(malloc(48));
    unsigned i;
    for (i = 0; i < 48; ++i)
        buf[i] = 'a' + (i % 5);
    __m512i v = _mm512_maskz_loadu_epi8(6148914691236517205, buf);
    for (i = 0; i < 48; ++i) {
        char c = ((char*)&v)[i];
        if (i & 1)
            ZASSERT(!c);
        else
            ZASSERT(c == 'a' + (i % 5));
    }
    return 0;
}
