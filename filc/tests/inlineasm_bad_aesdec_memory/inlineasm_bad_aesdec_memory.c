#include <stdfil.h>

typedef unsigned char v16qi __attribute__((vector_size(16)));

int main(void)
{
    v16qi a = {0x10, 0x20};
    unsigned long p = 0;
    asm volatile("aesdec (%1), %0" : "+x"(a) : "r"(p));
    return 0;
}
