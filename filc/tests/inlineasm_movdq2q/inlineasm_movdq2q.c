#include <stdfil.h>
#include <mmintrin.h>
#include <emmintrin.h>

int main(void)
{
    __m128i a = _mm_set_epi16(8, 7, 6, 5, 4, 3, 2, 1);
    __m64 b;

    asm volatile("movdq2q %1, %0" : "=y"(b) : "x"(a));

    ZASSERT(_mm_extract_pi16(b, 0) == 1);
    ZASSERT(_mm_extract_pi16(b, 1) == 2);
    ZASSERT(_mm_extract_pi16(b, 2) == 3);
    ZASSERT(_mm_extract_pi16(b, 3) == 4);

    return 0;
}
