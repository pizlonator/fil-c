/* Unnamed byval arguments must check pointer-word alignment too. */
#include <stdarg.h>
#include <stdio.h>

union Wide { void *pointer; unsigned long words[3]; };
const void *misalign(const void *pointer);

__attribute__((noinline)) unsigned long unnamed(int tag, ...)
{
    va_list args;
    va_start(args, tag);
    union Wide value = va_arg(args, union Wide);
    va_end(args);
    return value.words[1];
}

int main(void)
{
    int target = 42;
    void *slots[4] = { &target, &target, &target, &target };
    // Keep the bad address unknown to this translation unit.
    const void *misaligned = misalign(slots);
    unsigned long result = unnamed(17, *(const union Wide *)misaligned);
    printf("nono %lu\n", result);
    return 0;
}
