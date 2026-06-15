#include <stdfil.h>

int main(void)
{
    unsigned x = 666;

    /* The %2 in LLVM IR is aliasing %0, so it's technically OK.
       
       It would be valid to change this test's expectation to success. But that's sketchy. A bit
       better to say it's a fail. */
    asm volatile("add %0, %2" : "+r"(x) : "r"(x) : "cc");

    ZASSERT(x == 666 * 2);

    return 0;
}
