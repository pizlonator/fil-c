#include <stdfil.h>

int main(void)
{
    unsigned long x;

    x = 9;
    asm volatile("incq %0"
                 : "+r"(x)
                 :
                 : "cc");
    ZASSERT(x == 10);

    x = -1ul;
    asm volatile("incq %0"
                 : "+r"(x)
                 :
                 : "cc");
    ZASSERT(x == 0);

    unsigned int y;

    y = 9;
    asm volatile("incl %0"
                 : "+r"(y)
                 :
                 : "cc");
    ZASSERT(y == 10);

    y = 0xffffffffu;
    asm volatile("incl %0"
                 : "+r"(y)
                 :
                 : "cc");
    ZASSERT(y == 0);

    unsigned short z;

    z = 9;
    asm volatile("incw %0"
                 : "+r"(z)
                 :
                 : "cc");
    ZASSERT(z == 10);

    z = 0xffff;
    asm volatile("incw %0"
                 : "+r"(z)
                 :
                 : "cc");
    ZASSERT(z == 0);

    unsigned char w;

    w = 9;
    asm volatile("incb %0"
                 : "+r"(w)
                 :
                 : "cc");
    ZASSERT(w == 10);

    w = 0xff;
    asm volatile("incb %0"
                 : "+r"(w)
                 :
                 : "cc");
    ZASSERT(w == 0);

    return 0;
}
