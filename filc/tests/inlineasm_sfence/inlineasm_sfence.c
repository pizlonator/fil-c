#include <stdfil.h>

int main(void)
{
    asm volatile("sfence" ::: "memory");
    ZASSERT(1);
    return 0;
}
