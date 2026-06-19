#include <stdfil.h>

/* REP is a repeat prefix for string instructions (MOVS, STOS, LODS, INS,
 * OUTS). Every REP-prefixed instruction accesses memory implicitly via
 * RSI/RDI, so there is no memory-free safe form. Fil-C must reject it. */
int main(void)
{
    char buf[16];
    unsigned long ptr = (unsigned long)buf;
    unsigned long count = 16;
    char val = 0;
    asm volatile("rep stosb" : "+D"(ptr), "+c"(count) : "a"(val));
    zprintf("count = %lu\n", count);
    return 0;
}
