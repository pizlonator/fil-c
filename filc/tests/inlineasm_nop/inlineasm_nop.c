#include <stdfil.h>

int main(void)
{
    unsigned long x;

    /* Bare single-byte NOP. */
    asm volatile("nop");
    ZASSERT(1);

    /* Multi-byte (hint) NOPs with a literal-register memory operand. The memory
       operand is purely syntactic: these compute the effective address but
       perform no load or store, exactly like lea, so they are safe. The size
       suffix is required by the assembler for the memory-operand form. */
    asm volatile("nopw (%rax)");
    asm volatile("nopl (%rax)");
    asm volatile("nopq (%rax)");
    asm volatile("nopl 0(%rax)");
    asm volatile("nopq 0(%rax)");
    asm volatile("nopl (%rax, %rbx, 8)");

    /* Multi-byte NOP with a placeholder input register in the addressing
       expression. No memory access occurs, so the input is left untouched. */
    x = 42;
    asm volatile("nopq (%0)" :: "r"(x));
    ZASSERT(x == 42);

    return 0;
}
