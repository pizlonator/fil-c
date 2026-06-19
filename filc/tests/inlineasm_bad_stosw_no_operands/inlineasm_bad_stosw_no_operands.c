int main(void)
{
    asm volatile("stosw" : : : "rdi", "rax", "flags");
    return 0;
}
