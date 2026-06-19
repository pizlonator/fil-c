#include <stdfil.h>

int main(void)
{
    /* STUI (Set User Interrupt Flag) sets the UIF flag, which enables user
       interrupts to be delivered on the following instruction boundary. This
       is a system-level side effect (it changes asynchronous control-flow /
       interrupt delivery behavior) rather than merely modifying registers, so
       it is not safe for inline assembly. It also requires the UINTR CPUID
       feature, which is not present on this computer. */
    asm volatile("stui");
    return 0;
}
