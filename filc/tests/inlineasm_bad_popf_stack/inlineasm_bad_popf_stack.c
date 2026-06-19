int main(void)
{
    /* POPF pops flags from the stack (memory at SS:RSP) into EFLAGS.
       Unsafe: accesses memory (the stack), modifies the stack pointer (RSP)
       and flags. FilPizlonator must reject it. */
    asm volatile("popf" ::: "flags");
    return 0;
}
