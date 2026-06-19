#include <stdfil.h>

int main(void)
{
    unsigned long c1, c2;
    asm volatile("smsw %0" : "=r"(c1));
    asm volatile("smsw %0" : "=r"(c2));

    /* SMSW stores CR0 (the machine status word). In long mode paging is always
       enabled, so the PE (bit 0) and PG (bit 31) bits are set. The value must
       be deterministic across two reads. */
    ZASSERT((c1 & 0xffffffff) == (c2 & 0xffffffff));
    ZASSERT(c1 & 1);
    ZASSERT(c1 & (1UL << 31));

    return 0;
}
