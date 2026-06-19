#include <stdfil.h>

int main(void)
{
    /* PTWRITE reads a value from a register and sends it to the Intel
       Processor Trace (PT) hardware to be encoded as a PTW packet (if the
       various PT enable bits are set). It is not safe for inline asm: it has
       a side effect (injecting a packet into the PT output stream) beyond
       modifying registers or raising uncatchable signals, and it performs an
       implicit memory write to the hardware/OS-managed PT output buffer.
       FilPizlonator must reject it. */
    int x = 42;
    asm volatile("ptwrite %0" : : "r"(x));
    return 0;
}
