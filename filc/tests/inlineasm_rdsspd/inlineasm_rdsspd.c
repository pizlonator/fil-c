#include <stdfil.h>

int main(void)
{
    unsigned ssp;
    asm volatile("rdsspd %0" : "=r"(ssp));
    zprintf("rdsspd = 0x%x\n", ssp);
    return 0;
}
