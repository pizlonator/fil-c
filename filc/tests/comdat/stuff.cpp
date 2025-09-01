#include <stdfil.h>

void* stuff()
{
    zprintf("In stuff():\n");
    zdump_stack();
    void* result = __builtin_return_address(0);
    asm volatile ("" : : : "memory");
    return result;
}
