/* A one-word union uses direct pointer coercion, not byval. That path must
 * also reject a misaligned source rather than trusting its declared type. */
#include <stdio.h>

union Small { void *pointer; unsigned long bits; };
const void *misalign(const void *pointer);

__attribute__((noinline)) unsigned long direct(union Small value)
{
    return value.bits;
}

int main(void)
{
    int target = 42;
    void *slots[2] = { &target, &target };
    const void *misaligned = misalign(slots);
    unsigned long result = direct(*(const union Small *)misaligned);
    printf("nono %lu\n", result);
    return 0;
}
