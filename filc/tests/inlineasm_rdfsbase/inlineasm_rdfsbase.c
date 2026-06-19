#include <stdfil.h>

int main(void)
{
    unsigned long fs1, fs2;

    asm volatile("rdfsbase %0" : "=r"(fs1));
    asm volatile("rdfsbase %0" : "=r"(fs2));

    /* Reading the FS base twice must yield the same value. The FS base holds
       the thread pointer (TLS), so it is non-zero in a normal process. */
    ZASSERT(fs1 == fs2);
    ZASSERT(fs1 != 0);

    return 0;
}
