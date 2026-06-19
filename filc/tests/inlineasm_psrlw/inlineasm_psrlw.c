#include <stdfil.h>

typedef short v8hi __attribute__((vector_size(16)));

int main(void)
{
    /* PSRLW (SSE2): shift each packed word right logically by a count
       (zero-filling). The count can be an XMM register (count taken from
       bits [63:0]) or an 8-bit immediate. */

    /* Register count form: count register low 64 bits = 4. */
    {
        v8hi a = { 16, 256, 32, 512, 64, 1024, 128, 2048 };
        v8hi cnt = { 4, 0, 0, 0, 0, 0, 0, 0 };
        asm volatile("psrlw %1, %0" : "+x"(a) : "x"(cnt));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == 16);
        ZASSERT(a[2] == 2);
        ZASSERT(a[3] == 32);
        ZASSERT(a[4] == 4);
        ZASSERT(a[5] == 64);
        ZASSERT(a[6] == 8);
        ZASSERT(a[7] == 128);
    }

    /* Immediate count form: shift by 4. */
    {
        v8hi a = { 16, 256, 32, 512, 64, 1024, 128, 2048 };
        asm volatile("psrlw $4, %0" : "+x"(a));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == 16);
        ZASSERT(a[2] == 2);
        ZASSERT(a[3] == 32);
        ZASSERT(a[4] == 4);
        ZASSERT(a[5] == 64);
        ZASSERT(a[6] == 8);
        ZASSERT(a[7] == 128);
    }

    return 0;
}
