#include <stdfil.h>

int main(void)
{
    asm volatile("fnop");
    ZASSERT(1);
    return 0;
}
