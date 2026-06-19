#include <stdfil.h>
#include <xmmintrin.h>

__attribute__((target("sse4.1"))) int main(void)
{
    __m128 a = _mm_setr_ps(1.4f, 2.6f, 3.5f, 4.5f);
    __m128 result;
    asm volatile("roundps $0, %1, %0" : "=x"(result) : "x"(a));
    float r[4];
    _mm_storeu_ps(r, result);
    ZASSERT(r[0] == 1.0f);
    ZASSERT(r[1] == 3.0f);
    ZASSERT(r[2] == 4.0f);
    ZASSERT(r[3] == 4.0f);
    return 0;
}
