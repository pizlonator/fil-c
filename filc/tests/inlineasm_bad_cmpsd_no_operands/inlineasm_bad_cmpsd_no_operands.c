int main(void)
{
    asm volatile("cmpsd" : : : "rsi", "rdi", "flags");
    return 0;
}
