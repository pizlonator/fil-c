#include <stdfil.h>

typedef long long v2di __attribute__((vector_size(16)));

int main(void)
{
    /* PSRLQ (SSE2): shift each packed quadword right logically by a count
       (zero-filling). The count can be an XMM register (count taken from
       bits [63:0]) or an 8-bit immediate. */

    /* Register count form: count register low 64 bits = 4. */
    {
        v2di a = { 16, 256 };
        v2di cnt = { 4, 0 };
        asm volatile("psrlq %1, %0" : "+x"(a) : "x"(cnt));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == 16);
    }

    /* Immediate count form: shift by 4. */
    {
        v2di a = { 16, 256 };
        asm volatile("psrlq $4, %0" : "+x"(a));
        ZASSERT(a[0] == 1);
        ZASSERT(a[1] == 16);
    }

    return 0;
}
