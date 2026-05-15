#include <stdfil.h>

__attribute__((weak)) void foo(void)
{
    zerror("Should not be reached");
}

int main()
{
    foo();
    return 0;
}
