#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#define ASSERT(exp) do { \
        if ((exp)) \
            break; \
        fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, \
                #exp); \
        abort(); \
    } while (false)

__attribute__((noinline)) void* allocate(void)
{
    return malloc(32);
}

__attribute__((target("avx512bw"))) int main()
{
    char* buf = allocate();
    unsigned i;
    for (i = 0; i < 32; ++i)
        buf[i] = 'a' + (i % 5);
    __m512i v;
    for (i = 0; i < 32; ++i)
        ((uint16_t*)&v)[i] = i * 666;
    _mm512_mask_cvtepi16_storeu_epi8(buf, 1431655765, v);
    for (i = 0; i < 32; ++i) {
        uint8_t c = buf[i];
        if (i & 1)
            ASSERT(c == (uint8_t)('a' + (i % 5)));
        else
            ASSERT(c == (uint8_t)(i * 666));
    }
    return 0;
}
