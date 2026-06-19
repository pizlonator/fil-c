#include <stdfil.h>

int main(void)
{
    unsigned long a1, a2;
    asm volatile("sldt %0" : "=r"(a1));
    asm volatile("sldt %0" : "=r"(a2));

    /* SLDT stores the LDTR selector into a general-purpose register. The
       selector (low 16 bits) must be deterministic across two reads. The high
       bits may be non-zero when the kernel emulates the instruction under
       CR4.UMIP. */
    ZASSERT((a1 & 0xffff) == (a2 & 0xffff));

    return 0;
}
