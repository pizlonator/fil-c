#include <stdfil.h>

typedef double v2df __attribute__((vector_size(16)));

int main(void)
{
    v2df a = {1.0, 2.0};
    double b = 2.0;
    asm volatile("addpd %1, %0" : "+x"(a) : "m"(b));
    return 0;
}
