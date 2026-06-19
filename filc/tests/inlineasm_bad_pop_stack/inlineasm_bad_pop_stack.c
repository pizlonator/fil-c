int main(void)
{
    /* POP pops a value from the stack (memory at SS:RSP) into a register,
       memory location, or segment register. Unsafe: accesses memory (the
       stack), modifies the stack pointer (RSP). FilPizlonator must reject
       it. */
    asm volatile("popq %%rax" ::: "rax");
    return 0;
}
