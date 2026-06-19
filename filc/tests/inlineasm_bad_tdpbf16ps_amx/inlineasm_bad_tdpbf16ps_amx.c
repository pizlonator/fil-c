#include <stdfil.h>

int main(void)
{
    asm volatile("tdpbf16ps %%tmm1, %%tmm2, %%tmm3");
    return 0;
}
