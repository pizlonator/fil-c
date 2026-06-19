#include <stdfil.h>

int main(void)
{
    /* MWAIT is a privileged power-management instruction that hints the
       processor to enter an optimized state. It has system-level side
       effects (power management) and requires CPL 0, so it is not safe
       for inline assembly. */
    asm volatile("mwait" : : "a"(0), "c"(0));
    return 0;
}
