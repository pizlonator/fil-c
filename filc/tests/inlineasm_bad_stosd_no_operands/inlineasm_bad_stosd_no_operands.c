int main(void)
{
    asm volatile("stosl" : : : "rdi", "rax", "flags");
    return 0;
}
