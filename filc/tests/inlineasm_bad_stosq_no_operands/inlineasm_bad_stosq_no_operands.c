int main(void)
{
    asm volatile("stosq" : : : "rdi", "rax", "flags");
    return 0;
}
