#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));

int main(void)
{
    v2df a = {1.0, 2.0};
    unsigned long p = 0;
    asm volatile("addsubpd (%1), %0" : "+x"(a) : "r"(p));
    return 0;
}
