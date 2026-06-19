#include <stdfil.h>
#include <xmmintrin.h>

__attribute__((target("sse4.1"))) int main(void)
{
    __m128 a = _mm_setr_ps(0.0f, 7.0f, 8.0f, 9.0f);
    __m128 b = _mm_setr_ps(2.6f, 0.0f, 0.0f, 0.0f);
    asm volatile("roundss $0, %1, %0" : "+x"(a) : "x"(b));
    float r[4];
    _mm_storeu_ps(r, a);
    ZASSERT(r[0] == 3.0f);
    ZASSERT(r[1] == 7.0f);
    ZASSERT(r[2] == 8.0f);
    ZASSERT(r[3] == 9.0f);
    return 0;
}
