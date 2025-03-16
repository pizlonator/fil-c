#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ASSERT(exp) do { \
        if ((exp)) \
            break; \
        fprintf(stderr, "%s:%d: %s: assertion %s failed.\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, \
                #exp); \
        abort(); \
    } while (false)

__attribute__((target("avx512bw"))) int main()
{
    char buf[16];
    unsigned i;
    for (i = 0; i < 16; ++i)
        buf[i] = 'a' + (i % 5);
    __m512i v = _mm512_maskz_loadu_epi8(65535, buf);
    for (i = 0; i < 64; ++i) {
        char c = ((char*)&v)[i];
        if (i < 16) {
            ASSERT(c == 'a' + (i % 5));
        } else
            ASSERT(!c);
    }
    return 0;
}
