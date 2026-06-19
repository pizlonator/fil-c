int main(void)
{
    asm volatile("scasb" : : : "rax", "rdi", "flags");
    return 0;
}
