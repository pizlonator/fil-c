#include <stdfil.h>

int main(void)
{
    unsigned lo, hi, cpu;
    asm volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(cpu));

    unsigned long long tsc = ((unsigned long long)hi << 32) | lo;
    zprintf("tsc = %llu, cpu = %u\n", tsc, cpu);
    ZASSERT(tsc != 0);

    return 0;
}
