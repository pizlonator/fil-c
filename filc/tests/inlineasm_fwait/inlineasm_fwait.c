#include <stdfil.h>

int main(void)
{
    asm volatile("fwait");
    ZASSERT(1);
    return 0;
}
