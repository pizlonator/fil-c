#include <stdfil.h>

int main(void)
{
    unsigned long val1 = 0, val2 = 0;
    asm volatile("rdrand %0" : "=r"(val1) : : "cc");
    asm volatile("rdrand %0" : "=r"(val2) : : "cc");

    zprintf("rdrand1 = %lu, rdrand2 = %lu\n", val1, val2);

    return 0;
}
