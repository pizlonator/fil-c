#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

extern void storeheap(long *obj, long magic);
extern long *loadheap(long **slot);

void fill32(long *p)
{
    for (int i = 0; i < 4; i++) p[i] = 100 + i;
}

int main(void)
{
    long **slot = malloc(sizeof(long *));
    if (!slot)
        return 1;
    storeheap((long *)slot, 4111);
    /* GC churn: a region kept alive only by the heap object's aux must stay
       intact across it. */
    for (int i = 0; i < 100000; i++) {
        volatile long *q = malloc(64);
        if (!q)
            return 1;
        *q = (long)i;
    }
    zgc_request_and_wait();
    /* The LOAD happens in asm (`#! load ptr`), reconstructing the capability
       from the heap object's aux; C then dereferences the pointer. */
    long *p = loadheap(slot);
    if (p[0] != 4111 || p[1] != 4222 || p[2] != 102 || p[3] != 103 ||
        p[5] != 8538) {
        printf("FAIL: %ld %ld %ld %ld %ld\n", p[0], p[1], p[2], p[3], p[5]);
        return 1;
    }
    printf("frame escape storeheap att ok\n");
    return 0;
}
