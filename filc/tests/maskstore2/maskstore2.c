#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdfil.h>
#include "utils.h"

__attribute__((target("avx512bw"))) int main()
{
    __m512i v;
    unsigned i;
    for (i = 0; i < 64; ++i)
        ((char*)&v)[i] = 'M' + (i % 5);
    _mm512_mask_storeu_epi8(
        opaque("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd"),
        6148914691236517205, v);
    return 0;
}
