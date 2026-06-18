#include <stdfil.h>

int main(void)
{
    unsigned long src = 0;
    unsigned long dst = 0;
    asm volatile("movsq" : "+S"(src), "+D"(dst));
    return 0;
}
