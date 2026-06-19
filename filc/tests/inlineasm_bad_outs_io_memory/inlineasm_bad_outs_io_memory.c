int main(void)
{
    /* OUTS reads from memory at [RSI] and writes to the I/O port specified by
       DX. It is unsafe: it performs a memory load and I/O. FilPizlonator must
       reject it. */
    asm volatile("outsb" : : : "rsi", "rdx", "flags");
    return 0;
}
