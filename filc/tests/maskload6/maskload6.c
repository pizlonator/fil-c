#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdfil.h>
#include "utils.h"

__attribute__((target("avx512bw"))) int main()
{
    char* buf = opaque(malloc(16));
    unsigned i;
    for (i = 0; i < 16; ++i)
        buf[i] = 'a' + (i % 5);
    __m512i v = _mm512_maskz_loadu_epi8(65535lu << 48lu, buf - 48);
    for (i = 0; i < 64; ++i) {
        char c = ((char*)&v)[i];
        if (i >= 48) {
            ZASSERT(c == 'a' + ((i - 48) % 5));
        } else
            ZASSERT(!c);
    }
    return 0;
}
