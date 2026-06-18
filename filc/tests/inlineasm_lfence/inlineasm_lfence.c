#include <stdfil.h>

int main(void)
{
    asm volatile("lfence" ::: "memory");
    ZASSERT(1);
    return 0;
}
