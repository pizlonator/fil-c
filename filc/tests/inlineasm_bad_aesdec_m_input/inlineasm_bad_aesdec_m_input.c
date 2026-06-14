#include <stdfil.h>

typedef unsigned char v16qi __attribute__((vector_size(16)));

int main(void)
{
    v16qi a = {0x10, 0x20};
    unsigned char b = 0x20;
    asm volatile("aesdec %1, %0" : "+x"(a) : "m"(b));
    return 0;
}
