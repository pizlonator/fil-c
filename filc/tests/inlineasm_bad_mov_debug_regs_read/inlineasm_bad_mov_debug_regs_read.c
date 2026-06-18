int main(void)
{
    unsigned long x;
    asm volatile("mov %%db0, %0" : "=r"(x));
    return 0;
}
