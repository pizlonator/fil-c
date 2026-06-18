#include <stdfil.h>

int main(void)
{
    asm volatile("mfence" ::: "memory");
    ZASSERT(1);
    return 0;
}
