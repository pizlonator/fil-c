#include <stdfil.h>

/* SAVEPREVSSP is a CET shadow stack instruction that implicitly pushes/pops the
 * shadow stack (implicit memory access). It is not safe for inline assembly. */
int main(void)
{
    asm volatile("saveprevssp");
    return 0;
}
