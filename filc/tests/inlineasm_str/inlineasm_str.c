#include <stdfil.h>

int main(void)
{
    unsigned long t1, t2;
    asm volatile("str %0" : "=r"(t1));
    asm volatile("str %0" : "=r"(t2));

    /* STR stores the TR (task register) selector into a general-purpose
       register. The selector (low 16 bits) must be deterministic across two
       reads. The high bits may be non-zero when the kernel emulates the
       instruction under CR4.UMIP. */
    ZASSERT((t1 & 0xffff) == (t2 & 0xffff));

    return 0;
}
