int main(void)
{
    asm volatile("movsd" : : : "rsi", "rdi", "flags");
    return 0;
}
