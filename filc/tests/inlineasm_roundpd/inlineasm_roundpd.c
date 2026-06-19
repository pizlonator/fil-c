#include <stdfil.h>
#include <emmintrin.h>

__attribute__((target("sse4.1"))) int main(void)
{
    __m128d a = _mm_setr_pd(1.4, 2.6);
    __m128d result;
    asm volatile("roundpd $0, %1, %0" : "=x"(result) : "x"(a));
    double r[2];
    _mm_storeu_pd(r, result);
    ZASSERT(r[0] == 1.0);
    ZASSERT(r[1] == 3.0);
    return 0;
}
