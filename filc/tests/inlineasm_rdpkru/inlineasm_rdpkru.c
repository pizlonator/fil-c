#include <stdfil.h>

int main(void)
{
    unsigned val1, val2;
    asm volatile("rdpkru" : "=a"(val1) : "c"(0) : "rdx");
    asm volatile("rdpkru" : "=a"(val2) : "c"(0) : "rdx");

    ZASSERT(val1 == val2);
    zprintf("pkru = 0x%x\n", val1);

    return 0;
}
