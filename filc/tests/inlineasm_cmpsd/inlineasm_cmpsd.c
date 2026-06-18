#include <stdfil.h>
#include <emmintrin.h>
#include <string.h>
#include <stdint.h>

int main(void)
{
    __m128d a = _mm_setr_pd(1.0, 2.0);
    __m128d b = _mm_setr_pd(1.0, 5.0);

    asm volatile("cmpsd $0, %1, %0" : "+x"(a) : "x"(b));

    double rf[2];
    uint64_t bits[2];
    _mm_storeu_pd(rf, a);
    memcpy(bits, rf, sizeof(bits));

    ZASSERT(bits[0] == ~0ULL);
    ZASSERT(bits[1] == 0x4000000000000000ULL);

    return 0;
}
