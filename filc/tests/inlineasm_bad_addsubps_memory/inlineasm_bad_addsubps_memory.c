#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

int main(void)
{
    v4sf a = {1.0f, 2.0f};
    unsigned long p = 0;
    asm volatile("addsubps (%1), %0" : "+x"(a) : "r"(p));
    return 0;
}
