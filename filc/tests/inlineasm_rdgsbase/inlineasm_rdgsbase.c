#include <stdfil.h>

int main(void)
{
    unsigned long gs1, gs2;

    asm volatile("rdgsbase %0" : "=r"(gs1));
    asm volatile("rdgsbase %0" : "=r"(gs2));

    /* Reading the GS base twice must yield the same value. */
    ZASSERT(gs1 == gs2);

    return 0;
}
