#include <stdfil.h>

/* TESTUI requires User Interrupts (UINTR) support
 * (CPUID.07H.0H:EDX.UINTR[bit 5]), which is not present on this
 * machine, so the instruction is not supported in safe inline asm. */
int main(void)
{
    asm volatile("testui");
    return 0;
}
