#include <stdfil.h>
#include <xmmintrin.h>

int main(void)
{
    float src = 5.0f;
    __m128 dst = _mm_setr_ps(1.0f, 2.0f, 3.0f, 4.0f);
    unsigned long p = (unsigned long)&src;

    asm volatile("insertps $0x10, (%1), %0" : "+x"(dst) : "r"(p));

    return 0;
}
