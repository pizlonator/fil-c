#include <stdfil.h>

int main(void)
{
    unsigned lo1, hi1, lo2, hi2;
    asm volatile("rdtsc" : "=a"(lo1), "=d"(hi1));
    asm volatile("rdtsc" : "=a"(lo2), "=d"(hi2));

    unsigned long long tsc1 = ((unsigned long long)hi1 << 32) | lo1;
    unsigned long long tsc2 = ((unsigned long long)hi2 << 32) | lo2;

    zprintf("tsc1 = %llu, tsc2 = %llu\n", tsc1, tsc2);
    ZASSERT(tsc2 >= tsc1);

    return 0;
}
