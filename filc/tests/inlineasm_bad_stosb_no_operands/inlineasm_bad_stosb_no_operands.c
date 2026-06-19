int main(void)
{
    asm volatile("stosb" : : : "rdi", "rax", "flags");
    return 0;
}
