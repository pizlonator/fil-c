#include <stdfil.h>

int main(void)
{
    unsigned short sw;
    asm volatile("fninit\n\tfstsw %0" : "=a"(sw) : : "fpsr");
    ZASSERT(sw == 0);
    return 0;
}
