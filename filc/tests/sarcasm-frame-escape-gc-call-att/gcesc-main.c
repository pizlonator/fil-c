#include <stdio.h>
#include <stdlib.h>
#include <stdfil.h>

extern long gcesc(long arg);
long gc_helper(long *p);

/* Called with a derived pointer into the asm's promoted frame region. Reads
   through it, forces a GC while the asm's derived pointers and frame slots
   are live, then writes through it — the asm must observe the write. */
long gc_helper(long *p)
{
    long v = p[0];              /* the asm's slot16, read through the derived pointer */
    for (int i = 0; i < 1000; i++) {
        volatile long *q = malloc(64);
        if (!q)
            exit(1);
        *q = (long)i;
    }
    zgc_request_and_wait();     /* force GC at a safepoint below the asm frame */
    p[0] = 20881;               /* write a new value through the pointer, post-GC */
    return v;
}

int main(void)
{
    /* 4111 (helper return) + 20881 (post-GC write) + 4222 + 4222 + 5444 +
       5444 + 4333 = 48657. */
    long r = gcesc(4111);
    if (r != 48657) {
        printf("FAIL: got %ld, want 48657\n", r);
        return 1;
    }
    printf("frame escape gc call att ok\n");
    return 0;
}
