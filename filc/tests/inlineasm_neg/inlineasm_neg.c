#include <stdfil.h>

int main(void)
{
    unsigned long x;

    x = 9;
    asm volatile("negq %0"
                 : "+r"(x)
                 :
                 : "cc");
    ZASSERT(x == (unsigned long)-9);

    x = 0;
    asm volatile("negq %0"
                 : "+r"(x)
                 :
                 : "cc");
    ZASSERT(x == 0);

    x = (unsigned long)-9;
    asm volatile("negq %0"
                 : "+r"(x)
                 :
                 : "cc");
    ZASSERT(x == 9);

    unsigned int y;

    y = 9;
    asm volatile("negl %0"
                 : "+r"(y)
                 :
                 : "cc");
    ZASSERT(y == (unsigned int)-9);

    y = 0;
    asm volatile("negl %0"
                 : "+r"(y)
                 :
                 : "cc");
    ZASSERT(y == 0);

    unsigned short z;

    z = 9;
    asm volatile("negw %0"
                 : "+r"(z)
                 :
                 : "cc");
    ZASSERT(z == (unsigned short)-9);

    z = 0;
    asm volatile("negw %0"
                 : "+r"(z)
                 :
                 : "cc");
    ZASSERT(z == 0);

    unsigned char w;

    w = 9;
    asm volatile("negb %0"
                 : "+r"(w)
                 :
                 : "cc");
    ZASSERT(w == (unsigned char)-9);

    w = 0;
    asm volatile("negb %0"
                 : "+r"(w)
                 :
                 : "cc");
    ZASSERT(w == 0);

    return 0;
}
