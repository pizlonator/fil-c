#include <stdfil.h>

int foo(int x)
{
    int y = x + 5;
    asm volatile("   \t\n\r" : "+r,m"(y), "+r,m"(x) : : "memory");
    return y + 6;
}

int main()
{
    int result = foo(666);
    ZASSERT(result == 666 + 5 + 6);
    return 0;
}

