#include <stdfil.h>

int main(void)
{
    /* imul requires 1, 2, or 3 operands. */
    asm volatile("imul" ::: "cc");
    return 0;
}
