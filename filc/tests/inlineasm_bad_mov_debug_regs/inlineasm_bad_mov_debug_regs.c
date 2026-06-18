int main(void)
{
    unsigned long x = 0;
    asm volatile("mov %0, %%db0" : : "r"(x));
    return 0;
}
