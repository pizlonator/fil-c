/* A valid union type is not proof that a malicious C cast points at aligned
 * storage. The named byval path must trap before unchecked shadow access. */
#include <stdio.h>

union Wide { void *pointer; unsigned long words[3]; };
const void *misalign(const void *pointer);

__attribute__((noinline)) unsigned long named(union Wide value)
{
    return value.words[1];
}

int main(void)
{
    int target = 42;
    void *slots[4] = { &target, &target, &target, &target };
    // A separate translation unit keeps the bad address unknown at the call
    // site, so this exercises the actual runtime alignment check.
    const void *misaligned = misalign(slots);
    unsigned long result = named(*(const union Wide *)misaligned);
    printf("nono %lu\n", result);
    return 0;
}
