#include <stdfil.h>

typedef short v8hi __attribute__((vector_size(16)));

int main(void)
{
    /* PSRAW (SSE2): shift each packed word right arithmetically by a count
       (sign-extending). The count can be an XMM register (count taken from
       bits [63:0]) or an 8-bit immediate. */

    /* Register count form: count register low 64 bits = 4. */
    {
        v8hi a = { 16, -16, 32, -32, 48, -48, 64, -64 };
        v8hi cnt = { 4, 0, 0, 0, 0, 0, 0, 0 };
        asm volatile("psraw %1, %0" : "+x"(a) : "x"(cnt));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == -1);
        ZASSERT(a[2] == 2);
        ZASSERT(a[3] == -2);
        ZASSERT(a[4] == 3);
        ZASSERT(a[5] == -3);
        ZASSERT(a[6] == 4);
        ZASSERT(a[7] == -4);
    }

    /* Immediate count form: shift by 4. */
    {
        v8hi a = { 16, -16, 32, -32, 48, -48, 64, -64 };
        asm volatile("psraw $4, %0" : "+x"(a));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == -1);
        ZASSERT(a[2] == 2);
        ZASSERT(a[3] == -2);
        ZASSERT(a[4] == 3);
        ZASSERT(a[5] == -3);
        ZASSERT(a[6] == 4);
        ZASSERT(a[7] == -4);
    }

    return 0;
}
