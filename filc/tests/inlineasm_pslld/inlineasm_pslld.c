#include <stdfil.h>

typedef int v4si __attribute__((vector_size(16)));

int main(void)
{
    /* PSLLD (SSE2): shift each packed dword left by a count. The count operand
       can be an XMM register (count taken from bits [63:0]) or an 8-bit
       immediate. */

    /* Register count form: count register low 64 bits = 4. */
    {
        v4si a = { 1, 2, 3, 4 };
        v4si cnt = { 4, 0, 0, 0 };
        asm volatile("pslld %1, %0" : "+x"(a) : "x"(cnt));
        ZASSERT(a[0] == 16);
        ZASSERT(a[1] == 32);
        ZASSERT(a[2] == 48);
        ZASSERT(a[3] == 64);
    }

    /* Immediate count form: shift by 4. */
    {
        v4si a = { 1, 2, 3, 4 };
        asm volatile("pslld $4, %0" : "+x"(a));
        ZASSERT(a[0] == 16);
        ZASSERT(a[1] == 32);
        ZASSERT(a[2] == 48);
        ZASSERT(a[3] == 64);
    }

    return 0;
}
