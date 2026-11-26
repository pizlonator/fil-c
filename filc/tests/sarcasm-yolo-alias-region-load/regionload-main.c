#include <stdio.h>

extern long regionload(long a, void *b, void *c, void *d, long e, void *f,
                       long g);

int main(void)
{
    long x1 = 1, x2 = 2, x3 = 3, x4 = 4;
    /* The asm function stores the first argument into its frame and reads it
       back through the second (region-pointer) alias; it returns the stored
       value. Pass real pointers for the pointer arguments and a distinctive
       value for the integer ones so a stale or mis-keyed read shows up. */
    long r = regionload(424242, &x1, &x2, &x3, 0, &x4, 0);
    if (r != 424242) {
        printf("FAIL: got %ld, want 424242\n", r);
        return 1;
    }
    printf("yolo alias region load ok\n");
    return 0;
}
