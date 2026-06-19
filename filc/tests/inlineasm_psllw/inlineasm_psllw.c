#include <stdfil.h>

typedef short v8hi __attribute__((vector_size(16)));

int main(void)
{
    /* PSLLW (SSE2): shift each packed word left by a count. The count
       can be an XMM register (count taken from bits [63:0]) or an 8-bit
       immediate. */

    /* Register count form: count register low 64 bits = 4. */
    {
        v8hi a = { 1, 2, 3, 4, 5, 6, 7, 8 };
        v8hi cnt = { 4, 0, 0, 0, 0, 0, 0, 0 };
        asm volatile("psllw %1, %0" : "+x"(a) : "x"(cnt));
        ZASSERT(a[0] == 16);
        ZASSERT(a[1] == 32);
        ZASSERT(a[2] == 48);
        ZASSERT(a[3] == 64);
        ZASSERT(a[4] == 80);
        ZASSERT(a[5] == 96);
        ZASSERT(a[6] == 112);
        ZASSERT(a[7] == 128);
    }

    /* Immediate count form: shift by 4. */
    {
        v8hi a = { 1, 2, 3, 4, 5, 6, 7, 8 };
        asm volatile("psllw $4, %0" : "+x"(a));
        ZASSERT(a[0] == 16);
        ZASSERT(a[1] == 32);
        ZASSERT(a[2] == 48);
        ZASSERT(a[3] == 64);
        ZASSERT(a[4] == 80);
        ZASSERT(a[5] == 96);
        ZASSERT(a[6] == 112);
        ZASSERT(a[7] == 128);
    }

    return 0;
}
