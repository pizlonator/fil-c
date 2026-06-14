#include <stdfil.h>

typedef float v4sf __attribute__((vector_size(16)));

int main(void)
{
    v4sf a = {1.0f, 2.0f};
    asm volatile("addsubps %%xmm1, %%xmm0" :: "x"(a));
    return 0;
}
