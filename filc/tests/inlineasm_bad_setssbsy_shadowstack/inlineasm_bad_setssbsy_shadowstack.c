#include <stdfil.h>

/* SETSSBSY is a CET shadow stack instruction that implicitly accesses the
 * shadow stack (via the IA32_PL0_SSP MSR) and is supervisor-only (CPL=0).
 * It is not safe for inline assembly. */
int main(void)
{
    asm volatile("setssbsy");
    return 0;
}
