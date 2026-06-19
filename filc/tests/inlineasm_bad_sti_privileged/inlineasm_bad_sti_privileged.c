#include <stdfil.h>

int main(void)
{
    /* STI (Set Interrupt Flag) is a privileged instruction that enables
       maskable hardware interrupts by setting IF in EFLAGS. In user mode
       (CPL=3 with IOPL<3) it generates #GP. Like CLI, it manipulates
       system-level interrupt state and is not safe for inline assembly. */
    asm volatile("sti");
    return 0;
}
