#include <stdio.h>
#include <stdlib.h>

void align64_spill(void* touch_dst, void* sum_dst);

int main()
{
    /* 16-byte out buffers (Fil-C rounds all allocations to 16 bytes). */
    unsigned long long* touch_dst = malloc(16);
    unsigned long long* sum_dst = malloc(16);
    if (!touch_dst || !sum_dst)
        return 1;
    *touch_dst = 0;
    *sum_dst = 0;
    align64_spill(touch_dst, sum_dst);
    /* touch() must have stored 1 through the first pointer. */
    if (*touch_dst != 1) {
        printf("framealign-spill BAD touch: %llu\n", *touch_dst);
        return 1;
    }
    /* 11+22+33+44 must survive the 64-byte-aligned scratch stores and the call. */
    if (*sum_dst != 11 + 22 + 33 + 44) {
        printf("framealign-spill BAD sum: %llu\n", *sum_dst);
        return 1;
    }
    printf("framealign-spill ok\n");
    return 0;
}
