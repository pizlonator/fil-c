int main(void)
{
    /* POPFQ pops flags from the stack (memory at SS:RSP) into RFLAGS.
       Unsafe: accesses memory (the stack), modifies the stack pointer (RSP)
       and flags. FilPizlonator must reject it. */
    asm volatile("popfq" ::: "flags");
    return 0;
}
