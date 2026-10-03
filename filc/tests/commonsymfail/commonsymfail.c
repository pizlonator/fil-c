/* A common array keeps its own bounds. */
#include <stdfil.h>

__attribute__((common)) char name[32];
__attribute__((common)) char after[32];

int main()
{
    volatile int index = 40;
    name[index] = 1;
    return 0;
}
