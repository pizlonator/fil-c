#include <stdfil.h>
#include <emmintrin.h>

__attribute__((target("sse4.1"))) int main(void)
{
    __m128d a = _mm_setr_pd(0.0, 99.0);
    __m128d b = _mm_setr_pd(2.6, 0.0);
    asm volatile("roundsd $0, %1, %0" : "+x"(a) : "x"(b));
    double r[2];
    _mm_storeu_pd(r, a);
    ZASSERT(r[0] == 3.0);
    ZASSERT(r[1] == 99.0);
    return 0;
}
