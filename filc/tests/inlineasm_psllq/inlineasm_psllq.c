#include <stdfil.h>

typedef long long v2di __attribute__((vector_size(16)));

int main(void)
{
    /* PSLLQ (SSE2): shift each packed quadword left by a count. The count
       can be an XMM register (count taken from bits [63:0]) or an 8-bit
       immediate. */

    /* Register count form: count register low 64 bits = 4. */
    {
        v2di a = { 1, 2 };
        v2di cnt = { 4, 0 };
        asm volatile("psllq %1, %0" : "+x"(a) : "x"(cnt));
        ZASSERT(a[0] == 16);
        ZASSERT(a[1] == 32);
    }

    /* Immediate count form: shift by 4. */
    {
        v2di a = { 1, 2 };
        asm volatile("psllq $4, %0" : "+x"(a));
        ZASSERT(a[0] == 16);
        ZASSERT(a[1] == 32);
    }

    return 0;
}
