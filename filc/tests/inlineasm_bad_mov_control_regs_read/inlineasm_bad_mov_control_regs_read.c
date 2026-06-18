int main(void)
{
    unsigned long x;
    asm volatile("mov %%cr3, %0" : "=r"(x));
    return 0;
}
