#include <stdfil.h>
#include <mmintrin.h>
#include <emmintrin.h>

int main(void)
{
    __m64 a = _mm_set_pi16(4, 3, 2, 1);
    __m128i b;

    asm volatile("movq2dq %1, %0" : "=x"(b) : "y"(a));

    ZASSERT(_mm_extract_epi16(b, 0) == 1);
    ZASSERT(_mm_extract_epi16(b, 1) == 2);
    ZASSERT(_mm_extract_epi16(b, 2) == 3);
    ZASSERT(_mm_extract_epi16(b, 3) == 4);
    ZASSERT(_mm_extract_epi16(b, 4) == 0);
    ZASSERT(_mm_extract_epi16(b, 5) == 0);
    ZASSERT(_mm_extract_epi16(b, 6) == 0);
    ZASSERT(_mm_extract_epi16(b, 7) == 0);

    return 0;
}
