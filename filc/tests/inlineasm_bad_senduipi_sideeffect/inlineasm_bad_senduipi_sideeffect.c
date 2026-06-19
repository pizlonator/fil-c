#include <stdfil.h>

int main(void)
{
    /* SENDUIPI sends the user interprocessor interrupt (IPI) indicated by its
       register operand. It is not safe for inline asm: it implicitly accesses
       memory (reads the user-interrupt target table entry at UITTADDR, then
       reads and writes the user posted-interrupt descriptor at UPIDADDR, all
       performed with supervisor privilege) and, when a notification is owed,
       sends an ordinary IPI to another logical processor -- a side effect
       beyond modifying registers or raising uncatchable signals. FilPizlonator
       must reject it. */
    long x = 0;
    asm volatile("senduipi %0" : : "r"(x));
    return 0;
}
