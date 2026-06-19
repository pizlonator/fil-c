#include <stdfil.h>

typedef unsigned long long v2du __attribute__((vector_size(16)));

__attribute__((target("avx"))) int main(void)
{
    /* SSE2 form: orpd %1, %0  ->  %0 = %0 | %1 */
    v2du a = { 0xF0F0F0F0F0F0F0F0ULL, 0xAAAAAAAAAAAAAAAAULL };
    v2du b = { 0x0F0F0F0F0F0F0F0FULL, 0x4444444444444444ULL };

    asm volatile("orpd %1, %0" : "+x"(a) : "x"(b));

    ZASSERT(a[0] == 0xFFFFFFFFFFFFFFFFULL);
    ZASSERT(a[1] == 0xEEEEEEEEEEEEEEEEULL);

    /* AVX form: vorpd %2, %1, %0  ->  %0 = %1 | %2 */
    v2du c = { 0xF0F0F0F0F0F0F0F0ULL, 0xAAAAAAAAAAAAAAAAULL };
    v2du d = { 0x0F0F0F0F0F0F0F0FULL, 0x4444444444444444ULL };
    v2du result;

    asm volatile("vorpd %2, %1, %0" : "=x"(result) : "x"(c), "x"(d));

    ZASSERT(result[0] == 0xFFFFFFFFFFFFFFFFULL);
    ZASSERT(result[1] == 0xEEEEEEEEEEEEEEEEULL);

    return 0;
}
