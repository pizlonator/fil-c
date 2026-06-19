#include <stdfil.h>

int main(void)
{
    unsigned long ssp;
    asm volatile("rdsspq %0" : "=r"(ssp));
    zprintf("rdsspq = 0x%lx\n", ssp);
    return 0;
}
