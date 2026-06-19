#include <stdfil.h>

typedef unsigned long long v2udi __attribute__((vector_size(16)));

int main(void)
{
    v2udi a = { 5, 3 };
    v2udi b;
    /* PMINUQ is only available as an AVX512 (EVEX) instruction, which we do
       not support in inline assembly. FilPizlonator rejects it. */
    asm volatile("pminuq %1, %0" : "=x"(b) : "x"(a));
    return 0;
}
