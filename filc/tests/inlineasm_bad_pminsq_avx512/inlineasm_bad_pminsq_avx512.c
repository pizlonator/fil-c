#include <stdfil.h>

typedef long long v2di __attribute__((vector_size(16)));

int main(void)
{
    v2di a = { -5, 3 };
    v2di b;
    /* PMINSQ is only available as an AVX512 (EVEX) instruction, which we do
       not support in inline assembly. FilPizlonator rejects it. */
    asm volatile("pminsq %1, %0" : "=x"(b) : "x"(a));
    return 0;
}
