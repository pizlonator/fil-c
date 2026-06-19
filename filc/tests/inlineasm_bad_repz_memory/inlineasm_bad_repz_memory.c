#include <stdfil.h>

/* REPZ is a repeat prefix (synonym for REPE) for the SCAS/CMPS string
 * instructions, which access memory implicitly via RSI/RDI. There is no
 * memory-free safe form, so Fil-C must reject it. */
int main(void)
{
    char buf[16] = {0};
    unsigned long ptr = (unsigned long)buf;
    unsigned long count = 16;
    char val = 1;
    asm volatile("repz scasb" : "+D"(ptr), "+c"(count) : "a"(val) : "cc");
    zprintf("count = %lu\n", count);
    return 0;
}
