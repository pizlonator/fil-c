#include <stdlib.h>

/* An allocating callee: gives the GC a reason to run while the caller's frame
   slot holds a pointer with a capability. */
long churn(long *p)
{
    long sum = 0;
    for (int i = 0; i < 8; i++) {
        void *q = malloc(128);
        if (!q)
            return -1;
        sum += (long)(q != 0);
        free(q);
    }
    return sum + (p != 0);
}
