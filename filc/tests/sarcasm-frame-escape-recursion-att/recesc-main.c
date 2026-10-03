#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

extern long recesc(long depth, long *ctx);
long rec_c(long depth, long *region);

/* The C mirror of the asm's XOR chain: each activation XORs its own depth and
   its slot24 magic (3000+depth) into the inner chain. */
static long mirror(long d)
{
    long inner = (d > 0) ? mirror(d - 1) : 0;
    return inner ^ d ^ (3000 + d);
}

long rec_c(long depth, long *region)
{
    long inner = 0;
    for (int i = 0; i < 200; i++) {
        volatile long *q = malloc(64);
        if (!q)
            exit(1);
        *q = (long)i;
    }
    /* The CURRENT activation's region, read from C through the passed derived
       pointer. A shared (not per-activation) region would corrupt the magics. */
    if (region[0] != depth || region[1] != 3000 + depth) {
        printf("FAIL: rec_c depth %ld sees %ld %ld\n", depth, region[0], region[1]);
        exit(1);
    }
    if (depth > 0) {
        if ((depth & 3) == 0)
            zgc_request_and_wait();
        inner = recesc(depth - 1, region);
    }
    return inner;
}

int main(void)
{
    long obj[4] = {0, 0, 0, 0};
    long want = mirror(16);
    long r = recesc(16, obj);
    if (r != want) {
        printf("FAIL: got %ld, want %ld\n", r, want);
        return 1;
    }
    printf("frame escape recursion att ok\n");
    return 0;
}
