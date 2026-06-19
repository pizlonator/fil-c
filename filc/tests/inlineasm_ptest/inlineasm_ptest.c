#include <stdfil.h>

typedef unsigned long long v2di __attribute__((vector_size(16)));

int main(void)
{
    /* PTEST sets ZF if (SRC AND DEST) == 0 and CF if (SRC AND NOT DEST) == 0.
       The destination (first operand) is NOT modified. Both operands are
       read-only inputs, and the instruction sets EFLAGS (hence the "cc"
       clobber). This test verifies that the operands remain unchanged. */

    /* Case 1: SRC AND DEST is all zeros (b is zero) -> ZF should be set. */
    {
        v2di a = { 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL };
        v2di b = { 0x0ULL, 0x0ULL };
        asm volatile("ptest %1, %0"
                     :: "x"(a), "x"(b)
                     : "cc");
        ZASSERT(a[0] == 0xFFFFFFFFFFFFFFFFULL);
        ZASSERT(a[1] == 0xFFFFFFFFFFFFFFFFULL);
        ZASSERT(b[0] == 0x0ULL);
        ZASSERT(b[1] == 0x0ULL);
    }

    /* Case 2: SRC AND DEST is nonzero. Operands still unmodified. */
    {
        v2di a = { 0xFF00FF00FF00FF00ULL, 0x00FF00FF00FF00FFULL };
        v2di b = { 0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL };
        asm volatile("ptest %1, %0"
                     :: "x"(a), "x"(b)
                     : "cc");
        ZASSERT(a[0] == 0xFF00FF00FF00FF00ULL);
        ZASSERT(a[1] == 0x00FF00FF00FF00FFULL);
        ZASSERT(b[0] == 0x0F0F0F0F0F0F0F0FULL);
        ZASSERT(b[1] == 0xF0F0F0F0F0F0F0F0ULL);
    }

    /* Case 3: Identical operands -> SRC AND NOT DEST is zero -> CF set. */
    {
        v2di a = { 0x123456789ABCDEF0ULL, 0x0F0F0F0F0F0F0F0FULL };
        v2di b = { 0x123456789ABCDEF0ULL, 0x0F0F0F0F0F0F0F0FULL };
        asm volatile("ptest %1, %0"
                     :: "x"(a), "x"(b)
                     : "cc");
        ZASSERT(a[0] == 0x123456789ABCDEF0ULL);
        ZASSERT(a[1] == 0x0F0F0F0F0F0F0F0FULL);
        ZASSERT(b[0] == 0x123456789ABCDEF0ULL);
        ZASSERT(b[1] == 0x0F0F0F0F0F0F0F0FULL);
    }

    return 0;
}
