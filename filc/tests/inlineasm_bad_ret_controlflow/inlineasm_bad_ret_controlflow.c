int main(void)
{
    /* RET transfers program control to a return address popped from the top
       of the stack. Unsafe: it is a control flow instruction (return) and
       accesses memory (the stack). FilPizlonator must reject it. */
    asm volatile("ret");
    return 0;
}
