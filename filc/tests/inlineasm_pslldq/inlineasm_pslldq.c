#include <stdfil.h>

typedef int v4si __attribute__((vector_size(16)));

int main(void)
{
    /* PSLLDQ (SSE2): shift the 128-bit XMM register left by imm8 bytes,
       filling the vacated low-order bytes with zeros. Only the immediate
       count form exists. */

    /* Shift left by 4 bytes (= one int element). {1,2,3,4} -> {0,1,2,3}. */
    {
        v4si a = { 1, 2, 3, 4 };
        asm volatile("pslldq $4, %0" : "+x"(a));
        ZASSERT(a[0] == 0);
        ZASSERT(a[1] == 1);
        ZASSERT(a[2] == 2);
        ZASSERT(a[3] == 3);
    }

    /* Shift left by 16 bytes: everything shifts out, result is all zeros. */
    {
        v4si a = { 1, 2, 3, 4 };
        asm volatile("pslldq $16, %0" : "+x"(a));
        ZASSERT(a[0] == 0);
        ZASSERT(a[1] == 0);
        ZASSERT(a[2] == 0);
        ZASSERT(a[3] == 0);
    }

    return 0;
}
