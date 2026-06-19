#include <stdfil.h>

typedef int v4si __attribute__((vector_size(16)));

int main(void)
{
    /* PSRLDQ (SSE2): shift the 128-bit XMM register right by imm8 bytes,
       filling the vacated high-order bytes with zeros. Only the immediate
       count form exists. */

    /* Shift right by 4 bytes (= one int element). {1,2,3,4} -> {2,3,4,0}. */
    {
        v4si a = { 1, 2, 3, 4 };
        asm volatile("psrldq $4, %0" : "+x"(a));
        ZASSERT(a[0] == 2);
        ZASSERT(a[1] == 3);
        ZASSERT(a[2] == 4);
        ZASSERT(a[3] == 0);
    }

    /* Shift right by 16 bytes: everything shifts out, result is all zeros. */
    {
        v4si a = { 1, 2, 3, 4 };
        asm volatile("psrldq $16, %0" : "+x"(a));
        ZASSERT(a[0] == 0);
        ZASSERT(a[1] == 0);
        ZASSERT(a[2] == 0);
        ZASSERT(a[3] == 0);
    }

    return 0;
}
