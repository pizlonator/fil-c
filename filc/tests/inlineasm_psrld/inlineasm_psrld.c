#include <stdfil.h>

typedef int v4si __attribute__((vector_size(16)));

int main(void)
{
    /* PSRLD (SSE2): shift each packed doubleword right logically by a count
       (zero-filling). The count can be an XMM register (count taken from
       bits [63:0]) or an 8-bit immediate. */

    /* Register count form: count register low 64 bits = 4. */
    {
        v4si a = { 16, 256, 32, 4096 };
        v4si cnt = { 4, 0, 0, 0 };
        asm volatile("psrld %1, %0" : "+x"(a) : "x"(cnt));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == 16);
        ZASSERT(a[2] == 2);
        ZASSERT(a[3] == 256);
    }

    /* Immediate count form: shift by 4. */
    {
        v4si a = { 16, 256, 32, 4096 };
        asm volatile("psrld $4, %0" : "+x"(a));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == 16);
        ZASSERT(a[2] == 2);
        ZASSERT(a[3] == 256);
    }

    return 0;
}
