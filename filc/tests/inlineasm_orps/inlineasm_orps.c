#include <stdfil.h>

typedef unsigned int v4su __attribute__((vector_size(16)));

__attribute__((target("avx"))) int main(void)
{
    /* SSE form: orps %1, %0  ->  %0 = %0 | %1 */
    v4su a = { 0xF0F0F0F0u, 0xAAAAAAAAu, 0x12345678u, 0x0000FFFFu };
    v4su b = { 0x0F0F0F0Fu, 0x44444444u, 0x87654321u, 0xFFFF0000u };

    asm volatile("orps %1, %0" : "+x"(a) : "x"(b));

    ZASSERT(a[0] == 0xFFFFFFFFu);
    ZASSERT(a[1] == 0xEEEEEEEEu);
    ZASSERT(a[2] == 0x97755779u);
    ZASSERT(a[3] == 0xFFFFFFFFu);

    /* AVX form: vorps %2, %1, %0  ->  %0 = %1 | %2 */
    v4su c = { 0xF0F0F0F0u, 0xAAAAAAAAu, 0x12345678u, 0x0000FFFFu };
    v4su d = { 0x0F0F0F0Fu, 0x44444444u, 0x87654321u, 0xFFFF0000u };
    v4su result;

    asm volatile("vorps %2, %1, %0" : "=x"(result) : "x"(c), "x"(d));

    ZASSERT(result[0] == 0xFFFFFFFFu);
    ZASSERT(result[1] == 0xEEEEEEEEu);
    ZASSERT(result[2] == 0x97755779u);
    ZASSERT(result[3] == 0xFFFFFFFFu);

    return 0;
}
